/*
 * Camera Preview Sketch
 * 
 * Sends one QCIF (176x144) RGB565 frame to Serial when requested.
 * 
 * Protocol:
 *   - Wait for a single 'C' character on Serial
 *   - On receipt, capture a frame and dump it as hex (one 16-bit pixel per line)
 *   - End with a line containing "END"
 *   - Then go back to waiting
 */

#include <Arduino_OV767X.h>

#define IMG_WIDTH  176
#define IMG_HEIGHT 144

byte imageData[IMG_WIDTH * IMG_HEIGHT * 2];

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  if (!Camera.begin(QCIF, RGB565, 5)) {
    Serial.println("ERROR: Camera init failed");
    while (1);
  }
  
  Serial.println("READY");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == 'C') {
      sendFrame();
    }
  }
}

void sendFrame() {
  Camera.readFrame(imageData);
  
  Serial.println("FRAME_START");
  Serial.print("WIDTH=");
  Serial.println(IMG_WIDTH);
  Serial.print("HEIGHT=");
  Serial.println(IMG_HEIGHT);
  
  for (int i = 0; i < IMG_WIDTH * IMG_HEIGHT; i++) {
    uint16_t pixel = (imageData[i * 2] << 8) | imageData[i * 2 + 1];
    Serial.println(pixel, HEX);
  }
  
  Serial.println("FRAME_END");
}