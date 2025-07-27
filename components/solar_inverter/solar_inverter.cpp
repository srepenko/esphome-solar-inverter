// ============================
// File: solar_inverter.cpp
// ============================

#include "solar_inverter.h"
#include "esphome/core/time.h"
#include <sstream>
#include <set>
#include "esphome/core/preferences.h"

namespace esphome {
namespace solar_inverter {

static const char *const TAG = "solar_inverter";

// ────────────────────────────────────────────────────────────────
// setup()
// ────────────────────────────────────────────────────────────────
void SolarInverter::setup() {
  ESP_LOGI(TAG, "Ініціалізація інвертора...");

  poll_commands_ = {
      {"QPIRI", 3000, 0},
      {"QMOD",  3000, 0},
      {"QPIGS", 1000, 0},
      {"QFLAG", 3000, 0},
      {"QPIWS", 1000, 0},
      {"QBEQI", 3000, 0},
  };
  
  send_priority_command("QPI");
  send_priority_command("QID");
  

  ready_ = false;
  current_command_.clear();
  state_ = IDLE;
  poll_index_ = 0;
  
  this->pref_solar_total_ = global_preferences->make_preference<float>(0x6000);
  this->pref_inverter_total_ = global_preferences->make_preference<float>(0x6001);
  this->pref_solar_year_ = global_preferences->make_preference<float>(0x6002);
  this->pref_inverter_year_ = global_preferences->make_preference<float>(0x6003);
  this->pref_solar_month_ = global_preferences->make_preference<float>(0x6004);
  this->pref_inverter_month_ = global_preferences->make_preference<float>(0x6005);
  this->pref_solar_today_ = global_preferences->make_preference<float>(0x6006);
  this->pref_inverter_today_ = global_preferences->make_preference<float>(0x6007);

  load_energy_from_eeprom_();

  setup_qflag_switches();

  this->set_timeout("start_commands", 3000, [this]() { this->ready_ = true; });
}

// ────────────────────────────────────────────────────────────────
// loop()
// ────────────────────────────────────────────────────────────────
void SolarInverter::loop() {
  // ─── UART приём ───
  while (available()) {
    char c = read();
    if (!receiving_) {
      if (c == '(') {
        receiving_ = true;
        rx_buffer_.clear();
        rx_buffer_ += c;
      }
      continue;
    }
    rx_buffer_ += c;
    if (c == '\r') {
      receiving_ = false;
      process_raw_response(rx_buffer_);
      rx_buffer_.clear();
    }
  }

  if (!ready_)
    return;

  // ─── ACK обработан — следующая команда ───
  if (ack_received_) {
    ack_received_ = false;
    next_command_();
  }

  // ─── Таймаут ответа ───
  if (state_ == WAITING_RESPONSE && millis() - last_send_ > RESPONSE_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Таймаут для команди %s", current_command_.c_str());
    state_ = IDLE;
    current_command_.clear();
    next_command_();
  }

  // ─── Обработка очереди результатов (парсинг) ───
  if (!pending_results_.empty()) {
    auto &res = pending_results_.front();
    process_result(res.command, res.payload);
    pending_results_.pop();
  }

  // ─── Публикация QPIGS по частям ───
  if (qpigs_ready_) {
    publish_next_qpigs_chunk_();
  }
  // ─── Публикация QBEQI по частям ───
  if (qbeqi_ready_) {
    publish_next_qbeqi_chunk_();
  }
  // ─── Публикация QPIRI по частям ───
  if (qpiri_ready_) {
    publish_next_qpiri_chunk_();
  }
  // ─── Обновление интеграции энергии и истории ───
  update_energy_history_();

  // ─── Запуск новой команды, если можно ───
  if (state_ == IDLE) {
    next_command_();
  }
}

// ────────────────────────────────────────────────────────────────
// Обновление интеграции энергии и истории
// ────────────────────────────────────────────────────────────────
void SolarInverter::update_energy_history_() {
  static uint32_t last_update = 0;
  static uint32_t last_day = 0, last_month = 0, last_year = 0;
  static uint32_t last_loop_time = 0;
  static uint32_t last_save_time = 0;

  uint32_t now = millis();

  // Обновляем раз в секунду
  if (now - last_update < 1000)
    return;

  last_update = now;

  // Получаем дату (реализуйте get_current_date())
  Date current_date = this->get_current_date();

  // Сброс счётчиков при смене дня/месяца/года
  if (current_date.day != last_day) {
    last_day = current_date.day;
    accumulated_energy_solar_today_ = 0.0f;
    accumulated_energy_inverter_today_ = 0.0f;
    ESP_LOGI(TAG, "Сброс энергии за день");
  }
  if (current_date.month != last_month) {
    last_month = current_date.month;
    accumulated_energy_solar_month_ = 0.0f;
    accumulated_energy_inverter_month_ = 0.0f;
    ESP_LOGI(TAG, "Сброс энергии за месяц");
  }
  if (current_date.year != last_year) {
    last_year = current_date.year;
    accumulated_energy_solar_year_ = 0.0f;
    accumulated_energy_inverter_year_ = 0.0f;
    ESP_LOGI(TAG, "Сброс энергии за год");
  }

  // Интервал в часах для интеграции
  if (last_loop_time == 0) {
    last_loop_time = now;
    return;  // пропускаем первый вызов
  }

  float dt_hours = (now - last_loop_time) / 3600000.0f;
  last_loop_time = now;

  // Интеграция мощности в энергию (кВт·ч)
  if (this->pv_charging_power_sensor_ != nullptr) {
    float pv_power = this->pv_charging_power_sensor_->state;
    if (pv_power >= 0) {
      float energy_kwh = (pv_power / 1000.0f) * dt_hours;
      accumulated_energy_solar_today_ += energy_kwh;
      accumulated_energy_solar_month_ += energy_kwh;
      accumulated_energy_solar_year_ += energy_kwh;
      accumulated_energy_solar_total_ += energy_kwh;
    }
  }

  if (this->output_active_power_sensor_ != nullptr) {
    float inv_power = this->output_active_power_sensor_->state;
    if (inv_power >= 0) {
      float energy_kwh = (inv_power / 1000.0f) * dt_hours;
      accumulated_energy_inverter_today_ += energy_kwh;
      accumulated_energy_inverter_month_ += energy_kwh;
      accumulated_energy_inverter_year_ += energy_kwh;
      accumulated_energy_inverter_total_ += energy_kwh;
    }
  }

  // Публикация накопленных значений в сенсоры
  if (this->energy_solar_today_sensor_ != nullptr)
    this->energy_solar_today_sensor_->publish_state(accumulated_energy_solar_today_);
  if (this->energy_solar_month_sensor_ != nullptr)
    this->energy_solar_month_sensor_->publish_state(accumulated_energy_solar_month_);
  if (this->energy_solar_year_sensor_ != nullptr)
    this->energy_solar_year_sensor_->publish_state(accumulated_energy_solar_year_);
  if (this->energy_solar_total_sensor_ != nullptr)
    this->energy_solar_total_sensor_->publish_state(accumulated_energy_solar_total_);

  if (this->energy_inverter_today_sensor_ != nullptr)
    this->energy_inverter_today_sensor_->publish_state(accumulated_energy_inverter_today_);
  if (this->energy_inverter_month_sensor_ != nullptr)
    this->energy_inverter_month_sensor_->publish_state(accumulated_energy_inverter_month_);
  if (this->energy_inverter_year_sensor_ != nullptr)
    this->energy_inverter_year_sensor_->publish_state(accumulated_energy_inverter_year_);
  if (this->energy_inverter_total_sensor_ != nullptr)
    this->energy_inverter_total_sensor_->publish_state(accumulated_energy_inverter_total_);

  // Сохраняем общий накопленный (total) в EEPROM раз в минуту
  if (now - last_save_time >= 60000) {
    last_save_time = now;
    save_energy_to_eeprom_();
  }
}


void SolarInverter::load_energy_from_eeprom_() {
  pref_solar_total_.load(&accumulated_energy_solar_total_);
  pref_inverter_total_.load(&accumulated_energy_inverter_total_);
  pref_solar_year_.load(&accumulated_energy_solar_year_);
  pref_inverter_year_.load(&accumulated_energy_inverter_year_);
  pref_solar_month_.load(&accumulated_energy_solar_month_);
  pref_inverter_month_.load(&accumulated_energy_inverter_month_);
  pref_solar_today_.load(&accumulated_energy_solar_today_);
  pref_inverter_today_.load(&accumulated_energy_inverter_today_);
  ESP_LOGI(TAG, "Завантажено з NVS: total S=%.2f, I=%.2f", accumulated_energy_solar_total_, accumulated_energy_inverter_total_);
}


void SolarInverter::save_energy_to_eeprom_() {
  pref_solar_total_.save(&accumulated_energy_solar_total_);
  pref_inverter_total_.save(&accumulated_energy_inverter_total_);
  pref_solar_year_.save(&accumulated_energy_solar_year_);
  pref_inverter_year_.save(&accumulated_energy_inverter_year_);
  pref_solar_month_.save(&accumulated_energy_solar_month_);
  pref_inverter_month_.save(&accumulated_energy_inverter_month_);
  pref_solar_today_.save(&accumulated_energy_solar_today_);
  pref_inverter_today_.save(&accumulated_energy_inverter_today_);
  ESP_LOGD(TAG, "Збережено в NVS (S=%.2f/%.2f/%.2f, I=%.2f/%.2f/%.2f)",
           accumulated_energy_solar_today_, accumulated_energy_solar_month_, accumulated_energy_solar_year_,
           accumulated_energy_inverter_today_, accumulated_energy_inverter_month_, accumulated_energy_inverter_year_);
}


Date SolarInverter::get_current_date() {
  time_t now = ::time(nullptr);
  struct tm *timeinfo = ::localtime(&now);

  Date d;
  d.day = timeinfo->tm_mday;
  d.month = timeinfo->tm_mon + 1;
  d.year = timeinfo->tm_year + 1900;
  return d;
}
// ────────────────────────────────────────────────────────────────
// Отправка и планирование команд
// ────────────────────────────────────────────────────────────────
void SolarInverter::send_priority_command(const std::string &cmd) {
  priority_commands_.push(cmd);
}

void SolarInverter::next_command_() {
  if (state_ != IDLE || !current_command_.empty())
    return;

  if (!priority_commands_.empty()) {
    current_command_ = priority_commands_.front();
    priority_commands_.pop();
  } else {
    const size_t sz = poll_commands_.size();
    uint32_t now = millis();
    for (size_t i = 0; i < sz; i++) {
      auto &cmd = poll_commands_[poll_index_];
      poll_index_ = (poll_index_ + 1) % sz;
      if (cmd.last_run_ms == 0 || now - cmd.last_run_ms >= cmd.interval_ms) {
        cmd.last_run_ms = now;
        current_command_ = cmd.command;
        break;
      }
    }
  }

  if (!current_command_.empty()) {
    send_command(current_command_);
    state_ = WAITING_RESPONSE;
  }
}

void SolarInverter::send_command(const std::string &cmd) {
  uint16_t crc = calculate_crc(cmd);
  write_str(cmd.c_str());
  write_byte((crc >> 8) & 0xFF);
  write_byte(crc & 0xFF);
  write_byte('\r');
  last_send_ = millis();
  ESP_LOGD(TAG, "Відправлено команду: %s", cmd.c_str());
}

// ────────────────────────────────────────────────────────────────
// Приём сырых ответов
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_raw_response(const std::string &response) {
  if (!check_crc(response)) {
    ESP_LOGW(TAG, "CRC помилка для [%s]: %s", current_command_.c_str(), response.c_str());
    std::string hex_string;
    for (size_t i = 0; i < response.size(); i++) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%02X ", static_cast<uint8_t>(response[i]));
      hex_string += buf;
    }
    ESP_LOGI(TAG, "Response HEX: %s", hex_string.c_str());
    state_ = IDLE;
    current_command_.clear();
    next_command_();
    return;
  }

