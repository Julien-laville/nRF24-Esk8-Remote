#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>

#include "config.h"

#include "RF24.h"
#include "VescUart.h"
#include "vescValues.h"

#define TYPE TX_TYPE


#if TYPE == TX_TYPE


  #include <EEPROM.h>
  
  #include "battery.h"
  #include "settings.h"
  #include "screen.h"
  #include "stats.h"
  
  // Defining variables for speed and distance calculation
  float gearRatio;
  float ratioRpmSpeed;
  float ratioPulseDistance;
  
  byte currentSetting = 0;

  
  struct vescValues data;
  struct settings remoteSettings;
  
  // Defining variables for Hall Effect throttle.
  short hallMeasurement, throttle;
  byte hallCenterMargin = 4;
  
  // Defining variables for NRF24 communication
  bool connected = false;
  short failCount;
  unsigned long lastTransmission;
  
  // Defining variables for OLED display
  char displayBuffer[20];
  String displayString;
  short displayData = 0;
  unsigned long lastSignalBlink;
  unsigned long lastDataRotation;
  
  // Instantiating RF24 object for NRF24 communication
  
  // Defining variables for Settings menu
  bool changeSettings = false;
  bool changeSelectedSetting = false;
  
  bool settingsLoopFlag = false;
  bool settingsChangeFlag = false;
  bool settingsChangeValueFlag = false;
#else
  #include <nRF24L01.h>
  
  
  bool recievedData = false;
  uint32_t lastTimeReceived = 0;
  
  int motorSpeed = 127;
  int timeoutMax = 500;
  int speedPin = 5;
  
  
  struct vescValues data;
#endif

struct bldcMeasure measuredValues;
unsigned long lastDataCheck;

RF24 radio(9, 10);

void setup() {
  #if TYPE == TX_TYPE
    // setDefaultEEPROMSettings(); // Call this function if you want to reset settings
  
    #ifdef DEBUG
      Serial.begin(9600);
    #endif
  
    loadEEPROMSettings();
  
    pinMode(triggerPin, INPUT_PULLUP);
    pinMode(hallSensorPin, INPUT);
    pinMode(batteryMeasurePin, INPUT);
  
    u8g2.begin();
  
    drawStartScreen();
  
    if (triggerActive()) {
      changeSettings = true;
      drawTitleScreen("Remote Settings");
    }
  
    // Start radio communication
    radio.begin();
    radio.setPALevel(RF24_PA_MAX);
    radio.enableAckPayload();
    radio.enableDynamicPayloads();
    radio.openWritingPipe(pipe);
  
    #ifdef DEBUG
      printf_begin();
      radio.printDetails();
    #endif

  #else  // rx
  
     Serial.begin(115200);
  
      radio.begin();
      radio.enableAckPayload();
      radio.enableDynamicPayloads();
      radio.openReadingPipe(1, pipe);
      radio.startListening();
    
      pinMode(speedPin, OUTPUT);
      analogWrite(speedPin, motorSpeed);
  #endif
}

void loop() {
  #if TYPE == TX_TYPE
    calculateThrottlePosition();
  
    if (changeSettings == true) {
      // Use throttle and trigger to change settings
      controlSettingsMenu();
    }
    else
    {
      // Use throttle and trigger to drive motors
      if (triggerActive())
      {
        throttle = throttle;
      }
      else
      {
        // 127 is the middle position - no throttle and no brake/reverse
        throttle = 127;
      }
      // Transmit to receiver
      transmitToVesc();
    }
  
    // Call function to update display and LED
    updateMainDisplay();
    
  #else

    getVescData();
  
    // If transmission is available
    if (radio.available())
    {
      // The next time a transmission is received on pipe, the data in gotByte will be sent back in the acknowledgement (this could later be changed to data from VESC!)
      radio.writeAckPayload(pipe, &data, sizeof(data));
  
      // Read the actual message
      radio.read(&motorSpeed, sizeof(motorSpeed));
      recievedData = true;
    }
  
    if (recievedData == true)
    {
      // A speed is received from the transmitter (remote).
  
      lastTimeReceived = millis();
      recievedData = false;
  
      // Write the PWM signal to the ESC (0-255).
      analogWrite(speedPin, motorSpeed);
    }
    else if ((millis() - lastTimeReceived) > timeoutMax)
    {
      // No speed is received within the timeout limit.
      motorSpeed = 127;
      analogWrite(speedPin, motorSpeed);
    }

  #endif
}

