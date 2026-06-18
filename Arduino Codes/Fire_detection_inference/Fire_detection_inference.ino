/*
 * IKEA Fire Detection — Inference Sketch
 * 
 * Runs trained Edge Impulse model on Arduino Nano 33 BLE Sense
 * with DHT20 temperature sensor and OV7675 camera.
 * 
 * Outputs:
 *   - Onboard RGB LED (red during fire, green during safe)
 *   - Serial log (115200 baud)
 *   - BLE characteristics for laptop bridge
 */

#include <fire-detection_inferencing.h>

#include <Arduino_OV767X.h>
#include <Wire.h>
#include <ArduinoBLE.h>

// ==== Pin Configuration ====
#define DHT20_ADDRESS  0x38
// Onboard RGB LED pins are predefined as LEDR / LEDG / LEDB
// They are ACTIVE LOW: LOW = ON, HIGH = OFF

// ==== Camera config ====
#define CAMERA_WIDTH   176
#define CAMERA_HEIGHT  144
byte cameraFrame[CAMERA_WIDTH * CAMERA_HEIGHT * 2];

// ==== Model input ====
#define MODEL_IMG_WIDTH    32
#define MODEL_IMG_HEIGHT   32
#define TOTAL_FEATURES     (MODEL_IMG_WIDTH * MODEL_IMG_HEIGHT + 1)
float features[TOTAL_FEATURES];

// ==== Latching alarm logic ====
#define CONFIDENCE_THRESHOLD  0.7f
#define REQUIRED_STREAK       2
int fireStreak = 0;
bool alarmActive = false;

// ==== BLE service ====
BLEService fireService("180A");
BLEStringCharacteristic classChar("2A19", BLERead | BLENotify, 24);
BLEFloatCharacteristic  confChar("2A1A",  BLERead | BLENotify);

// ==== Helper: control onboard RGB LED ====
void setLED(bool red, bool green, bool blue) {
  // Active LOW: LOW = ON, HIGH = OFF
  digitalWrite(LEDR, red   ? LOW : HIGH);
  digitalWrite(LEDG, green ? LOW : HIGH);
  digitalWrite(LEDB, blue  ? LOW : HIGH);
}

// ==== DHT20 helpers ====
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

// ==== Edge Impulse signal callback ====
int get_signal_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

// ==== Downscale 176x144 RGB565 to 32x32 grayscale ====
void prepareFeatures(float temperature) {
  float x_scale = (float)CAMERA_WIDTH  / MODEL_IMG_WIDTH;
  float y_scale = (float)CAMERA_HEIGHT / MODEL_IMG_HEIGHT;
  
  for (int y = 0; y < MODEL_IMG_HEIGHT; y++) {
    for (int x = 0; x < MODEL_IMG_WIDTH; x++) {
      int src_x = (int)(x * x_scale);
      int src_y = (int)(y * y_scale);
      int idx = (src_y * CAMERA_WIDTH + src_x) * 2;
      
      uint16_t pixel = (cameraFrame[idx] << 8) | cameraFrame[idx + 1];
      uint8_t r = ((pixel >> 11) & 0x1F) << 3;
      uint8_t g = ((pixel >> 5)  & 0x3F) << 2;
      uint8_t b = ( pixel        & 0x1F) << 3;
      
      uint8_t gray = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
      features[y * MODEL_IMG_WIDTH + x] = gray / 255.0f;
    }
  }
  
  features[MODEL_IMG_WIDTH * MODEL_IMG_HEIGHT] = temperature;
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000);
  
  // Setup onboard RGB LED pins
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  setLED(false, true, false);  // Start green
  
  Wire.begin();
  if (initDHT20()) {
    Serial.println("DHT20 OK");
  } else {
    Serial.println("WARNING: DHT20 init failed");
  }
  
  if (!Camera.begin(QCIF, RGB565, 5)) {
    Serial.println("ERROR: Camera init failed");
    // Flash red forever if camera failed
    while (1) {
      setLED(true, false, false); delay(200);
      setLED(false, false, false); delay(200);
    }
  }
  Serial.println("Camera OK");
  
  if (BLE.begin()) {
    BLE.setLocalName("IKEA-Fire-Node");
    BLE.setAdvertisedService(fireService);
    fireService.addCharacteristic(classChar);
    fireService.addCharacteristic(confChar);
    BLE.addService(fireService);
    classChar.writeValue("safe");
    confChar.writeValue(0.0f);
    BLE.advertise();
    Serial.println("BLE advertising as IKEA-Fire-Node");
  } else {
    Serial.println("WARNING: BLE init failed");
  }
  
  Serial.println("=== Fire Detector Ready ===");
  Serial.println("Format: STATE,class,confidence,temp,raw_label,streak");
}

void loop() {
  // 1. Read temperature
  float temperature = 25.0f;
  float humidity = 50.0f;
  readDHT20(&temperature, &humidity);
  
  // 2. Capture camera frame
  Camera.readFrame(cameraFrame);
  
  // 3. Build input vector
  prepareFeatures(temperature);
  
  // 4. Run inference
  ei_impulse_result_t result = { 0 };
  signal_t signal;
  signal.total_length = TOTAL_FEATURES;
  signal.get_data = &get_signal_data;
  
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
    Serial.print("Inference failed: "); Serial.println(err);
    delay(1000);
    return;
  }
  
  // 5. Find best class
  int bestIdx = 0;
  float bestConf = result.classification[0].value;
  for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (result.classification[i].value > bestConf) {
      bestConf = result.classification[i].value;
      bestIdx = i;
    }
  }
  const char *bestLabel = result.classification[bestIdx].label;
  
  // 6. Latching logic
  if (strcmp(bestLabel, "fire") == 0 && bestConf > CONFIDENCE_THRESHOLD) {
    fireStreak++;
  } else {
    fireStreak = 0;
  }
  alarmActive = (fireStreak >= REQUIRED_STREAK);
  
  // 7. Onboard LED: red during fire, green during safe
  if (alarmActive) {
    setLED(true, false, false);   // Red
  } else {
    setLED(false, true, false);   // Green
  }
  
  // 8. BLE update
  classChar.writeValue(alarmActive ? "fire" : "safe");
  confChar.writeValue(bestConf);
  
  // 9. Serial log
  Serial.print("STATE,");
  Serial.print(alarmActive ? "fire" : "safe");
  Serial.print(",");
  Serial.print(bestConf, 4);
  Serial.print(",");
  Serial.print(temperature, 2);
  Serial.print(",");
  Serial.print(bestLabel);
  Serial.print(",streak=");
  Serial.println(fireStreak);
  
  BLE.poll();
  delay(100);
}