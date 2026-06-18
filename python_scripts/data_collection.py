"""
Data Collection Script — Fire Detection with Sensor Fusion

Captures N image+temperature pairs and saves them organized for training.
1
Folder structure created:
  dataset/
    safe_environment/   sample_0001.png ... sample_0150.png
    fire/               sample_0001.png ... sample_0150.png
    temperatures.csv    (filename, class, temperature, humidity)

USAGE:
  1. Upload data_collection_sketch.ino to Nano. Close Serial Monitor.
  2. Edit CLASS_NAME below to either 'safe_environment' or 'fire'.
  3. Edit SERIAL_PORT.
  4. python collect_data.py
"""

import serial
import time
import os
import csv
import numpy as np
from PIL import Image

# ==== CONFIGURATION ====
SERIAL_PORT  = 'COM3'
BAUD_RATE    = 115200

# Change for each class:
CLASS_NAME   = 'fire'   # or 'fire'
NUM_SAMPLES  = 150
DELAY_BETWEEN_SAMPLES = 2.0  # seconds — gives you time to move the scene slightly

DATASET_DIR  = 'dataset'
IMG_WIDTH    = 176
IMG_HEIGHT   = 144

# ==== Setup folders & CSV ====
class_dir = os.path.join(DATASET_DIR, CLASS_NAME)
os.makedirs(class_dir, exist_ok=True)
csv_path = os.path.join(DATASET_DIR, 'temperatures.csv')

csv_exists = os.path.exists(csv_path)
csv_file = open(csv_path, 'a', newline='')
csv_writer = csv.writer(csv_file)
if not csv_exists:
    csv_writer.writerow(['filename', 'class', 'temperature', 'humidity'])

# ==== Capture function ====
def capture_one(ser):
    """Returns (image_array, temperature, humidity)"""
    ser.reset_input_buffer()
    ser.write(b'C')
    
    start = time.time()
    found = False
    while time.time() - start < 10:
        line = ser.readline().decode(errors='ignore').strip()
        if line == 'FRAME_START':
            found = True
            break
    if not found:
        raise TimeoutError('No FRAME_START received')
    
    temp   = float(ser.readline().decode().strip().split('=')[1])
    hum    = float(ser.readline().decode().strip().split('=')[1])
    width  = int(ser.readline().decode().strip().split('=')[1])
    height = int(ser.readline().decode().strip().split('=')[1])
    
    pixels = []
    while True:
        line = ser.readline().decode(errors='ignore').strip()
        if line == 'FRAME_END':
            break
        if not line:
            continue
        try:
            pixels.append(int(line, 16))
        except ValueError:
            pass
    
    if len(pixels) != width * height:
        print(f'  ! got {len(pixels)} pixels, expected {width*height}')
    
    arr = np.array(pixels[:width*height], dtype=np.uint16)
    r = ((arr >> 11) & 0x1F) << 3
    g = ((arr >> 5)  & 0x3F) << 2
    b = ( arr        & 0x1F) << 3
    rgb = np.stack([r, g, b], axis=-1).astype(np.uint8).reshape((height, width, 3))
    return rgb, temp, hum

# ==== Main loop ====
print(f'Class: {CLASS_NAME}')
print(f'Connecting to {SERIAL_PORT}...')
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10)
time.sleep(2)
ser.reset_input_buffer()

print('Waiting for Nano READY...')
start = time.time()
while time.time() - start < 5:
    line = ser.readline().decode(errors='ignore').strip()
    if line == 'READY':
        print('Nano ready!')
        break
    elif line:
        print(f'  Nano: {line}')

# Resume from where we left off if rerun
existing = len([f for f in os.listdir(class_dir) if f.endswith('.png')])
print(f'\nExisting samples for "{CLASS_NAME}": {existing}')
print(f'Collecting {NUM_SAMPLES} more samples.')
print('TIP: shift the camera slightly every ~10 frames for variation.\n')

try:
    for i in range(NUM_SAMPLES):
        n = existing + i + 1
        filename = f'sample_{n:04d}.png'
        filepath = os.path.join(class_dir, filename)
        
        try:
            img, temp, hum = capture_one(ser)
        except Exception as e:
            print(f'[{i+1}/{NUM_SAMPLES}] FAILED: {e}')
            continue
        
        Image.fromarray(img).save(filepath)
        csv_writer.writerow([filename, CLASS_NAME, f'{temp:.2f}', f'{hum:.2f}'])
        csv_file.flush()
        
        print(f'[{i+1}/{NUM_SAMPLES}] {filename}  T={temp:.1f}°C  H={hum:.1f}%  brightness={img.mean():.0f}')
        time.sleep(DELAY_BETWEEN_SAMPLES)

except KeyboardInterrupt:
    print('\nStopped by user.')

finally:
    csv_file.close()
    ser.close()
    print(f'\nDone! Saved to {class_dir}')
    print(f'Log: {csv_path}')