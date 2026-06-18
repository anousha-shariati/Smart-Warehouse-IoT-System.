"""
Camera Preview — IKEA Fire Detection Project

Captures frames from the Arduino Nano 33 BLE Sense camera and displays them.

SETUP:
1. Upload camera_preview_sketch.ino to the Nano
2. Close the Arduino Serial Monitor
3. Update SERIAL_PORT below
4. pip install pyserial pillow numpy matplotlib
5. python camera_preview.py
"""

import serial
import time
import os
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt

# ==== Configuration ====
SERIAL_PORT = 'COM3'      # Windows: COM3/4/5 etc. (check Device Manager)
                          # Mac:     '/dev/cu.usbmodem...' — run `ls /dev/cu.*`
                          # Linux:   '/dev/ttyACM0' usually
BAUD_RATE = 115200
IMG_WIDTH = 176
IMG_HEIGHT = 144


# ==== Connect to Nano ====
def connect():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5)
    time.sleep(2)  # Let the Nano reset
    ser.reset_input_buffer()
    
    print('Connecting...')
    start = time.time()
    while time.time() - start < 3:
        line = ser.readline().decode(errors='ignore').strip()
        if line == 'READY':
            print('Nano is ready!')
            return ser
        elif line:
            print(f'  Nano: {line}')
    
    print('No READY message — that may be OK if the Nano was already running.')
    return ser


# ==== Capture one frame ====
def capture_frame(ser):
    """Request one frame from the Nano. Returns numpy array (H, W, 3) uint8."""
    ser.reset_input_buffer()
    ser.write(b'C')
    
    # Wait for FRAME_START
    start = time.time()
    while time.time() - start < 10:
        line = ser.readline().decode(errors='ignore').strip()
        if line == 'FRAME_START':
            break
    else:
        raise TimeoutError('No FRAME_START received')
    
    # Read width and height
    width_line = ser.readline().decode().strip()
    height_line = ser.readline().decode().strip()
    width = int(width_line.split('=')[1])
    height = int(height_line.split('=')[1])
    
    # Read pixel data
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
        print(f'Warning: got {len(pixels)} pixels, expected {width*height}')
    
    # Convert RGB565 → RGB888
    pixels_array = np.array(pixels[:width * height], dtype=np.uint16)
    r = ((pixels_array >> 11) & 0x1F) << 3
    g = ((pixels_array >> 5)  & 0x3F) << 2
    b = ( pixels_array        & 0x1F) << 3
    
    rgb = np.stack([r, g, b], axis=-1).astype(np.uint8)
    return rgb.reshape((height, width, 3))


# ==== Show full + 32x32 grayscale side-by-side ====
def show_frame(image, save_path=None):
    """Display the full image and the 32x32 grayscale view."""
    pil_small = Image.fromarray(image).convert('L').resize((32, 32), Image.BILINEAR)
    small_array = np.array(pil_small)
    
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    axes[0].imshow(image)
    axes[0].set_title(f'Full ({IMG_WIDTH}x{IMG_HEIGHT} RGB) — brightness {image.mean():.0f}')
    axes[0].axis('off')
    
    axes[1].imshow(small_array, cmap='gray', vmin=0, vmax=255)
    axes[1].set_title(f'Model input (32x32 gray) — brightness {small_array.mean():.0f}')
    axes[1].axis('off')
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=100, bbox_inches='tight')
        print(f'Saved {save_path}')
    
    plt.show()


# ==== Live loop ====
def live_capture(ser, delay=1.0):
    """Keep capturing frames until you close the window or press Ctrl+C."""
    print('Live capture started. Press Ctrl+C in the terminal to stop.')
    plt.ion()  # Interactive mode
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    
    try:
        frame_count = 0
        while True:
            image = capture_frame(ser)
            pil_small = Image.fromarray(image).convert('L').resize((32, 32), Image.BILINEAR)
            small_array = np.array(pil_small)
            
            axes[0].clear()
            axes[0].imshow(image)
            axes[0].set_title(f'Frame #{frame_count} — brightness {image.mean():.0f}')
            axes[0].axis('off')
            
            axes[1].clear()
            axes[1].imshow(small_array, cmap='gray', vmin=0, vmax=255)
            axes[1].set_title(f'32x32 gray — brightness {small_array.mean():.0f}')
            axes[1].axis('off')
            
            plt.pause(delay)
            frame_count += 1
    except KeyboardInterrupt:
        print(f'\nStopped after {frame_count} frames.')
    finally:
        plt.ioff()


# ==== Main menu ====
def main():
    ser = connect()
    
    try:
        while True:
            print('\n=== Camera Preview ===')
            print('  1 - Capture single frame')
            print('  2 - Live capture loop')
            print('  3 - Save reference image')
            print('  q - Quit')
            choice = input('Choice: ').strip().lower()
            
            if choice == '1':
                image = capture_frame(ser)
                show_frame(image)
            elif choice == '2':
                live_capture(ser, delay=1.0)
            elif choice == '3':
                label = input('Label (e.g. "demo_safe", "candles_test"): ').strip() or 'reference'
                os.makedirs('reference_images', exist_ok=True)
                image = capture_frame(ser)
                
                # Save the actual image (not the matplotlib figure)
                filename = f'reference_images/{label}_{int(time.time())}.png'
                Image.fromarray(image).save(filename)
                print(f'Saved {filename}')
                show_frame(image)
            elif choice == 'q':
                break
    finally:
        ser.close()
        print('Serial port closed.')


if __name__ == '__main__':
    main()