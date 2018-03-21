// Defining struct to hold setting values while remote is turned on.
struct settings {
  byte triggerMode;
  byte batteryType;
  byte batteryCells;
  byte motorPoles;
  byte motorPulley;
  byte wheelPulley;
  byte wheelDiameter;
  bool useUart;
  int minHallValue;
  int centerHallValue;
  int maxHallValue;
};

// Setting rules format: default, min, max.
int settingRules[numOfSettings][3] {
  {0, 0, 3}, // 0 Killswitch, 1 cruise & 2 data toggle
  {0, 0, 1}, // 0 Li-ion & 1 LiPo
  {10, 0, 12},
  {14, 0, 250},
  {15, 0, 250},
  {40, 0, 250},
  {83, 0, 250},
  {1, 0, 1}, // Yes or no
  {0, 0, 1023},
  {512, 0, 1023},
  {1023, 0, 1023}
};
