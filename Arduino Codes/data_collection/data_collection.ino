/*
 * Data Collection Sketch — Camera + DHT20 Sensor Fusion
 * 
 * On Serial command 'C', captures one QCIF image and one DHT20 reading,
 * then sends both to the laptop in a parseable format.
 */

#include <Arduino_OV767X.h>
#include <Wire.h>

#define IMG_WIDTH      176
#define IMG_HEIGHT     144
#define DHT20_ADDRESS  0x38

byte imageData[IMG_WIDTH * IMG_HEIGHT * 2];

// ==== DHT20 helpers (proven working from earlier test) ====
void resetRegister(uint8_t reg) {
  Wire.beginTransmission(DHT20_ADDRESS);
  Wire.write(reg); Wire.write(0x00); Wire.write(0x00);
  Wire.endTransmission(); delay(5);
  Wire.requestFrom(DHT20_ADDRESS, 3);
  uint8_t b[3] = {0,0,0};
  for (int i=0; i<3 && Wire.available(); i++) b[i] = Wire.read();
  delay(10);
  Wire.beginTransmission(DHT20_ADDRESS);
  Wire.write(0xB0 | reg); Wire.write(b[1]); Wire.write(b[2]);
  Wire.endTransmission(); delay(5);
}

bool initDHT20() {
  delay(50);
  Wire.beginTransmission(DHT20_ADDRESS); Wire.write(0x71); Wire.endTransmission();
  delay(10);
  Wire.requestFrom(DHT20_ADDRESS, 1);
  uint8_t status = 0;
  if (Wire.available()) status = Wire.read();
  if ((status & 0x18) != 0x18) {
    resetRegister(0x1B); resetRegister(0x1C); resetRegister(0x1E);
    delay(10);
  }
  return true;
}

bool readDHT20(float* temperature, float* humidity) {
  Wire.beginTransmission(DHT20_ADDRESS);
  Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(100);
  uint8_t count = Wire.requestFrom(DHT20_ADDRESS, 7);
  if (count < 7) return false;
  uint8_t data[7];
  for (int i=0; i<7; i++) data[i] = Wire.read();
  if (data[0] & 0x80) return false;
  uint32_t rawH = ((uint32_t)data[1]<<12) | ((uint32_t)data[2]<<4) | ((uint32_t)data[3]>>4);
  *humidity = ((float)rawH / 1048576.0) * 100.0;
  uint32_t rawT = (((uint32_t)data[3] & 0x0F)<<16) | ((uint32_t)data[4]<<8) | (uint32_t)data[5];
  *temperature = (((float)rawT / 1048576.0) * 200.0) - 50.0;
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  Wire.begin();
  
  if (!Camera.begin(QCIF, RGB565, 5)) {
    Serial.println("ERROR: Camera init failed");
    while (1);
  }
  
  if (!initDHT20()) {
    Serial.println("WARNING: DHT20 init failed — using defaults");
  }
  
  Serial.println("READY");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'C') captureAndSend();
  }
}

void captureAndSend() {
  // Read temperature/humidity FIRST (before camera ties up time)
  float temperature = 25.0;  // fallback if read fails
  float humidity = 50.0;
  readDHT20(&temperature, &humidity);
  
  // Capture frame
  Camera.readFrame(imageData);
  
  // Send metadata + image
  Serial.println("FRAME_START");
  Serial.print("TEMP=");     Serial.println(temperature, 2);
  Serial.print("HUMIDITY="); Serial.println(humidity, 2);
  Serial.print("WIDTH=");    Serial.println(IMG_WIDTH);
  Serial.print("HEIGHT=");   Serial.println(IMG_HEIGHT);
  
  for (int i = 0; i < IMG_WIDTH * IMG_HEIGHT; i++) {
    uint16_t pixel = (imageData[i * 2] << 8) | imageData[i * 2 + 1];
    Serial.println(pixel, HEX);
  }
  
  Serial.println("FRAME_END");
}