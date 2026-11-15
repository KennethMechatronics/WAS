// ===== LP5817.h =====
#pragma once
#include <Arduino.h>
#include <Wire.h>

class LP5817 {
public:
  static constexpr uint8_t I2C_ADDR = 0x2D;

  enum : uint8_t {
    REG_CHIP_EN          = 0x00,
    REG_DEV_CONFIG0      = 0x01,
    REG_DEV_CONFIG1      = 0x02,
    REG_DEV_CONFIG2      = 0x03,
    REG_DEV_CONFIG3      = 0x04,
    REG_SHUTDOWN_CMD     = 0x0D,
    REG_RESET_CMD        = 0x0E,
    REG_UPDATE_CMD       = 0x0F,
    REG_FLAG_CLR         = 0x13,
    REG_OUT0_DC          = 0x14,
    REG_OUT1_DC          = 0x15,
    REG_OUT2_DC          = 0x16,
    REG_OUT0_PWM         = 0x18,
    REG_OUT1_PWM         = 0x19,
    REG_OUT2_PWM         = 0x1A,
    REG_FLAG_READ        = 0x40
  };

  explicit LP5817(TwoWire& w = Wire, uint8_t addr = I2C_ADDR);

  bool begin();
  bool setMaxCurrent(bool mc51mA);
  bool enableOutputs(uint8_t mask);
  bool setExponential(bool out0, bool out1, bool out2);
  bool setFade(uint8_t timeCode, bool out0_en, bool out1_en, bool out2_en);
  bool setDC(uint8_t dc0, uint8_t dc1, uint8_t dc2);
  bool setPWM(uint8_t p0, uint8_t p1, uint8_t p2);
  bool update();
  bool shutdown();
  bool reset();
  int  readFlags();
  bool setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t dcR=255, uint8_t dcG=255, uint8_t dcB=255);

private:
  TwoWire* _w;
  uint8_t _addr;

  bool write8(uint8_t reg, uint8_t val);
  int  read8(uint8_t reg);
};

