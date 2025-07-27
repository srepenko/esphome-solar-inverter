#pragma once

#include "esphome/components/switch/switch.h"

namespace esphome {
namespace solar_inverter {

    class InverterSwitch : public switch_::Switch {
        public:
         bool internal_update_{false};
         using CommandCallback = std::function<void(bool)>;
       
         void set_command_callback(CommandCallback cb) { command_callback_ = cb; }
       
         void update_state_from_inverter(bool state) {
           internal_update_ = true;
           this->publish_state(state);
           internal_update_ = false;
         }
       
        protected:
         void write_state(bool state) override {
           // Это вызов при ручном управлении
           if (!internal_update_) {
             this->publish_state(state);
             if (command_callback_) {
               command_callback_(state);  // Отправляем команду наружу
             }
           }
           // Если internal_update_ == true, значит обновление из инвертора — не вызываем callback
         }
       
        private:
         CommandCallback command_callback_;
       };
       

}  // namespace solar_inverter
}  // namespace esphome
