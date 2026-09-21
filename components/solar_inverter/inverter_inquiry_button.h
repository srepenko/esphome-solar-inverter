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
  // Whitelisted POP/PCP SET probe (not a free-form send_command bypass).
  void set_set_probe(bool set_probe) { set_probe_ = set_probe; }
  // When set ("POP" / "PCP"), press builds PREFIX+NN from debug_probe_nn.
  void set_set_probe_prefix(const std::string &prefix) { set_probe_prefix_ = prefix; }

 protected:
  void press_action() override;

  SolarInverter *parent_{nullptr};
  std::string cmd_;
  bool dump_all_{false};
  bool set_probe_{false};
  std::string set_probe_prefix_;
};

}  // namespace solar_inverter
}  // namespace esphome
