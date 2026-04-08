#!/usr/bin/env python3
"""
Simple serial-to-CSV logger for long VL53L7CX data streams.
Finds an ESP32-style COM port if none provided, reads every line
and appends to a CSV file. Buffers lines and flushes every N seconds.

Usage: python log_serial_to_csv.py --port COM15 --out data.csv --baud 115200 --flush 5
"""

import argparse
import csv
import time
import sys
import signal
import traceback
from datetime import datetime

import serial
import serial.tools.list_ports


def find_esp_port():
	ports = serial.tools.list_ports.comports()
	# Heuristics: look for common descriptors used by ESP32 boards
	for p in ports:
		desc = (p.description or "").lower()
		if any(k in desc for k in ("usb serial", "silicon labs", "cp210", "usb-to-serial", "ch340")):
			return p.device
	# fallback to first available port
	return ports[0].device if ports else None


class SerialCsvLogger:
	def __init__(self, port=None, baud=115200, out_file=None, flush_interval=5, timeout=0.2, verbose=True):
		self.port = port
		self.baud = baud
		self.out_file = out_file or f"serial_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
		self.flush_interval = float(flush_interval)
		self.timeout = timeout
		self.ser = None
		self.buffer = []
		self.running = False
		self.verbose = verbose

	def connect(self):
		try:
			target = self.port or find_esp_port()
			if not target:
				raise RuntimeError("No serial ports found. Plug in your device.")

			if self.verbose:
				print(f"Connecting to {target} @ {self.baud}...")

			self.ser = serial.Serial(port=target, baudrate=self.baud, timeout=self.timeout,
									 rtscts=False, dsrdtr=False, xonxoff=False)
			# Some boards toggle DTR on open; give a moment
			time.sleep(0.5)
			# Drain input
			try:
				self.ser.reset_input_buffer()
			except Exception:
				pass

			if self.verbose:
				print(f"Connected -> logging to {self.out_file}")
			return True
		except Exception as e:
			print(f"ERROR: Failed to open serial port: {e}")
			if self.verbose:
				traceback.print_exc()
			return False

	def close(self):
		try:
			if self.ser and self.ser.is_open:
				self.ser.close()
		except Exception:
			pass

	def write_buffer(self):
		if not self.buffer:
			return
		try:
			# append to CSV with timestamp and raw line
			write_header = False
			try:
				# check if file exists by opening in append and checking size
				with open(self.out_file, 'a', newline='') as _:
					pass
			except FileNotFoundError:
				write_header = True

			with open(self.out_file, 'a', newline='', encoding='utf-8') as f:
				writer = csv.writer(f)
				if write_header:
					writer.writerow(['timestamp_iso', 'timestamp_epoch', 'line'])
				for ts, line in self.buffer:
					writer.writerow([ts.isoformat(), f"{ts.timestamp():.3f}", line])
			if self.verbose:
				print(f"Flushed {len(self.buffer)} lines to {self.out_file}")
			self.buffer = []
		except Exception as e:
			print(f"ERROR: Failed writing to CSV: {e}")
			if self.verbose:
				traceback.print_exc()

	def run(self):
		if not self.connect():
			return

		self.running = True
		last_flush = time.time()

		# graceful shutdown on signals
		def _stop(signum, frame):
			print("\nSignal received, stopping...")
			self.running = False

		signal.signal(signal.SIGINT, _stop)
		signal.signal(signal.SIGTERM, _stop)

		try:
			while self.running:
				try:
					if not self.ser or not self.ser.is_open:
						print("Serial port closed unexpectedly. Attempting reconnect...")
						time.sleep(1)
						if not self.connect():
							time.sleep(2)
							continue

					raw = self.ser.readline()
					if not raw:
						# periodic flush check even if no data
						if time.time() - last_flush >= self.flush_interval:
							self.write_buffer()
							last_flush = time.time()
						continue

					try:
						line = raw.decode('utf-8', errors='replace').rstrip('\r\n')
					except Exception:
						line = str(raw)

					if line:
						ts = datetime.utcnow()
						self.buffer.append((ts, line))
						if self.verbose:
							print(f"{ts.isoformat()}  {line}")

					# flush if interval passed or buffer large
					if time.time() - last_flush >= self.flush_interval or len(self.buffer) >= 1000:
						self.write_buffer()
						last_flush = time.time()

				except serial.SerialException as se:
					print(f"Serial error: {se}. Reconnecting in 1s...")
					if self.verbose:
						traceback.print_exc()
					try:
						self.close()
					except Exception:
						pass
					time.sleep(1)
				except Exception as e:
					print(f"ERROR while reading serial: {e}")
					if self.verbose:
						traceback.print_exc()
					time.sleep(0.5)

		finally:
			# final flush
			try:
				self.write_buffer()
			except Exception:
				pass
			self.close()
			print("Stopped. Goodbye.")


def main():
	parser = argparse.ArgumentParser(description="Serial to CSV logger")
	parser.add_argument('--port', help='Serial port (e.g., COM3). If omitted, auto-detects.')
	parser.add_argument('--baud', type=int, default=115200, help='Baud rate')
	parser.add_argument('--out', help='Output CSV file path')
	parser.add_argument('--flush', type=float, default=5.0, help='Seconds between flushes')
	parser.add_argument('--timeout', type=float, default=0.2, help='Serial read timeout (s)')
	parser.add_argument('--quiet', action='store_true', help='Less verbose output')
	args = parser.parse_args()

	logger = SerialCsvLogger(port=args.port, baud=args.baud, out_file=args.out,
							 flush_interval=args.flush, timeout=args.timeout, verbose=not args.quiet)
	try:
		logger.run()
	except Exception as e:
		print(f"Fatal error: {e}")
		traceback.print_exc()


if __name__ == '__main__':
	main()