void controlSettingsMenu() {
  if (triggerActive()) {
    if (settingsChangeFlag == false) {

      // Save settings to EEPROM
      if (changeSelectedSetting == true) {
        updateEEPROMSettings();
      }

      changeSelectedSetting = !changeSelectedSetting;
      settingsChangeFlag = true;
    }
  } else {
    settingsChangeFlag = false;
  }

  if (hallMeasurement >= (remoteSettings.maxHallValue - 150) && settingsLoopFlag == false) {
    // Up
    if (changeSelectedSetting == true) {
      int val = getSettingValue(currentSetting) + 1;

      if (inRange(val, settingRules[currentSetting][1], settingRules[currentSetting][2])) {
        setSettingValue(currentSetting, val);
        settingsLoopFlag = true;
      }
    } else {
      if (currentSetting != 0) {
        currentSetting--;
        settingsLoopFlag = true;
      }
    }
  }
  else if (hallMeasurement <= (remoteSettings.minHallValue + 150) && settingsLoopFlag == false) {
    // Down
    if (changeSelectedSetting == true) {
      int val = getSettingValue(currentSetting) - 1;

      if (inRange(val, settingRules[currentSetting][1], settingRules[currentSetting][2])) {
        setSettingValue(currentSetting, val);
        settingsLoopFlag = true;
      }
    } else {
      if (currentSetting < (numOfSettings - 1)) {
        currentSetting++;
        settingsLoopFlag = true;
      }
    }
  } else if (inRange(hallMeasurement, remoteSettings.centerHallValue - 50, remoteSettings.centerHallValue + 50)) {
    settingsLoopFlag = false;
  }
}

void drawSettingNumber() {
  // Position on OLED
  int x = 2; int y = 10;

  // Draw current setting number box
  u8g2.drawRFrame(x + 102, y - 10, 22, 32, 4);

  // Draw current setting number
  displayString = (String)(currentSetting + 1);
  displayString.toCharArray(displayBuffer, displayString.length() + 1);

  u8g2.setFont(u8g2_font_profont22_tn);
  u8g2.drawStr(x + 108, 22, displayBuffer);
}

void drawSettingsMenu() {
  // Position on OLED
  int x = 0; int y = 10;

  // Draw setting title
  displayString = settingPages[currentSetting][0];
  displayString.toCharArray(displayBuffer, displayString.length() + 1);

  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(x, y, displayBuffer);

  int val = getSettingValue(currentSetting);

  displayString = (String)val + "" + settingPages[currentSetting][1];
  displayString.toCharArray(displayBuffer, displayString.length() + 1);
  u8g2.setFont(u8g2_font_10x20_tr  );

  if (changeSelectedSetting == true) {
    u8g2.drawStr(x + 10, y + 20, displayBuffer);
  } else {
    u8g2.drawStr(x, y + 20, displayBuffer);
  }
}

void setDefaultEEPROMSettings() {
  for (int i = 0; i < numOfSettings; i++) {
    setSettingValue(i, settingRules[i][0]);
  }

  updateEEPROMSettings();
}

void loadEEPROMSettings() {
  // Load settings from EEPROM to custom struct
  EEPROM.get(0, remoteSettings);

  bool rewriteSettings = false;

  // Loop through all settings to check if everything is fine
  for (int i = 0; i < numOfSettings; i++) {
    int val = getSettingValue(i);

    if (! inRange(val, settingRules[i][1], settingRules[i][2])) {
      // Setting is damaged or never written. Rewrite default.
      rewriteSettings = true;
      setSettingValue(i, settingRules[i][0]);
    }
  }

  if (rewriteSettings == true) {
    updateEEPROMSettings();
  } else {
    // Calculate constants
    calculateRatios();
  }
}

// Write settings to the EEPROM then exiting settings menu.
void updateEEPROMSettings() {
  EEPROM.put(0, remoteSettings);
  calculateRatios();
}

// Update values used to calculate speed and distance travelled.
void calculateRatios() {
  gearRatio = (float)remoteSettings.motorPulley / (float)remoteSettings.wheelPulley;

  ratioRpmSpeed = (gearRatio * 60 * (float)remoteSettings.wheelDiameter * 3.14156) / (((float)remoteSettings.motorPoles / 2) * 1000000); // ERPM to Km/h

  ratioPulseDistance = (gearRatio * (float)remoteSettings.wheelDiameter * 3.14156) / (((float)remoteSettings.motorPoles * 3) * 1000000); // Pulses to km travelled
}

// Get settings value by index (usefull when iterating through settings).
int getSettingValue(int index) {
  int value;
  switch (index) {
    case 0: value = remoteSettings.triggerMode;     break;
    case 1: value = remoteSettings.batteryType;     break;
    case 2: value = remoteSettings.batteryCells;    break;
    case 3: value = remoteSettings.motorPoles;      break;
    case 4: value = remoteSettings.motorPulley;     break;
    case 5: value = remoteSettings.wheelPulley;     break;
    case 6: value = remoteSettings.wheelDiameter;   break;
    case 7: value = remoteSettings.useUart;         break;
    case 8: value = remoteSettings.minHallValue;    break;
    case 9: value = remoteSettings.centerHallValue; break;
    case 10: value = remoteSettings.maxHallValue;   break;
  }
  return value;
}

