#pragma once
#include "esphome/components/number/number.h"

namespace esphome {
namespace solar_inverter {

class SolarInverter;

class InverterNumber : public number::Number, public number::NumberTraits {
 public:
  void set_parent(SolarInverter *parent) { parent_ = parent; }
  void set_command_prefix(const std::string &p) { cmd_prefix_ = p; }
  void set_format(const std::string &f) { format_ = f; }
  // Local-only: HA number stores a value (e.g. probe NN) without UART SET.
  void set_local_only(bool local_only) { local_only_ = local_only; }
  void set_state_from_inverter(float value) {
    is_from_inverter_ = true;
    this->publish_state(value);
    is_from_inverter_ = false;
  }

  void control(float value) override;  // <--- только объявляем

  esphome::number::NumberTraits get_traits() const  {
    return this->traits;
  }

 protected:
  SolarInverter *parent_{nullptr};
  std::string cmd_prefix_;
  std::string format_{"%s%03d"};
  bool is_from_inverter_{false};
  bool local_only_{false};
};

}  // namespace solar_inverter
}  // namespace esphome