  std::string data = response.substr(1, response.length() - 4); // снять '(' и CRC+CR

  if (data == "ACK") {
    ESP_LOGD(TAG, "Отримано ACK для команди [%s]", current_command_.c_str());
    ack_received_ = true;
    state_ = IDLE;
    current_command_.clear();
    return;
  }
  if (data == "NAK") {
    ESP_LOGW(TAG, "Отримано NAK для команди [%s]", current_command_.c_str());
    ack_received_ = true;
    state_ = IDLE;
    current_command_.clear();
    return;
  }

  ESP_LOGD(TAG, "Отримано відповідь для команди [%s]: %s", current_command_.c_str(), data.c_str());

  pending_results_.push({current_command_, data});
  state_ = IDLE;
  current_command_.clear();
}


// ────────────────────────────────────────────────────────────────
// process_result: только сохраняет полезные payload‑ы
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_result(const std::string &command, const std::string &payload) {
  if (command == "QPIGS") {
    last_qpigs_data_ = payload;
    qpigs_publish_index_ = 0;
    qpigs_ready_ = true;
  } else if (command == "QBEQI") {
    last_qbeqi_data_ = payload;
    qbeqi_publish_index_ = 0;
    qbeqi_ready_ = true;
  } else if (command == "QPIRI") {
    last_qpiri_data_ = payload;
    qpiri_publish_index_ = 0;
    qpiri_ready_ = true;
  } else if (command == "QMOD") {
    this->process_qmod_(payload); 
  } else if (command == "QFLAG") {
    this->process_qflag_(payload);
  } else if (command == "QPIWS") {
    if (this->warning_status_text_sensor_)
      this->warning_status_text_sensor_->publish_state(decode_qpiws_(payload));
  }else if (command == "QPI") {
    if (protocol_id_sensor_) protocol_id_sensor_->publish_state(payload);
  } else if (command == "QID") {
    if (serial_number_sensor_) serial_number_sensor_->publish_state(payload);
  } else {
    ESP_LOGD(TAG, "Невідома відповідь [%s]: %s", command.c_str(), payload.c_str());
  }
}

// ────────────────────────────────────────────────────────────────
// Публикация QPIGS по частям (не >1 сенсор за цикл)
// ────────────────────────────────────────────────────────────────
void SolarInverter::publish_next_qpigs_chunk_() {
  if (!qpigs_ready_) return;

  static std::vector<std::string> parts;
  if (qpigs_publish_index_ == 0) {
    parts = split_string(last_qpigs_data_, ' ');
    if (parts.size() < 21) {
      qpigs_ready_ = false;
      return;
    }
  }

  auto publish = [&](sensor::Sensor *s, int idx) {
    float v;
    if (idx < parts.size() && safe_stof(parts[idx], v) && s)
      s->publish_state(v);
  };

  switch (qpigs_publish_index_) {
    case 0:  publish(grid_voltage_sensor_, 0); break;
    case 1:  publish(grid_freq_sensor_, 1); break;
    case 2:  publish(ac_output_voltage_sensor_, 2); break;
    case 3:  publish(ac_output_freq_sensor_, 3); break;
    case 4:  publish(output_apparent_power_sensor_, 4); break;
    case 5:  publish(output_active_power_sensor_, 5); break;
    case 6:  publish(output_load_percent_sensor_, 6); break;
    case 7:  publish(bus_voltage_sensor_, 7); break;
    case 8:  publish(battery_voltage_sensor_, 8); break;
    case 9:  publish(battery_charging_current_sensor_, 9); break;
    case 10: publish(battery_capacity_sensor_, 10); break;
    case 11: publish(inverter_temp_sensor_, 11); break;
    case 12: publish(pv_input_current_sensor_, 12); break;
    case 13: publish(pv_input_voltage_sensor_, 13); break;
    case 14: publish(battery_voltage_from_scc_sensor_, 14); break;
    case 15: publish(battery_discharge_current_sensor_, 15); break;
    case 16: process_qpigs_status_bits_(parts[16]); break;
    case 17: {
      float v; if (safe_stof(parts[17], v) && fan_on_voltage_offset_sensor_) fan_on_voltage_offset_sensor_->publish_state(v*0.01f);
      break;
    }
    case 18: if (eeprom_version_text_) eeprom_version_text_->publish_state(parts[18]); break;
    case 19: publish(pv_charging_power_sensor_, 19); break;
    case 20: process_qpigs_flag_bits_(parts[20]); break;
  }

  qpigs_publish_index_++;
  if (qpigs_publish_index_ >= 21) {
    qpigs_ready_ = false;
    qpigs_publish_index_ = 0;
  }
}

// ────────────────────────────────────────────────────────────────
// Разбор bits b7..b0 (index 16)
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qpigs_status_bits_(const std::string &bits) {
  if (bits.length() != 8) return;  // ожидаем 8 символов 0/1
  auto b = [&](int i) { return bits[7 - i] == '1'; }; // b0 = bits[7]

  if (pv_or_ac_powering_load_) pv_or_ac_powering_load_->publish_state(b(7));
  if (config_changed_)         config_changed_->publish_state(b(6));
  if (scc_fw_updated_)         scc_fw_updated_->publish_state(b(5));
  if (load_on_)                load_on_->publish_state(b(4));
  if (charging_on_)            charging_on_->publish_state(b(2));
  if (scc_charging_on_)        scc_charging_on_->publish_state(b(1));
  if (ac_charging_on_)         ac_charging_on_->publish_state(b(0));

  int mode = (b(2) << 2) | (b(1) << 1) | b(0);
  if (charging_mode_sensor_) charging_mode_sensor_->publish_state(mode);
  if (charging_mode_text_) {
    std::string txt;
    switch (mode) {
      case 0: txt = "No charging"; break;
      case 5: txt = "AC only"; break;
      case 6: txt = "SCC only"; break;
      case 7: txt = "SCC + AC"; break;
      default: txt = "Unknown"; break;
    }
    charging_mode_text_->publish_state(txt);
  }
}

// ────────────────────────────────────────────────────────────────
// Разбор bits b10..b8 (index 20)
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qpigs_flag_bits_(const std::string &bits) {
  // строка может быть, например, "110" или "001" — 3 бита
  if (bits.length() != 3) return;
  bool b10 = bits[0]=='1';
  bool b9  = bits[1]=='1';
  bool b8  = bits[2]=='1';
  if (charging_to_float_)     charging_to_float_->publish_state(b10);
  if (inverter_on_)           inverter_on_->publish_state(b9);
  if (dustproof_installed_)   dustproof_installed_->publish_state(b8);
}

// ────────────────────────────────────────────────────────────────
// Публикация QBEQI по частям (не >1 сенсор за цикл)
// ────────────────────────────────────────────────────────────────
void SolarInverter::publish_next_qbeqi_chunk_() {
  static std::vector<std::string> parts;
  static size_t count = 0;
  if (qbeqi_publish_index_ == 0) {
    parts = split_string(last_qbeqi_data_, ' ');
    count = parts.size();
//    if (parts.size() < 10) {
//      qbeqi_ready_ = false;
//      return;
//    }
  }
  auto to_int = [](const std::string &s) -> int {
    return std::stoi(s);
  };

    auto to_float = [](const std::string &s) -> float {
    return std::stof(s);
  };

  switch (qbeqi_publish_index_) {
    case 0:
      if (equalization_enable_)
        equalization_enable_->update_state_from_inverter(parts[0]);
      break;
    case 1:
      if (equalization_time_)
        equalization_time_->publish_state(to_int(parts[1]));
      break;
    case 2:
      if (equalization_period_)
        equalization_period_->publish_state(to_int(parts[2]));
      break;
    case 3:
      if (equalization_max_current_)
        equalization_max_current_->publish_state(to_int(parts[3]));
      break;
    case 5:
      if (equalization_voltage_)
        equalization_voltage_->publish_state(to_float(parts[5]));
      break;
    case 7:
      if (equalization_over_time_)
        equalization_over_time_->publish_state(to_int(parts[7]));
      break;
    case 8:
      if (equalization_active_)
        equalization_active_->update_state_from_inverter(parts[8]);
      break;
    case 9:
      if (equalization_elapsed_time_)
        equalization_elapsed_time_->publish_state(to_int(parts[9]));
      break;
  }

  qbeqi_publish_index_++;
  if (qbeqi_publish_index_ >= count) {
    qbeqi_ready_ = false;
    qbeqi_publish_index_ = 0;
  }
}

// ────────────────────────────────────────────────────────────────
// Публикация QPIRI по частям (не >1 сенсор за цикл)
// ────────────────────────────────────────────────────────────────
void SolarInverter::publish_sensor(sensor::Sensor *sensor, int index) {
  if (sensor != nullptr && index < this->qpiri_parts_.size()) {
    float v;
    if (safe_stof(this->qpiri_parts_[index], v)) {
      sensor->publish_state(v);
    }
  }
}

void SolarInverter::publish_number(InverterNumber *number, int index) {
  if (number != nullptr && index < this->qpiri_parts_.size()) {
    float v;
    if (!safe_stof(this->qpiri_parts_[index], v)) {return;}
    ESP_LOGD(TAG, "Publishing number: index=%d value=%.2f", index, v);
    number->publish_state(v);
  }
}

void SolarInverter::publish_select(InverterSelect *sel, int index) {
  if (sel != nullptr && index < this->qpiri_parts_.size()) {
    sel->update_state_from_inverter(this->qpiri_parts_[index]);
  }
}


void SolarInverter::publish_next_qpiri_chunk_() {
  if (!qpiri_ready_) return;

  if (qpiri_publish_index_ == 0) {
    this->qpiri_parts_ = split_string(last_qpiri_data_, ' ');
    // Можешь вернуть эту проверку при необходимости:
    // if (qpiri_parts_.size() < 28) {
    //   qpiri_ready_ = false;
    //   return;
    // }
  }

  switch (qpiri_publish_index_) {
    case 0:  publish_sensor(grid_rating_voltage_, 0); break;           // BBB.B  Grid rating voltage V
    case 1:  publish_sensor(grid_rating_current_, 1); break;           // CC.C   Grid rating current A
    case 2:  publish_number(ac_output_rating_voltage_, 2); break;      // DDD.D  (10) AC output rating voltage V
    case 3:  publish_number(ac_output_rating_frequency_, 3); break;    // EE.E   (09) AC output rating frequency Hz
    case 4:  publish_number(ac_output_rating_current_, 4); break;      // FF.F   AC output rating current A
    case 5:  publish_number(ac_output_apparent_power_, 5); break;      // HHHH   AC output rating apparent power VA
    case 6:  publish_number(ac_output_active_power_, 6); break;        // IIII   AC output rating active power W
    case 7:  publish_number(battery_rating_voltage_, 7); break;        // JJ.J   Battery rating voltage V
    case 8:  publish_number(battery_recharge_voltage_, 8); break;      // KK.K   (12) Battery re-charge voltage V
    case 9:  publish_number(battery_undervoltage_, 9); break;          // JJ.J   (29) Battery under voltage V
    case 10: publish_number(battery_bulk_voltage_, 10); break;         // KK.K   (26) Battery bulk voltage V
    case 11: publish_number(battery_float_voltage_, 11); break;        // LL.L   (27) Battery float voltage V
    case 12: publish_select(battery_type_, 12); break;                 // O      (05) Battery type 0: AGM 1: Flooded 2: User 3: LIB 4: LIC 5: LIP 6: LIL
    case 13: publish_number(max_ac_charging_current_, 13); break;      // PPP    (11) Current max AC charging current
    case 14: publish_number(max_charging_current_, 14); break;         // QQ0    (02) Current max charging current
    case 15: publish_select(input_voltage_range_, 15); break;          // O      (03) Input voltage range 0: Appliance  1: UPS
    case 16: publish_select(output_source_priority_, 16); break;       // P      (01) Output source priority 0: UtilitySolarBat 1: SolarUtilityBat 2: SolarBatUtility 3:SolarBatUtility*
    case 17: publish_select(charger_source_priority_, 17); break;      // Q      (16) Charger source priority 1: Solar + Utility (SNU) 2: Only Solar (OSO) 3|0: Solar first (CSO)
    case 18: publish_sensor(parallel_max_number_, 18); break;          // R      Parallel max number 
    case 19: publish_select(machine_type_, 19); break;                 // SS     Machine type 00: Grid tie; 01: Off Grid; 10: Hybrid
    case 20: publish_select(topology_, 20); break;                     // T      Topology 0: transformerless 1: transformer
    case 21: publish_select(output_mode_, 21); break;                  // U      Output mode 
    case 22: publish_number(battery_redischarge_voltage_, 22); break;  // VV.V   (13) Battery re-discharge voltage
    case 23: publish_select(pv_ok_condition_, 23); break;              // W      PV OK condition for parallel
    case 24: publish_select(pv_power_balance_, 24); break;             // X      PV power balance 
    case 25: publish_sensor(neizvestno_, 25); break;                   // X.XX   ()
    case 26: publish_number(grid_tie_current_, 26); break;             // YY     (38) GRID-tie current
    case 27: publish_number(operation_logic_, 27); break;              // Zz.z   ()
  }
  

  qpiri_publish_index_++;
  if (qpiri_publish_index_ >= qpiri_parts_.size()) {
    qpiri_ready_ = false;
    qpiri_publish_index_ = 0;
  }
}


// ────────────────────────────────────────────────────────────────
// Разбор  QMOD<cr>: Device Mode inquiry 
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qmod_(const std::string &payload) {
  char code = payload[0];
  static const std::map<char, const char *> mode_names{
      {'P', "Power On"}, {'S', "Standby"},   {'L', "Line"},
      {'B', "Battery"},  {'F', "Fault"},     {'H', "Power Saving"},
      {'D', "Shutdown"}, {'C', "Charge"},    {'Y', "Bypass"},
      {'E', "ECO"}};
  auto it = mode_names.find(code);
  // device_mode_sensor_ — публикуем просто символ
  if (device_mode_sensor_)
    device_mode_sensor_->publish_state(std::string(1, code));
  // device_mode_text_ — публикуем описание
  if (device_mode_text_) {
    if (it != mode_names.end())
      device_mode_text_->publish_state(it->second);
    else
      device_mode_text_->publish_state("Unknown");
  }
}

// ────────────────────────────────────────────────────────────────
// Разбор  QFLAG<cr>: Device Mode inquiry 
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qflag_(const std::string &payload) {
  if (payload.empty()) {
    ESP_LOGW(TAG, "Empty QFLAG payload");
    return;
  }

  std::set<char> enabled_flags;
  std::set<char> disabled_flags;

  size_t pos = 0;
  while (pos < payload.size()) {
    char state = payload[pos];  // 'E' или 'D'
    ++pos;
    // Читаем все символы пока не встретится 'E' или 'D' или конец строки
    std::string flags;
    while (pos < payload.size() && payload[pos] != 'E' && payload[pos] != 'D') {
      flags += payload[pos];
      ++pos;
    }

    if (state == 'E') {
      enabled_flags.insert(flags.begin(), flags.end());
    } else if (state == 'D') {
      disabled_flags.insert(flags.begin(), flags.end());
    } else {
      ESP_LOGW(TAG, "Unknown state char in QFLAG: %c", state);
    }
  }

  // Инициализация карты (пример)
  std::map<char, InverterSwitch*> flag_map = {
    {'a', buzzer_control_},
    {'b', overload_bypass_},
    {'k', display_escape_to_default_page_},
    {'u', overload_restart_},
    {'v', over_temperature_restart_},
    {'x', backlight_control_},
    {'y', alarm_primary_source_interrupt_},
    {'z', fault_code_record_},
    {'w', power_saving_},
    {'m', data_log_popup_},
    {'d', solar_feed_to_grid_},
    {'g', grid_charge_enable_}
  };

  // Обработка пришедших флагов
  for (const auto &pair : flag_map) {
    if (pair.second != nullptr) {
      bool enabled = enabled_flags.count(pair.first) > 0;
      pair.second->update_state_from_inverter(enabled);  // обновляем состояние без вызова callback

      ESP_LOGD(TAG, "QFLAG: %c = %s", pair.first, enabled ? "ON" : "OFF");
    }
  }
}

void SolarInverter::setup_qflag_switches() {
  // Инициализация карты флагов -> переключателей
  qflag_switches_['a'] = buzzer_control_;
  qflag_switches_['b'] = overload_bypass_;
  qflag_switches_['k'] = display_escape_to_default_page_;
  qflag_switches_['u'] = overload_restart_;
  qflag_switches_['v'] = over_temperature_restart_;
  qflag_switches_['x'] = backlight_control_;
  qflag_switches_['y'] = alarm_primary_source_interrupt_;
  qflag_switches_['z'] = fault_code_record_;
  qflag_switches_['w'] = power_saving_;
  qflag_switches_['m'] = data_log_popup_;
  qflag_switches_['d'] = solar_feed_to_grid_;
  qflag_switches_['g'] = grid_charge_enable_;

  for (auto &pair : qflag_switches_) {
    if (pair.second != nullptr) {
      char flag = pair.first;
      auto *sw = pair.second;

      // Подписка на изменение состояния свитча
      sw->add_on_state_callback([this, flag, sw](bool state) {
        // Отправляем команду ТОЛЬКО если изменение пришло от пользователя,
        // а не из обновления состояния из инвертора
        if (!sw->internal_update_) {
          std::string cmd;

          // Формируем команду по протоколу: "PE" для включения, "PD" для выключения
          cmd = (state ? "PE" : "PD");
          cmd += flag;

          send_priority_command(cmd);
          send_priority_command("QFLAG");
          ESP_LOGD(TAG, "Sent command for flag %c: %s", flag, cmd.c_str());
        }
      });
    }
  }
}

std::string SolarInverter::decode_qpiws_(const std::string &bits) {
  static const char* warning_messages[36] = {
    "Inverter fault / Overcharge current",      // 0 = a0
    "Battery over-temperature",                 // 1
    "Battery under-voltage",                    // 2
    "Battery over-voltage",                     // 3
    "PV input over-voltage",                    // 4
    "Battery temp too low",                     // 5
    "Battery temp too high",                    // 6
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",                       // 7..19
    "PV low loss warning",                      // 20
    "PV derating (high PV)",                    // 21
    "Derating (high temp)",                     // 22
    "Battery temperature low warning",          // 23
    "Battery disconnect",                       // 24
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",          // 25..29
    "Battery low warning",                      // 30
    "Load short circuit fault",                 // 31
    "DSP communication fault"                   // 32
    // Bits 33, 34, 35 — можно также добавить при необходимости
  };

  std::vector<std::string> active_warnings;
  for (size_t i = 0; i < bits.size() && i < 36; ++i) {
    if (bits[i] == '1') {
      active_warnings.push_back(warning_messages[i]);
    }
  }

  if (active_warnings.empty())
    return "No warnings";

  std::string result;
  for (const auto &msg : active_warnings) {
    if (!result.empty())
      result += ", ";
    result += msg;
  }
  return result;
}

// ────────────────────────────────────────────────────────────────
// CRC utils & helpers                                            
// ────────────────────────────────────────────────────────────────
uint16_t SolarInverter::calculate_crc(const std::string &cmd) {
  return cal_crc_half(reinterpret_cast<const uint8_t *>(cmd.data()), cmd.length());
}

uint16_t SolarInverter::cal_crc_half(const uint8_t* data, size_t len) {
  static const uint16_t crc_ta[16] = {
      0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
      0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
  };
  uint16_t crc = 0;
  size_t pos = 0;
  if (len == 0) return 0;
  while (len-- != 0) {
      uint8_t da = (crc >> 12) & 0x0F;
      crc <<= 4;
      crc ^= crc_ta[da ^ (data[pos] >> 4)];
      da = (crc >> 12) & 0x0F;
      crc <<= 4;
      crc ^= crc_ta[da ^ (data[pos] & 0x0F)];
      pos++;
  }
  uint8_t bCRCLow = crc & 0xFF;
  uint8_t bCRCHigh = (crc >> 8) & 0xFF;
  if (bCRCLow == 0x28 || bCRCLow == 0x0d || bCRCLow == 0x0a) bCRCLow++;
  if (bCRCHigh == 0x28 || bCRCHigh == 0x0d || bCRCHigh == 0x0a) bCRCHigh++;
  crc = (bCRCHigh << 8) | bCRCLow;
  return crc;
}

bool SolarInverter::check_crc(const std::string &response) {
  if (response.size() < 3) return false;
  const uint8_t *data = reinterpret_cast<const uint8_t *>(response.data());
  uint16_t received = (data[response.size()-3] << 8) | data[response.size()-2];
  uint16_t calc = cal_crc_half(data, response.size()-3);
  return received == calc;
}

std::vector<std::string> SolarInverter::split_string(const std::string &s, char delimiter) {
  std::vector<std::string> out; std::string tmp; std::istringstream ss(s);
  while (std::getline(ss, tmp, delimiter)) out.push_back(tmp);
  return out;
}

bool SolarInverter::safe_stof(const std::string &s, float &v) {
  if (s.empty()) return false;
  char *end = nullptr;
  v = strtof(s.c_str(), &end);
  return end != s.c_str() && *end == '\0';
}

void SolarInverter::set_flag(char flag, bool enabled) {
  std::string cmd = (enabled ? "PE" : "PD");
  cmd += flag;
  send_command(cmd);
}



void SolarInverter::add_inverter_select(int index, InverterSelect *sel) {
//  this->inverter_selects_by_index_[index] = sel;  // сохраняем по индексу

  const std::string &prefix = sel->get_command_prefix();
  const auto &params = sel->get_parameters();
  const auto &options = sel->get_options_list();
  const std::string &field_name = sel->get_field_name();

  sel->set_on_user_select_callback([this, prefix, params, options, field_name](const std::string &value) {
    auto it = std::find(options.begin(), options.end(), value);
    if (it != options.end()) {
      int idx = std::distance(options.begin(), it);
      if (idx >= 0 && idx < static_cast<int>(params.size())) {
        std::string command = prefix + params[idx];
        this->send_command(command);
        ESP_LOGD(TAG, "Select '%s': '%s' -> '%s'", field_name.c_str(), value.c_str(), command.c_str());
      } else {
        ESP_LOGW(TAG, "Index %d out of range for select '%s'", idx, field_name.c_str());
      }
    } else {
      ESP_LOGW(TAG, "Value '%s' not found in options for '%s'", value.c_str(), field_name.c_str());
    }
  });
}


}  // namespace solar_inverter
}  // namespace esphome