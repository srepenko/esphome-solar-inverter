#include "inverter_number.h"
#include "solar_inverter.h"
#include <cstdio>  // sprintf
#include <string>

namespace esphome {
namespace solar_inverter {

    void InverterNumber::control(float value) {
        if (this->is_from_inverter_)
          return;
      
        if (this->parent_ != nullptr) {
          if (this->format_.empty()) {
            ESP_LOGW("inverter_number", "Пустой формат — команда не отправлена");
            return;
          }
      
          char val_buf[16] = {0};
          int ret = snprintf(val_buf, sizeof(val_buf), this->format_.c_str(), value);
      
          if (ret < 0 || ret >= (int)sizeof(val_buf)) {
            ESP_LOGW("inverter_number", "Ошибка форматирования команды: формат='%s', значение=%.2f", this->format_.c_str(), value);
            return;
          }
      
          std::string cmd = this->cmd_prefix_ + std::string(val_buf);
          ESP_LOGD("inverter_number", "Отправка команды: %s", cmd.c_str());
      
          this->parent_->send_priority_command(cmd);
        }
      
        this->publish_state(value);
      }

      
}  // namespace solar_inverter
}  // namespace esphome
