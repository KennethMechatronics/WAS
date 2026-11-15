#include "LP5817.h"

LP5817::LP5817(TwoWire& w, uint8_t addr) : _w(&w), _addr(addr) {}

bool LP5817::begin() {
  bool ok = write8(REG_CHIP_EN, 0x01);
  ok &= update();
  ok &= write8(REG_FLAG_CLR, 0x01);
  return ok;
}

bool LP5817::setMaxCurrent(bool mc51mA) {
  return write8(REG_DEV_CONFIG0, mc51mA ? 0x01 : 0x00);
}

bool LP5817::enableOutputs(uint8_t mask) {
  uint8_t v = 0;
  if (mask & 0x01) v |= 0x01;
  if (mask & 0x02) v |= 0x02;
  if (mask & 0x04) v |= 0x04;
  return write8(REG_DEV_CONFIG1, v);
}

bool LP5817::setExponential(bool out0, bool out1, bool out2) {
  uint8_t v = (out0 ? 0x10 : 0) | (out1 ? 0x20 : 0) | (out2 ? 0x40 : 0);
  return write8(REG_DEV_CONFIG3, v);
}

bool LP5817::setFade(uint8_t timeCode, bool out0_en, bool out1_en, bool out2_en) {
  timeCode &= 0xF;
  uint8_t v = (timeCode << 4) | (out0_en ? 0x01 : 0) | (out1_en ? 0x02 : 0) | (out2_en ? 0x04 : 0);
  return write8(REG_DEV_CONFIG2, v);
}

bool LP5817::setDC(uint8_t dc0, uint8_t dc1, uint8_t dc2) {
  return write8(REG_OUT0_DC, dc0) && write8(REG_OUT1_DC, dc1) && write8(REG_OUT2_DC, dc2);
}

bool LP5817::setPWM(uint8_t p0, uint8_t p1, uint8_t p2) {
  return write8(REG_OUT0_PWM, p0) && write8(REG_OUT1_PWM, p1) && write8(REG_OUT2_PWM, p2);
}

bool LP5817::update() { return write8(REG_UPDATE_CMD, 0x55); }

bool LP5817::shutdown() { return write8(REG_SHUTDOWN_CMD, 0x33); }

bool LP5817::reset() { return write8(REG_RESET_CMD, 0xCC); }

int LP5817::readFlags() { return read8(REG_FLAG_READ); }

bool LP5817::setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t dcR, uint8_t dcG, uint8_t dcB) {
  bool ok = setDC(dcR, dcG, dcB);
  ok &= update();
  ok &= enableOutputs(0x07);
  ok &= setPWM(r, g, b);
  return ok;
}

bool LP5817::write8(uint8_t reg, uint8_t val) {
  _w->beginTransmission(_addr);
  _w->write(reg);
  _w->write(val);
  return _w->endTransmission() == 0;
}

int LP5817::read8(uint8_t reg) {
  _w->beginTransmission(_addr);
  _w->write(reg);
  if (_w->endTransmission(false) != 0) return -1;
  if (_w->requestFrom((int)_addr, 1) != 1) return -1;
  return _w->read();
}