// Set a value of a specific setting by index.
void setSettingValue(int index, int value) {
  switch (index) {
    case 0: remoteSettings.triggerMode = value;     break;
    case 1: remoteSettings.batteryType = value;     break;
    case 2: remoteSettings.batteryCells = value;    break;
    case 3: remoteSettings.motorPoles = value;      break;
    case 4: remoteSettings.motorPulley = value;     break;
    case 5: remoteSettings.wheelPulley = value;     break;
    case 6: remoteSettings.wheelDiameter = value;   break;
    case 7: remoteSettings.useUart = value;         break;
    case 8: remoteSettings.minHallValue = value;    break;
    case 9: remoteSettings.centerHallValue = value; break;
    case 10: remoteSettings.maxHallValue = value;   break;
  }
}

// Check if an integer is within a min and max value
bool inRange(int val, int minimum, int maximum) {
  return ((minimum <= val) && (val <= maximum));
}

// Return true if trigger is activated, false otherwice
boolean triggerActive() {
  if (digitalRead(triggerPin) == LOW)
    return true;
  else
    return false;
}

// Function used to transmit the throttle value, and receive the VESC realtime data.
void transmitToVesc() {
  // Transmit once every 50 millisecond
  if (millis() - lastTransmission >= 50) {

    lastTransmission = millis();

    boolean sendSuccess = false;
    // Transmit the speed value (0-255).
    sendSuccess = radio.write(&throttle, sizeof(throttle));

    // Listen for an acknowledgement reponse (return of VESC data).
    while (radio.isAckPayloadAvailable()) {
      radio.read(&data, sizeof(data));
    }

    if (sendSuccess == true)
    {
      // Transmission was a succes
      failCount = 0;
      sendSuccess = false;

      DEBUG_PRINT("Transmission succes");
    } else {
      // Transmission was not a succes
      failCount++;

      DEBUG_PRINT("Failed transmission");
    }

    // If lost more than 5 transmissions, we can assume that connection is lost.
    if (failCount < 5) {
      connected = true;
    } else {
      connected = false;
    }
  }
}

void calculateThrottlePosition() {
  // Hall sensor reading can be noisy, lets make an average reading.
  int total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(hallSensorPin);
  }
  hallMeasurement = total / 10;

  DEBUG_PRINT( (String)hallMeasurement );

  if (hallMeasurement >= remoteSettings.centerHallValue) {
    throttle = constrain(map(hallMeasurement, remoteSettings.centerHallValue, remoteSettings.maxHallValue, 127, 255), 127, 255);
  } else {
    throttle = constrain(map(hallMeasurement, remoteSettings.minHallValue, remoteSettings.centerHallValue, 0, 127), 0, 127);
  }

  // removeing center noise
  if (abs(throttle - 127) < hallCenterMargin) {
    throttle = 127;
  }
}

// Function used to indicate the remotes battery level.
int batteryLevel() {
  float voltage = batteryVoltage();

  if (voltage <= minVoltage) {
    return 0;
  } else if (voltage >= maxVoltage) {
    return 100;
  } else {
    return (voltage - minVoltage) * 100 / (maxVoltage - minVoltage);
  }
}

// Function to calculate and return the remotes battery voltage.
float batteryVoltage() {
  float batteryVoltage = 0.0;
  int total = 0;

  for (int i = 0; i < 10; i++) {
    total += analogRead(batteryMeasurePin);
  }

  batteryVoltage = (refVoltage / 1024.0) * ((float)total / 10.0);

  return batteryVoltage;
}

void updateMainDisplay() {

  u8g2.firstPage();
  do {

    if (changeSettings == true) {
      drawSettingsMenu();
      drawSettingNumber();
    } else {
      drawThrottle();
      drawPage();
      drawBatteryLevel();
      drawSignal();
    }

  } while ( u8g2.nextPage() );
}

void drawStartScreen() {
  u8g2.firstPage();
  do {
    u8g2.drawXBM( 4, 4, 24, 24, logo_bits);

    displayString = "Esk8 remote";
    displayString.toCharArray(displayBuffer, 12);
    u8g2.setFont(u8g2_font_helvR10_tr  );
    u8g2.drawStr(34, 22, displayBuffer);
  } while ( u8g2.nextPage() );
  delay(1500);
}

void drawTitleScreen(String title) {
  u8g2.firstPage();
  do {
    title.toCharArray(displayBuffer, 20);
    u8g2.setFont(u8g2_font_helvR10_tr  );
    u8g2.drawStr(12, 20, displayBuffer);
  } while ( u8g2.nextPage() );
  delay(1500);
}

