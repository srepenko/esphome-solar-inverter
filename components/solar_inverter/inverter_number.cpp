#include "inverter_number.h"
#include "solar_inverter.h"
#include <cstdio>  // sprintf
#include <string>
#include <cmath>

namespace esphome {
namespace solar_inverter {

    void InverterNumber::control(float value) {
        if (this->is_from_inverter_)
          return;

        // Probe NN and similar: update HA state only, never UART.
        if (this->local_only_) {
          this->publish_state(value);
          return;
        }
      
        if (this->parent_ != nullptr && this->parent_->soc_thresholds() && this->parent_->is_soc_threshold_number(this)) {
          ESP_LOGW("inverter_number", "АКБ літієва зі зв'язком: пороги 12/13 задаються на екрані у %% SOC — команду %s не надіслано",
                   this->cmd_prefix_.c_str());
          this->publish_state(NAN);
          return;
        }
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
