#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID    "TMPL3aeiekkQP"
#define BLYNK_TEMPLATE_NAME  "Quickstart Device"
#define BLYNK_AUTH_TOKEN     "HVhqq0bWSCBZJ2bKYdJtmMY2igh0nM4n"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// ===== WiFi credentials =====
char ssid[] = "abhi";
char pass[] = "abhi2004";

// ===== I2C slave address (Arduino Mega) =====
#define SLAVE_ADDR 8

// ===== Blynk virtual pins =====
#define VP_PATH1 V4
#define VP_PATH2 V5
#define VP_PATH3 V6
#define VP_LCD   V1

// ===== Traffic status buffers =====
char lightStatus[32];
char lastStatus[32];  

// ===== I2S Mic Config =====
#define I2S_WS   25
#define I2S_SD   32
#define I2S_SCK  33
#define SAMPLES 512
#define SAMPLING_FREQUENCY 16000

double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>();
#define LED_BUILTIN 2

// ===== IR Sensor Pins =====
#define IR_PATH1 14
#define IR_PATH2 27
#define IR_PATH3 26
int irPins[] = {IR_PATH1, IR_PATH2, IR_PATH3};

// ===== Emergency detection =====
volatile int manualEmergencyPath = 0; // Blynk manual emergency
volatile int autoEmergencyPath = 0;   // IR+Siren automatic emergency
bool emergencyActive = false;         // true if any emergency active
unsigned long lastRead = 0;
const unsigned long readInterval = 1000; 
unsigned long lastI2SRead = 0;
const unsigned long i2sInterval = 100; 
const double SIREN_MIN_FREQ = 600.0;
const double SIREN_MAX_FREQ = 1500.0;
const double AMPLITUDE_THRESHOLD = 50000.0; // adjust for your mic

// ===== Flashing LCD =====
unsigned long lastFlash = 0;
bool flashState = false;
const unsigned long flashInterval = 500; // 500ms toggle

// ===== Helper: Write Blynk LCD with padding =====
void writeBlynkLCD(const char* msg){
  char buffer[32]; // max 32 chars
  snprintf(buffer, sizeof(buffer), "%-16s", msg); // pad to 16 chars
  Blynk.virtualWrite(VP_LCD, buffer);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // I2C pins for ESP32

  pinMode(LED_BUILTIN, OUTPUT);
  for(int i=0;i<3;i++) pinMode(irPins[i], INPUT_PULLUP); // IR sensors

  // Connect WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");

  // Connect Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);
  while (!Blynk.connect()) { delay(500); Serial.print("."); }
  Serial.println("\nConnected to Blynk Cloud!");

  lastStatus[0] = '\0';

  // Initialize I2S mic
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLING_FREQUENCY,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };
  i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = -1, .data_in_num = I2S_SD };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

// ===== Send emergency path to Mega slave =====
void sendEmergencyCommand(int path) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(path);
  Wire.endTransmission();
  Serial.printf("Emergency command sent: %d\n", path);
}

// ===== Read traffic light status =====
void readTrafficStatus() {
  Wire.requestFrom(SLAVE_ADDR, 32);
  int i = 0;
  while (Wire.available() && i < sizeof(lightStatus) - 1) lightStatus[i++] = Wire.read();
  lightStatus[i] = '\0';

  // Ignore RANDOM updates during active emergency
  if(emergencyActive && strcmp(lightStatus, "RANDOM") == 0) return;

  if (strcmp(lightStatus, lastStatus) != 0 && strlen(lightStatus) > 0) {
    Serial.printf("New status: %s\n", lightStatus);
    writeBlynkLCD(lightStatus); // padded write
    strcpy(lastStatus, lightStatus);
  }
}

// ===== I2S Mic FFT read =====
double readMicAndDetectSiren() {
  int32_t samples[SAMPLES];
  size_t bytesRead;
  i2s_read(I2S_NUM_0, (void*)samples, sizeof(samples), &bytesRead, portMAX_DELAY);
  int sampleCount = bytesRead / sizeof(int32_t);
  if (sampleCount < SAMPLES) return 0;

  for (int i=0;i<SAMPLES;i++){ vReal[i]=(double)(samples[i]>>14); vImag[i]=0.0; }

  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);

  double peak = 0; int index=0;
  for(int i=1;i<SAMPLES/2;i++){
    if(vReal[i]>peak){ peak=vReal[i]; index=i; }
  }

  double frequency = (index * ((double)SAMPLING_FREQUENCY / SAMPLES));
  if(frequency >= SIREN_MIN_FREQ && frequency <= SIREN_MAX_FREQ && peak > AMPLITUDE_THRESHOLD){
    digitalWrite(LED_BUILTIN, HIGH);
    return frequency;
  } else { digitalWrite(LED_BUILTIN, LOW); return 0; }
}

// ===== Blynk button handlers =====
void handleBlynkEmergency(int path, int val){
  manualEmergencyPath = val ? path : 0;  // 0 = cleared
  emergencyActive = (manualEmergencyPath != 0) || (autoEmergencyPath != 0);
  sendEmergencyCommand(manualEmergencyPath ? manualEmergencyPath : 0);
  if(manualEmergencyPath == 0 && autoEmergencyPath == 0){
    writeBlynkLCD("RANDOM");
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Blynk cleared emergency, back to RANDOM mode");
  }
}

BLYNK_WRITE(VP_PATH1){ handleBlynkEmergency(1, param.asInt()); }
BLYNK_WRITE(VP_PATH2){ handleBlynkEmergency(2, param.asInt()); }
BLYNK_WRITE(VP_PATH3){ handleBlynkEmergency(3, param.asInt()); }

// ===== Main loop =====
void loop() {
  Blynk.run();
  unsigned long currentMillis = millis();

  // Read traffic status every second
  if(currentMillis - lastRead >= readInterval){
    lastRead = currentMillis;
    if(!emergencyActive){
      readTrafficStatus();
    }
  }

  // I2S mic detection
  if(currentMillis - lastI2SRead >= i2sInterval){
    lastI2SRead = currentMillis;
    double freq = readMicAndDetectSiren();

    // Automatic emergency: ONLY if siren detected
    if(freq > 0 && manualEmergencyPath == 0){
      int detectedPath = 0;
      // Check IR only if siren present
      for(int i=0;i<3;i++){
        if(digitalRead(irPins[i]) == LOW){
          detectedPath = i+1;
          break;
        }
      }

      if(detectedPath != 0){
        autoEmergencyPath = detectedPath;
        emergencyActive = true;
        sendEmergencyCommand(detectedPath);
        Serial.printf("Automatic emergency triggered on path %d, freq: %.1f Hz\n", detectedPath, freq);
      }
    }

    // Flash EMERGENCY on LCD while any emergency active
    if(emergencyActive){
      if(currentMillis - lastFlash >= flashInterval){
        lastFlash = currentMillis;
        flashState = !flashState;
        if (flashState){
          writeBlynkLCD("AMBULANCE!");
        } else {
          writeBlynkLCD("          ");
        }
      }
    }

    // Automatic reset for automatic emergency
    if(autoEmergencyPath != 0 && manualEmergencyPath == 0){
      int pathIndex = autoEmergencyPath - 1;
      if(digitalRead(irPins[pathIndex]) == HIGH && freq == 0){ // siren gone & IR normal
        autoEmergencyPath = 0;
        emergencyActive = false;
        sendEmergencyCommand(0);
        writeBlynkLCD("RANDOM");
        digitalWrite(LED_BUILTIN, LOW);
        Serial.println("Automatic emergency cleared, back to RANDOM mode");
      }
    }
  }
}
