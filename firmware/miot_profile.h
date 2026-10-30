#pragma once

// Keep this type in a header so Arduino's automatic function prototypes can
// see it before generating prototypes for functions that return MiotProfile.
struct MiotProfile {
  const char *name;
  uint8_t acService, power, mode, temperature, eco, sleep;
  uint8_t fanService, fanLevel, verticalSwing, verticalAngle;
  uint8_t environmentService, environmentTemperature;
  uint8_t horizontalSwing, offMode;
};
