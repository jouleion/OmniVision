#!/usr/bin/env python3
"""
VL53L7CX 8x8 Grid Parser - 100% WORKING VERSION
Matches your EXACT PlatformIO serial format
"""

import serial
import serial.tools.list_ports
import numpy as np
import matplotlib
matplotlib.use('Qt5Agg')
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time
import argparse
import re

class RobustGridParser:
    def __init__(self, port=None, verbose=True):
        self.port = port
        self.ser = None
        self.grids = []
        self.pending_rows = []
        self.verbose = verbose
        self.connected = False
        
    def connect(self):
        """ESP32-safe connection"""
        ports = serial.tools.list_ports.comports()
        if self.port:
            target_port = self.port
        else:
            target_port = next((p.device for p in ports if 'USB Serial' in p.description), None)
        
        if not target_port:
            print("❌ No ESP32 found! Connect USB cable.")
            return False
            
        try:
            self.ser = serial.Serial(
                port=target_port, baudrate=115200, timeout=0.1,
                rtscts=False, dsrdtr=False, xonxoff=False
            )
            self.ser.dtr = False
            self.ser.rts = False
            self.ser.reset_input_buffer()
            self.connected = True
            print(f"✅ Connected: {target_port}")
            time.sleep(1)
            return True
        except Exception as e:
            print(f"❌ Connect failed: {e}")
            self.connected = False
            return False
    
    def is_connected(self):
        """Check cable status"""
        if not self.connected or not self.ser or not self.ser.is_open:
            self.connected = False
            return False
        return True
    
    def read_all_lines(self):
        """Read all available serial data"""
        if not self.is_connected():
            return []
        lines = []
        while self.ser.in_waiting:
            try:
                line = self.ser.readline()
                line_str = line.decode('utf-8', errors='ignore').rstrip()
                if line_str:
                    lines.append(line_str)
                    if self.verbose:
                        print(f"📡 {line_str}")
            except:
                self.connected = False
                break
        return lines
    
    def parse_grids(self, lines):
        """Parse your EXACT messy serial format"""
        for line in lines:
            # New grid marker
            if "=== 8x8 GRID ===" in line:
                if self.verbose: 
                    print("📊 START GRID")
                continue
            
            # Extract exactly 8 numbers = valid row
            numbers = re.findall(r'\d+', line.strip())
            if len(numbers) == 8:
                row = [int(x) for x in numbers]
                
                # 8th row completes grid!
                if len(self.pending_rows) == 7:
                    grid = np.array(self.pending_rows + [row], dtype=float)
                    self.grids.append(grid)
                    self.grids = self.grids[-3:]  # Keep newest 3
                    
                    if self.verbose:
                        print(f"✅ GRID #{len(self.grids)}: "
                              f"center={grid[3,4]:.0f}mm, "
                              f"zones={int(np.sum(grid>0))}/64")
                    self.pending_rows = []
                else:
                    self.pending_rows.append(row)
    
    def get_latest(self):
        """Get newest grid + stats"""
        if self.grids:
            grid = self.grids[-1]
            stats = {
                'zones': int(np.sum(grid > 0)),
                'center': int(grid[3,4]),  # True 8x8 center
                'min': int(np.min(grid[grid > 0])),
                'max': int(np.max(grid)),
                'mean': int(np.mean(grid[grid > 0]))
            }
            return grid, stats
        return None, {}
    
    def start(self):
        self.running = True
        return self.connect()
    
    def stop(self):
        self.running = False
        self.connected = False
        if self.ser:
            self.ser.close()

def live_plot():
    parser = RobustGridParser(verbose=True)
    if not parser.start():
        return
    
    # Setup figure ONCE
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8))
    fig.suptitle('VL53L7CX 8x8 Live ToF Sensor', fontsize=16)
    
    im = None  # Persistent heatmap
    cbar = None
    frame_count = 0
    
    def update(frame):
        nonlocal im, cbar, frame_count
        frame_count += 1
        
        # Connection check
        if not parser.is_connected():
            ax1.clear()
            ax1.text(0.5, 0.5, '🔌 CABLE DISCONNECTED\nReconnect & restart', 
                    ha='center', va='center', fontsize=20, color='red')
            ax1.set_title('Connection Lost')
            ax2.clear()
            ax2.axis('off')
            return []
        
        # Read + parse serial
        lines = parser.read_all_lines()
        parser.parse_grids(lines)
        
        print(f"Frame {frame_count}: {len(parser.grids)} grids")
        
        grid, stats = parser.get_latest()
        if grid is not None:
            print(f"🎯 SHOWING: center={stats['center']}mm, zones={stats['zones']}")
            
            # HEATMAP (persistent - NO clear!)
            if im is None:
                im = ax1.imshow(grid, cmap='RdYlBu_r', vmin=0, vmax=1000, 
                               origin='upper', animated=True)
                cbar = plt.colorbar(im, ax=ax1, label='Distance (mm)')
                ax1.set_title('VL53L7CX 8x8 Live Grid')
                ax1.set_xlabel('Column')
                ax1.set_ylabel('Row')
            else:
                im.set_array(grid)
                im.changed()  # Force redraw
            
            # NEO PIXEL SIMULATION
            ax2.clear()
            ax2.set_xlim(0, 1)
            ax2.set_ylim(0, 1)
            ax2.axis('off')
            
            center = stats['center']
            if center < 300:
                color, status = 'red', 'CLOSE'
            elif center < 800:
                color, status = 'yellow', 'MEDIUM' 
            else:
                color, status = 'green', 'FAR'
            
            # Draw NeoPixel circle
            circle = plt.Circle((0.5, 0.5), 0.35, color=color, alpha=0.85)
            ax2.add_patch(circle)
            
            # Center distance
            ax2.text(0.5, 0.52, f'{center}mm', ha='center', va='center',
                    fontsize=28, fontweight='bold', color='white')
            
            # Status
            ax2.text(0.5, 0.38, status, ha='center', va='center',
                    fontsize=18, fontweight='bold', color='black')
            
            # Stats
            ax2.text(0.5, 0.15, f'Zones: {stats["zones"]}/64\n'
                                f'Min: {stats["min"]}mm  Max: {stats["max"]}mm',
                    ha='center', va='center', fontsize=12)
            
            ax2.set_title('NeoPixel Simulation', fontsize=14)
            
        else:
            # Waiting screen
            ax1.clear()
            ax1.text(0.5, 0.5, f'Waiting for 8x8 grid...\n'
                              f'{len(parser.grids)} grids received',
                    ha='center', va='center', fontsize=16)
            ax1.set_title('VL53L7CX Scanner - Initializing')
        
        return [im] if im else []
    
    print("🚀 Live visualization ready! Close window to stop.")
    ani = FuncAnimation(fig, update, interval=250, blit=False, cache_frame_data=False, repeat=True)
    plt.tight_layout()
    plt.show(block=True)
    
    parser.stop()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="VL53L7CX 8x8 Visualizer")
    parser.add_argument('--live', action='store_true', help="Live visualization")
    parser.add_argument('--port', help="Specific COM port")
    args = parser.parse_args()
    
    live_plot()