void drawPage() {
  int decimals;
  float value;
  String suffix;
  String prefix;

  int first, last;

  int x = 0;
  int y = 16;

  // Rotate the realtime data each 4s.
  if ((millis() - lastDataRotation) >= 4000) {

    lastDataRotation = millis();
    displayData++;

    if (displayData > 2) {
      displayData = 0;
    }
  }

  switch (displayData) {
    case 0:
      value = ratioRpmSpeed * data.rpm;
      suffix = "KMH";
      prefix = "SPEED";
      decimals = 1;
      break;
    case 1:
      value = ratioPulseDistance * data.tachometerAbs;
      suffix = "KM";
      prefix = "DISTANCE";
      decimals = 2;
      break;
    case 2:
      value = data.inpVoltage;
      suffix = "V";
      prefix = "BATTERY";
      decimals = 1;
      break;
  }

  // Display prefix (title)
  displayString = prefix;
  displayString.toCharArray(displayBuffer, 10);
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(x, y - 1, displayBuffer);

  // Split up the float value: a number, b decimals.
  first = abs(floor(value));
  last = value * pow(10, 3) - first * pow(10, 3);

  // Add leading zero
  if (first <= 9) {
    displayString = "0" + (String)first;
  } else {
    displayString = (String)first;
  }

  // Display numbers
  displayString.toCharArray(displayBuffer, 10);
  u8g2.setFont(u8g2_font_logisoso22_tn );
  u8g2.drawStr(x + 55, y + 13, displayBuffer);

  // Display decimals
  displayString = "." + (String)last;
  displayString.toCharArray(displayBuffer, decimals + 2);
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(x + 86, y - 1, displayBuffer);

  // Display suffix
  displayString = suffix;
  displayString.toCharArray(displayBuffer, 10);
  u8g2.setFont(u8g2_font_profont12_tr);
  u8g2.drawStr(x + 86 + 2, y + 13, displayBuffer);

}

void drawThrottle() {
  int x = 0;
  int y = 18;

  // Draw throttle
  u8g2.drawHLine(x, y, 52);
  u8g2.drawVLine(x, y, 10);
  u8g2.drawVLine(x + 52, y, 10);
  u8g2.drawHLine(x, y + 10, 5);
  u8g2.drawHLine(x + 52 - 4, y + 10, 5);

  if (throttle >= 127) {
    int width = map(throttle, 127, 255, 0, 49);

    for (int i = 0; i < width; i++) {
      //if( (i % 2) == 0){
      u8g2.drawVLine(x + i + 2, y + 2, 7);
      //}
    }
  } else {
    int width = map(throttle, 0, 126, 49, 0);
    for (int i = 0; i < width; i++) {
      //if( (i % 2) == 0){
      u8g2.drawVLine(x + 50 - i, y + 2, 7);
      //}
    }

  }
}

bool signalBlink = false;

void drawSignal() {
  // Position on OLED
  int x = 114; int y = 17;

  if (connected == true) {
    if (triggerActive()) {
      u8g2.drawXBM(x, y, 12, 12, signal_transmitting_bits);
    } else {
      u8g2.drawXBM(x, y, 12, 12, signal_connected_bits);
    }
  } else {
    if (millis() - lastSignalBlink > 500) {
      signalBlink = !signalBlink;
      lastSignalBlink = millis();
    }

    if (signalBlink == true) {
      u8g2.drawXBM(x, y, 12, 12, signal_connected_bits);
    } else {
      u8g2.drawXBM(x, y, 12, 12, signal_noconnection_bits);
    }
  }
}

void drawBatteryLevel() {
  int level = batteryLevel();

  // Position on OLED
  int x = 108; int y = 4;

  u8g2.drawFrame(x + 2, y, 18, 9);
  u8g2.drawBox(x, y + 2, 2, 5);

  for (int i = 0; i < 5; i++) {
    int p = round((100 / 5) * i);
    if (p <= level)
    {
      u8g2.drawBox(x + 4 + (3 * i), y + 2, 2, 5);
    }
  }
}

void getVescData() {

  if (millis() - lastDataCheck >= 250) {

    lastDataCheck = millis();

    // Only transmit what we need
    if (VescUartGetValue(measuredValues)) {
      data.ampHours = measuredValues.ampHours;
      data.inpVoltage = measuredValues.inpVoltage;
      data.rpm = measuredValues.rpm;
      data.tachometerAbs = measuredValues.tachometerAbs;
    } else {
      data.ampHours = 0.0;
      data.inpVoltage = 0.0;
      data.rpm = 0;
      data.tachometerAbs = 0;
    }
  }
}

