"""
BLE → Arduino Cloud bridge for IKEA Fire Detection.

Connects to the Nano via BLE, reads fire state + confidence + temp,
and pushes them to Arduino Cloud variables.
"""

import asyncio
import struct
import time
from bleak import BleakScanner, BleakClient
import requests

# ==== Configuration ====
NANO_NAME = 'IKEA-Fire-Node'
CLASS_UUID = '00002a19-0000-1000-8000-00805f9b34fb'   # classChar
CONF_UUID  = '00002a1a-0000-1000-8000-00805f9b34fb'   # confChar

# Arduino Cloud — fill these in from your Manual Device setup
DEVICE_ID  = 'b989036d-e705-47ad-8a03-b24e3dc8ef62'
SECRET_KEY = 'gnnj@gciRj0JiZQF57p#Pv!V2'
THING_ID   = '82da1409-b236-4a49-99d4-cefdac8a83c4'   # find in Thing settings

# State to push
state = {
    'fire_state': 'safe',
    'confidence': 0.0,
    'temperature': 22.0  # placeholder until we add temp to BLE
}

# ==== Arduino IoT Cloud OAuth token ====
def get_token():
    """Get OAuth bearer token for Arduino Cloud API."""
    response = requests.post(
        'https://api2.arduino.cc/iot/v1/clients/token',
        data={
            'grant_type': 'client_credentials',
            'client_id': DEVICE_ID,
            'client_secret': SECRET_KEY,
            'audience': 'https://api2.arduino.cc/iot'
        },
        headers={'content-type': 'application/x-www-form-urlencoded'}
    )
    response.raise_for_status()
    return response.json()['access_token']

token = None
last_token_refresh = 0

def push_to_cloud(variable_name, value):
    """Send a single variable update to Arduino Cloud."""
    global token, last_token_refresh
    
    # Refresh token every ~4 hours (they last 5)
    if time.time() - last_token_refresh > 4 * 3600:
        token = get_token()
        last_token_refresh = time.time()
    
    url = f'https://api2.arduino.cc/iot/v2/things/{THING_ID}/properties'
    headers = {
        'Authorization': f'Bearer {token}',
        'Content-Type': 'application/json'
    }
    payload = {
        'name': variable_name,
        'value': value
    }
    try:
        r = requests.put(url, json=payload, headers=headers, timeout=5)
        if r.status_code != 200:
            print(f'  ! Cloud update failed for {variable_name}: {r.status_code} {r.text}')
    except Exception as e:
        print(f'  ! Cloud error: {e}')

# ==== BLE callbacks ====
def on_class_change(sender, data):
    new_state = data.decode().strip()
    if new_state != state['fire_state']:
        state['fire_state'] = new_state
        print(f'→ fire_state changed: {new_state}')
        push_to_cloud('fire_state', new_state)

def on_conf_change(sender, data):
    new_conf = struct.unpack('<f', data)[0]
    state['confidence'] = new_conf
    # Don't spam cloud with tiny confidence changes
    push_to_cloud('confidence', round(new_conf, 3))

# ==== Main loop ====
async def main():
    global token, last_token_refresh
    
    # Get initial token
    print('Getting Arduino Cloud token...')
    token = get_token()
    last_token_refresh = time.time()
    print('OK')
    
    # Find the Nano
    print(f'Scanning for {NANO_NAME}...')
    device = await BleakScanner.find_device_by_name(NANO_NAME, timeout=20)
    if not device:
        print('!! Nano not found. Is it powered on and running the inference sketch?')
        return
    
    print(f'Found: {device.address}')
    
    async with BleakClient(device) as client:
        await client.start_notify(CLASS_UUID, on_class_change)
        await client.start_notify(CONF_UUID, on_conf_change)
        print('Connected. Forwarding updates to Arduino Cloud...')
        print('(Press Ctrl+C to stop)\n')
        
        # Keep alive
        while True:
            await asyncio.sleep(1)

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print('\nStopped.')