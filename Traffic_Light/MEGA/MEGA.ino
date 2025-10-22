#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>

#define SLAVE_ADDR 8

// LED pins for 3 paths
int redPins[] = {6, 10, 13};
int yellowPins[] = {5, 8, 12};
int greenPins[] = {7, 9, 11};

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Emergency path (0 = none)
volatile int emergencyPath = 0;

// Status buffers
char latestStatus[32];
char prevStatus[32];

// Path states: 0=RED, 1=YELLOW, 2=GREEN
int pathState[3];
unsigned long previousMillis[3];
unsigned long durations[3];

// Custom durations for each path [path][state] in milliseconds
unsigned long customDurations[3][3] = {
  {5000, 2000, 6000},  // Path 1: RED, YELLOW, GREEN
  {6000, 1500, 5000},  // Path 2
  {4000, 2500, 7000}   // Path 3
};

void setup() {
  Wire.begin(SLAVE_ADDR);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  Serial.begin(9600);

  // Initialize pins and start all paths with RED
  for (int i = 0; i < 3; i++) {
    pinMode(redPins[i], OUTPUT);
    pinMode(yellowPins[i], OUTPUT);
    pinMode(greenPins[i], OUTPUT);

    pathState[i] = 0;        // RED
    setLights(i, 0);         // turn on RED
    durations[i] = customDurations[i][0];
    previousMillis[i] = millis();
  }

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Traffic Control");

  strcpy(latestStatus, "System Ready");
  strcpy(prevStatus, "");
  updateLCD();
}

void loop() {
  bool statusChanged = false;
  unsigned long currentMillis = millis();

  if (emergencyPath != 0) {
    handleEmergency();
    statusChanged = true;
  } else {
    // Sequential logic for each path
    for (int i = 0; i < 3; i++) {
      // Only check for state change if the duration has passed
      if (currentMillis - previousMillis[i] >= durations[i]) {
        // Tentatively move to next state
        int nextState = (pathState[i] + 1) % 3;

        // Check for conflicts
        if (!hasConflict(i, nextState)) {
          // No conflict: proceed
          pathState[i] = nextState;
          setLights(i, pathState[i]);
          durations[i] = customDurations[i][pathState[i]];
          previousMillis[i] = currentMillis;

          // Serial debug
          Serial.print("Path ");
          Serial.print(i + 1);
          Serial.print(" -> ");
          if (pathState[i] == 0) Serial.println("RED");
          else if (pathState[i] == 1) Serial.println("YELLOW");
          else Serial.println("GREEN");
        }
        // else: conflict exists, wait in current state (do nothing)
      }
    }

    strcpy(latestStatus, "RANDOM");
    statusChanged = true;
  }

  // Update LCD if status changed
  if (statusChanged && strcmp(latestStatus, prevStatus) != 0) {
    updateLCD();
    strcpy(prevStatus, latestStatus);
  }
}

// -------------------- Core Functions --------------------
void setLights(int path, int state) {
  digitalWrite(redPins[path], state == 0 ? HIGH : LOW);
  digitalWrite(yellowPins[path], state == 1 ? HIGH : LOW);
  digitalWrite(greenPins[path], state == 2 ? HIGH : LOW);
}

// Returns true if nextState would cause conflict with other paths
bool hasConflict(int pathIndex, int nextState) {
  if (nextState == 2) { // GREEN
    for (int j = 0; j < 3; j++) {
      if (j != pathIndex && pathState[j] == 2) return true;
    }
  }
  if (nextState == 1) { // YELLOW
    for (int j = 0; j < 3; j++) {
      if (j != pathIndex && pathState[j] == 1) return true;
    }
  }
  return false;
}

// -------------------- Emergency Handler --------------------
void handleEmergency() {
  for (int i = 0; i < 3; i++) {
    if (i == emergencyPath - 1) {
      digitalWrite(redPins[i], LOW);
      digitalWrite(yellowPins[i], LOW);
      digitalWrite(greenPins[i], HIGH);
    } else {
      digitalWrite(redPins[i], HIGH);
      digitalWrite(yellowPins[i], LOW);
      digitalWrite(greenPins[i], LOW);
    }
  }

  char temp[32];
  sprintf(temp, "EMERGENCY PATH %d", emergencyPath);
  strcpy(latestStatus, temp);
}

// -------------------- I2C Communication --------------------
void receiveEvent(int howMany) {
  if (Wire.available()) {
    int path = Wire.read(); // 0 = none, 1–3 = emergency path
    if (path >= 0 && path <= 3) {
      emergencyPath = path;
      Serial.print("Emergency received: ");
      Serial.println(path);
    }
  }
}

void requestEvent() {
  Wire.write((uint8_t*)latestStatus, strlen(latestStatus));
}

// -------------------- LCD Update --------------------
void updateLCD() {
  lcd.setCursor(0, 1);
  lcd.print("                "); // clear previous
  lcd.setCursor(0, 1);
  lcd.print(latestStatus);
  Serial.println(latestStatus);
}
