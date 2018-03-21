const uint64_t pipe = 0xE8E8F0F0E1LL;
// #define DEBUG
// #define CLEAR_EEPROM

  
#define TX_TYPE 0
#define RX_TYPE 1

// Pin definition
const byte triggerPin = 4;
const int chargeMeasurePin = A1;
const int batteryMeasurePin = A2;
const int hallSensorPin = A3;

const byte numOfSettings = 11;

#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.println (x)
  #include "printf.h"
#else
  #define DEBUG_PRINT(x)
#endif



