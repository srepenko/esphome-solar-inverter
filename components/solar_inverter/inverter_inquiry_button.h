#pragma once

#include "esphome/components/button/button.h"

namespace esphome {
namespace solar_inverter {

class SolarInverter;

class InverterInquiryButton : public button::Button {
 public:
  void set_parent(SolarInverter *parent) { parent_ = parent; }
  void set_inquiry_command(const std::string &cmd) { cmd_ = cmd; }
  void set_dump_all(bool dump_all) { dump_all_ = dump_all; }

 protected:
  void press_action() override;

  SolarInverter *parent_{nullptr};
  std::string cmd_;
  bool dump_all_{false};
};

}  // namespace solar_inverter
}  // namespace esphome
