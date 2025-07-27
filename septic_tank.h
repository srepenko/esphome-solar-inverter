#pragma once

#include "esphome.h"

class UARTSensor {
 public:
  UARTSensor(UARTComponent *uart) : uart_(uart) {}

  float read_distance() {
    uint8_t data[2];
    uart_->read_bytes(data, 2);
    uint16_t distance_raw = (data[0] << 8) | data[1];
    return static_cast<float>(distance_raw) / 100.0;
  }

 private:
  UARTComponent *uart_;
};
