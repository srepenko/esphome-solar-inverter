//inverter_select.h
#pragma once

#include "esphome/components/select/select.h"

namespace esphome {
namespace solar_inverter {

  class InverterSelect : public select::Select {
    public:
     using UserSelectCallback = std::function<void(const std::string &)>;
     void set_command_prefix(const std::string &prefix) { this->command_prefix_ = prefix; }
     const std::string &get_command_prefix() const { return this->command_prefix_; }

     void set_status_command(const std::string &cmd) { this->status_command_ = cmd; }
     const std::string &get_status_command() const { return this->status_command_; }

     void set_parameters(const std::vector<std::string> &params) { this->parameters_ = params; }
     const std::vector<std::string> &get_parameters() const { return this->parameters_; }

     // Full UART commands (e.g. POP00). Empty string = read-only option, no SET.
     void set_set_commands(const std::vector<std::string> &cmds) { this->set_commands_ = cmds; }
     const std::vector<std::string> &get_set_commands() const { return this->set_commands_; }

     void set_field_name(const std::string &name) { this->field_name_ = name; }
     const std::string &get_field_name() const { return this->field_name_; }

     void set_options_list(const std::vector<std::string> &opts) { this->options_ = opts; }
     const std::vector<std::string> &get_options_list() const { return options_; }

     void control(const std::string &value) override {
       // Do not publish_state here. HA must show the value the inverter reports
       // (QPIRI/QBEQI), not the clicked option. POP03/UtS previously looked
       // "selected" in HA while the LCD never changed.
       if (!internal_update_ && this->on_user_select_callback_)
         this->on_user_select_callback_(value);
     }

    void update_state_from_inverter(const std::string &parameter_code) {
      internal_update_ = true;
      int index = -1;
      for (size_t i = 0; i < parameters_.size(); i++) {
        if (param_codes_match_(parameters_[i], parameter_code)) {
          index = static_cast<int>(i);
          break;
        }
      }
      if (index >= 0) {
        if (index < static_cast<int>(options_.size())) {
          this->publish_state(options_[index]);
        } else {
          ESP_LOGW("inverter", "Индекс вне диапазона options");
        }
      } else {
        ESP_LOGW("inverter", "Неизвестный код параметра: %s", parameter_code.c_str());
      }
      internal_update_ = false;
    }

     void set_on_user_select_callback(UserSelectCallback cb) {
       this->on_user_select_callback_ = cb;
     }

    protected:
     static bool param_codes_match_(const std::string &a, const std::string &b) {
       if (a == b)
         return true;
       auto strip_leading_zeros = [](const std::string &s) -> std::string {
         size_t i = 0;
         while (i + 1 < s.size() && s[i] == '0')
           i++;
         return s.substr(i);
       };
       return strip_leading_zeros(a) == strip_leading_zeros(b);
     }

     std::string command_prefix_;
     std::string status_command_;
     std::vector<std::string> parameters_;
     std::vector<std::string> set_commands_;
     std::vector<std::string> options_;
     std::string field_name_;
     bool internal_update_ = false;
     UserSelectCallback on_user_select_callback_;
   };

}  // namespace solar_inverter
}  // namespace esphome
