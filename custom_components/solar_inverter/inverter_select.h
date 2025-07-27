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
   
     void set_parameters(const std::vector<std::string> &params) { this->parameters_ = params; }
     const std::vector<std::string> &get_parameters() const { return this->parameters_; }
   
     void set_field_name(const std::string &name) { this->field_name_ = name; }
     const std::string &get_field_name() const { return this->field_name_; }
   
     void set_options_list(const std::vector<std::string> &opts) { this->options_ = opts; }
     const std::vector<std::string> &get_options_list() const { return options_; }
   
     void control(const std::string &value) override {
       if (!internal_update_ && this->on_user_select_callback_)
         this->on_user_select_callback_(value);
       this->publish_state(value);
     }
   
    void update_state_from_inverter(const std::string &parameter_code) {
      internal_update_ = true;  // чтобы не вызвать callback пользователя при обновлении из инвертора
      auto it = std::find(parameters_.begin(), parameters_.end(), parameter_code);
      if (it != parameters_.end()) {
        int index = std::distance(parameters_.begin(), it);
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
     std::string command_prefix_;
     std::vector<std::string> parameters_;
     std::vector<std::string> options_;
     std::string field_name_;
     bool internal_update_ = false;
     UserSelectCallback on_user_select_callback_;
   };

}  // namespace solar_inverter
}  // namespace esphome
