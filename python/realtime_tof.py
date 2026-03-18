import serial
import re

PORT = "COM5"
BAUD = 115200

def read_live_grids(ser):
    sensor1 = []
    sensor2 = []
    current_sensor = None

    while True:
        line = ser.readline().decode(errors="ignore").strip()

        if not line:
            continue

        # print(line)

        if line.startswith("Sensor 1"):
            current_sensor = 1
            sensor1 = []

        elif line.startswith("Sensor 2"):
            current_sensor = 2
            sensor2 = []

        elif line.startswith("8x8"):
            continue

        else:
            numbers = re.findall(r"\d+", line)

            if not numbers:
                continue

            row = [int(x) for x in numbers]

            if current_sensor == 1:
                sensor1.append(row)

            elif current_sensor == 2:
                sensor2.append(row)

            if len(sensor1) == 8 and len(sensor2) == 8:
                combined = [r1 + r2 for r1, r2 in zip(sensor1, sensor2)]
                yield combined

                sensor1 = []
                sensor2 = []

def split_regions(grid):
    left = [row[:5] for row in grid]
    middle = [row[5:11] for row in grid]
    right = [row[11:16] for row in grid]
    return left, middle, right


def detect_close_object(region, threshold=1000, percentage=0.35):
    values = [v for row in region for v in row]
    total = len(values)
    close_count = sum(v < threshold for v in values)
    return close_count >= total * percentage

ser = serial.Serial(PORT, BAUD, timeout=1)

for t, grid in enumerate(read_live_grids(ser)):
    left, middle, right = split_regions(grid)

    print(f"\nReading {t}")

    if detect_close_object(left):
        print("LEFT: CLOSE OBJECT DETECTED")

    if detect_close_object(middle):
        print("MIDDLE: CLOSE OBJECT DETECTED")

    if detect_close_object(right):
        print("RIGHT: CLOSE OBJECT DETECTED")