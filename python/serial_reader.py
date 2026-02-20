"""
OmniVision – Serial Reader
Reads JSON sensor data streamed by the ESP32 over a serial (USB) connection
and prints it to the console.  Extend this module to push data to a database,
display it in a UI, or forward it to another service.

Usage:
    python serial_reader.py [--port PORT] [--baud BAUD]

Examples:
    python serial_reader.py                         # uses defaults below
    python serial_reader.py --port /dev/ttyUSB0 --baud 115200
    python serial_reader.py --port COM3 --baud 115200
"""

import argparse
import json
import sys
import time

import serial


# ── Default configuration ────────────────────────────────────────────────────
DEFAULT_PORT = "/dev/ttyUSB0"   # Change to your port (e.g. COM3 on Windows)
DEFAULT_BAUD = 115200
READ_TIMEOUT = 2.0              # seconds before a read times out


# ── Helpers ───────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Read JSON sensor data from an ESP32 over Serial."
    )
    parser.add_argument(
        "--port", default=DEFAULT_PORT,
        help=f"Serial port (default: {DEFAULT_PORT})"
    )
    parser.add_argument(
        "--baud", type=int, default=DEFAULT_BAUD,
        help=f"Baud rate (default: {DEFAULT_BAUD})"
    )
    return parser.parse_args()


def open_serial(port: str, baud: int) -> serial.Serial:
    """Open the serial port, retrying until it becomes available."""
    while True:
        try:
            ser = serial.Serial(port, baud, timeout=READ_TIMEOUT)
            print(f"[INFO] Connected to {port} at {baud} baud.")
            return ser
        except serial.SerialException as exc:
            print(f"[WARN] Cannot open {port}: {exc}. Retrying in 3 s…")
            time.sleep(3)


def handle_line(raw_line: str) -> None:
    """Parse a single line received from the ESP32 and act on it."""
    line = raw_line.strip()
    if not line:
        return

    try:
        data = json.loads(line)
        # Pretty-print and process the received sensor reading
        print(f"[DATA] {json.dumps(data, indent=2)}")

        # Example: extract individual fields
        if "sensor" in data:
            sensor_name = data.get("sensor")
            temp        = data.get("temp")
            humidity    = data.get("humidity")
            print(
                f"       Sensor={sensor_name}  "
                f"Temp={temp:.2f} °C  "
                f"Humidity={humidity:.2f} %"
            )
    except json.JSONDecodeError:
        # Not every line will be JSON (e.g. debug messages) – log as-is
        print(f"[RAW]  {line}")


# ── Main loop ─────────────────────────────────────────────────────────────────

def main() -> None:
    args = parse_args()
    ser = open_serial(args.port, args.baud)

    print("[INFO] Listening for sensor data (Ctrl+C to stop)…\n")
    try:
        while True:
            try:
                raw = ser.readline()
                if raw:
                    handle_line(raw.decode("utf-8", errors="replace"))
            except serial.SerialException as exc:
                print(f"[ERROR] Serial error: {exc}. Attempting to reconnect…")
                ser.close()
                ser = open_serial(args.port, args.baud)
    except KeyboardInterrupt:
        print("\n[INFO] Stopped by user.")
    finally:
        if ser.is_open:
            ser.close()
        print("[INFO] Serial port closed.")
        sys.exit(0)


if __name__ == "__main__":
    main()
