#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>

// I2C slave address
#define SLAVE_ADDR 8

// Traffic light LED pins for 3 paths
int redPins[] = {6, 10, 13};
int yellowPins[] = {5, 8, 12};
int greenPins[] = {7, 9, 11};

// LCD
LiquidCrystal_I2C lcd(0x27,16,2);

// Emergency path (0 = none)
volatile int emergencyPath = 0;

// Status buffers
char latestStatus[32];
char prevStatus[32];

// Pause after emergency
unsigned long emergencyEndTime = 0;

// Normal mode timing
unsigned long previousMillis = 0;
unsigned long interval = 3000; // 3 seconds per random pattern

void setup() {
  Wire.begin(SLAVE_ADDR);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  Serial.begin(9600);

  // Initialize LEDs
  for(int i=0;i<3;i++){
    pinMode(redPins[i],OUTPUT);
    pinMode(yellowPins[i],OUTPUT);
    pinMode(greenPins[i],OUTPUT);
    digitalWrite(redPins[i], HIGH);
    digitalWrite(yellowPins[i], LOW);
    digitalWrite(greenPins[i], LOW);
  }

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Traffic Light");

  strcpy(latestStatus,"STOP");
  strcpy(prevStatus,"");
  updateLCD();

  randomSeed(analogRead(0)); // Initialize random
}

void loop() {
  bool statusChanged = false;

  if(emergencyPath != 0){
    // Emergency override: only emergencyPath green, others red
    for(int i=0;i<3;i++){
      if(i == emergencyPath-1){
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
    sprintf(temp,"EMERGENCY PATH %d", emergencyPath);
    if(strcmp(temp, latestStatus) != 0){
      strcpy(latestStatus,temp);
      statusChanged = true;
    }

    emergencyEndTime = 0; // reset pause

  } else {
    // If emergency just ended, wait 2 sec before resuming random cycle
    if(emergencyEndTime == 0){
      emergencyEndTime = millis();
      return; // skip normal cycle this iteration
    }
    if(millis() - emergencyEndTime < 2000) return; // 2-sec pause

    // Random traffic light pattern
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis >= interval){
      previousMillis = currentMillis;

      // Generate random state for each path
      for(int i=0;i<3;i++){
        int r = random(0,3); // 0=RED, 1=YELLOW, 2=GREEN
        switch(r){
          case 0: digitalWrite(redPins[i], HIGH); digitalWrite(yellowPins[i], LOW);  digitalWrite(greenPins[i], LOW); break;
          case 1: digitalWrite(redPins[i], LOW);  digitalWrite(yellowPins[i], HIGH); digitalWrite(greenPins[i], LOW); break;
          case 2: digitalWrite(redPins[i], LOW);  digitalWrite(yellowPins[i], LOW);  digitalWrite(greenPins[i], HIGH); break;
        }
      }

      // Set friendly status message instead of raw LED values
      strcpy(latestStatus, "RANDOM");
      statusChanged = true;
    }
  }

  if(statusChanged && strcmp(latestStatus, prevStatus) != 0){
    updateLCD();
    strcpy(prevStatus, latestStatus);
  }
}

void receiveEvent(int howMany){
  if(Wire.available()){
    int path = Wire.read(); // 0 = no emergency, 1-3 = path
    if(path >=0 && path <=3){
      emergencyPath = path;
      Serial.print("Emergency received: ");
      Serial.println(path);
    }
  }
}

void requestEvent() {
  Wire.write((uint8_t*)latestStatus, strlen(latestStatus));
}

void updateLCD(){
  lcd.setCursor(0,1);
  lcd.print("                "); // clear line
  lcd.setCursor(0,1);
  lcd.print(latestStatus);
  Serial.println(latestStatus);
}