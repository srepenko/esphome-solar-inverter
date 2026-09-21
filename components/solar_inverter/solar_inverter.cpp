// ============================
// File: solar_inverter.cpp
// ============================

#include "solar_inverter.h"
#include "inverter_inquiry_button.h"
#include "esphome/core/time.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <map>
#include "esphome/core/preferences.h"

namespace esphome {
namespace solar_inverter {

static const char *const TAG = "solar_inverter";

// ────────────────────────────────────────────────────────────────
// setup()
// ────────────────────────────────────────────────────────────────
void SolarInverter::setup() {
  ESP_LOGI(TAG, "Ініціалізація інвертора...");

  const uint32_t now = millis();
  // last_run_ms = now → не слати всі inquiry одразу після ready_ (інакше черга
  // QPIGS/QFLAG/QPIRI/QBEQI роз'їжджається і відповідь QPIGS чіпляється до QFLAG).
  poll_commands_ = {
      {"QPIGS", 1000, now, 1000, 0, false},
      {"QMOD", 3000, now, 3000, 0, false},
      {"QPIWS", 2000, now, 2000, 0, false},
      {"QFLAG", 5000, now, 5000, 0, false},
      {"QPIRI", 5000, now, 5000, 0, false},
  };
  if (this->has_qbeqi_entities_()) {
    poll_commands_.push_back({"QBEQI", 15000, now, 15000, 0, false});
    ESP_LOGI(TAG, "QBEQI polling enabled (equalization entities present)");
  } else {
    ESP_LOGI(TAG, "QBEQI polling skipped (no equalization entities in YAML)");
  }

  send_priority_command("QPI");
  send_priority_command("QID");

  ready_ = false;
  current_command_.clear();
  state_ = IDLE;
  poll_index_ = 0;
  next_send_allowed_ms_ = 0;
  receiving_ = false;
  rx_buffer_.clear();
  
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

  if (this->debug_probe_nn_ != nullptr) {
    // Default NN=3 (POP03 / PCP03 probe interest); HA box can change 0–99.
    this->debug_probe_nn_->publish_state(3);
  }

  this->set_timeout("start_commands", 3000, [this]() { this->ready_ = true; });
}

// ────────────────────────────────────────────────────────────────
// loop()
// ────────────────────────────────────────────────────────────────
void SolarInverter::loop() {
  const uint32_t loop_start = millis();

  // ─── UART приём: queue one complete frame, do NOT CRC/parse here ───
  while (available() && !raw_frame_pending_) {
    const char c = static_cast<char>(read());
    if (!receiving_) {
      if (c == '(') {
        receiving_ = true;
        rx_buffer_.clear();
        rx_buffer_ += c;
      }
    } else {
      rx_buffer_ += c;
      if (rx_buffer_.size() > MAX_RX_FRAME) {
        ESP_LOGW(TAG, "UART RX overflow (%u), discarding", (unsigned) rx_buffer_.size());
        rx_buffer_.clear();
        receiving_ = false;
      } else if (c == '\r') {
        receiving_ = false;
        pending_raw_frame_.swap(rx_buffer_);
        rx_buffer_.clear();
        raw_frame_pending_ = true;
        // CRC + parse next tick — stay under ESPHome ~50 ms budget
        return;
      }
    }
    if (millis() - loop_start >= MAX_LOOP_MS)
      return;
  }

  if (!ready_)
    return;

  // ─── CRC / ACK-NAK / queue payload (one frame per tick) ───
  if (raw_frame_pending_) {
    raw_frame_pending_ = false;
    this->process_raw_response(pending_raw_frame_);
    pending_raw_frame_.clear();
    return;
  }

  // ─── Таймаут відповіді ───
  if (state_ == WAITING_RESPONSE &&
      millis() - last_send_ > this->response_timeout_for_(current_command_)) {
    ESP_LOGW(TAG, "Таймаут для команди %s", current_command_.c_str());
    if (set_probe_pending_ && current_command_ == set_probe_command_) {
      this->finish_set_probe_(current_command_, "", "TIMEOUT", true);
    } else {
      this->publish_debug_(current_command_, "", "TIMEOUT");
      this->apply_nak_backoff_(current_command_);
      this->flush_rx_();
      this->finish_command_(POST_ERROR_SETTLE_MS);
    }
    return;
  }

  if (millis() - loop_start >= MAX_LOOP_MS)
    return;

  // ─── Parse one queued inquiry result ───
  if (!pending_results_.empty()) {
    const PendingResult res = pending_results_.front();
    pending_results_.pop();
    this->process_result(res.command, res.payload);
    return;
  }

  if (millis() - loop_start >= MAX_LOOP_MS)
    return;

  // ─── Publish at most one entity/chunk (never combine with TX) ───
  if (qpigs_ready_) {
    this->publish_next_qpigs_chunk_();
    return;
  }
  if (qbeqi_ready_) {
    this->publish_next_qbeqi_chunk_();
    return;
  }
  if (qpiri_ready_) {
    this->publish_next_qpiri_chunk_();
    return;
  }
  if (qflag_ready_) {
    this->publish_next_qflag_chunk_();
    return;
  }

  // Energy integrate/publish only when UART path is idle (no flash here).
  if (!this->uart_busy_() && millis() - loop_start < MAX_LOOP_MS)
    this->update_energy_history_();

  // ─── Наступна команда тільки в IDLE і після паузи ───
  if (state_ == IDLE && static_cast<int32_t>(millis() - next_send_allowed_ms_) >= 0 &&
      millis() - loop_start < MAX_LOOP_MS && !energy_save_pending_) {
    this->next_command_();
  }
}

bool SolarInverter::uart_busy_() const {
  return state_ != IDLE || raw_frame_pending_ || !pending_results_.empty() || qpigs_ready_ ||
         qbeqi_ready_ || qpiri_ready_ || qflag_ready_ || set_probe_pending_;
}

// ────────────────────────────────────────────────────────────────
// Обновление интеграции энергии и истории
// ────────────────────────────────────────────────────────────────
void SolarInverter::update_energy_history_() {
  const uint32_t now = millis();

  // Integrate at most once per second; publish at most one energy sensor per call.
  if (now - last_energy_update_ms_ < 1000 && energy_publish_index_ == 0)
    return;

  if (energy_publish_index_ == 0) {
    last_energy_update_ms_ = now;
    Date current_date = this->get_current_date();

    if (current_date.day != last_energy_day_) {
      last_energy_day_ = current_date.day;
      accumulated_energy_solar_today_ = 0.0f;
      accumulated_energy_inverter_today_ = 0.0f;
    }
    if (current_date.month != last_energy_month_) {
      last_energy_month_ = current_date.month;
      accumulated_energy_solar_month_ = 0.0f;
      accumulated_energy_inverter_month_ = 0.0f;
    }
    if (current_date.year != last_energy_year_) {
      last_energy_year_ = current_date.year;
      accumulated_energy_solar_year_ = 0.0f;
      accumulated_energy_inverter_year_ = 0.0f;
    }

    if (last_energy_loop_ms_ == 0) {
      last_energy_loop_ms_ = now;
      return;
    }

    const float dt_hours = (now - last_energy_loop_ms_) / 3600000.0f;
    last_energy_loop_ms_ = now;

    if (this->pv_charging_power_sensor_ != nullptr) {
      const float pv_power = this->pv_charging_power_sensor_->state;
      if (pv_power >= 0) {
        const float energy_kwh = (pv_power / 1000.0f) * dt_hours;
        accumulated_energy_solar_today_ += energy_kwh;
        accumulated_energy_solar_month_ += energy_kwh;
        accumulated_energy_solar_year_ += energy_kwh;
        accumulated_energy_solar_total_ += energy_kwh;
      }
    }

    if (this->output_active_power_sensor_ != nullptr) {
      const float inv_power = this->output_active_power_sensor_->state;
      if (inv_power >= 0) {
        const float energy_kwh = (inv_power / 1000.0f) * dt_hours;
        accumulated_energy_inverter_today_ += energy_kwh;
        accumulated_energy_inverter_month_ += energy_kwh;
        accumulated_energy_inverter_year_ += energy_kwh;
        accumulated_energy_inverter_total_ += energy_kwh;
      }
    }

    // Flash is deferred — never block UART loop on NVS.
    if (now - last_energy_save_ms_ >= 60000) {
      last_energy_save_ms_ = now;
      this->schedule_energy_save_();
    }
    energy_publish_index_ = 1;
    return;  // publish sensors on following idle ticks
  }

  // One HA publish per tick (8 energy sensors total).
  switch (energy_publish_index_) {
    case 1:
      if (energy_solar_today_sensor_)
        energy_solar_today_sensor_->publish_state(accumulated_energy_solar_today_);
      break;
    case 2:
      if (energy_solar_month_sensor_)
        energy_solar_month_sensor_->publish_state(accumulated_energy_solar_month_);
      break;
    case 3:
      if (energy_solar_year_sensor_)
        energy_solar_year_sensor_->publish_state(accumulated_energy_solar_year_);
      break;
    case 4:
      if (energy_solar_total_sensor_)
        energy_solar_total_sensor_->publish_state(accumulated_energy_solar_total_);
      break;
    case 5:
      if (energy_inverter_today_sensor_)
        energy_inverter_today_sensor_->publish_state(accumulated_energy_inverter_today_);
      break;
    case 6:
      if (energy_inverter_month_sensor_)
        energy_inverter_month_sensor_->publish_state(accumulated_energy_inverter_month_);
      break;
    case 7:
      if (energy_inverter_year_sensor_)
        energy_inverter_year_sensor_->publish_state(accumulated_energy_inverter_year_);
      break;
    case 8:
      if (energy_inverter_total_sensor_)
        energy_inverter_total_sensor_->publish_state(accumulated_energy_inverter_total_);
      break;
    default:
      energy_publish_index_ = 0;
      return;
  }
  energy_publish_index_++;
  if (energy_publish_index_ > 8)
    energy_publish_index_ = 0;
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
  // Kept for compatibility; prefer schedule_energy_save_() (one NVS write per step).
  this->schedule_energy_save_();
}

void SolarInverter::schedule_energy_save_() {
  if (energy_save_pending_)
    return;
  energy_save_pending_ = true;
  energy_save_index_ = 0;
  this->set_timeout("energy_save_step", ENERGY_SAVE_STEP_MS, [this]() { this->save_energy_step_(); });
}

void SolarInverter::save_energy_step_() {
  // One preference write per callback — avoids 80+ ms preferences stalls.
  switch (energy_save_index_) {
    case 0:
      pref_solar_total_.save(&accumulated_energy_solar_total_);
      break;
    case 1:
      pref_inverter_total_.save(&accumulated_energy_inverter_total_);
      break;
    case 2:
      pref_solar_year_.save(&accumulated_energy_solar_year_);
      break;
    case 3:
      pref_inverter_year_.save(&accumulated_energy_inverter_year_);
      break;
    case 4:
      pref_solar_month_.save(&accumulated_energy_solar_month_);
      break;
    case 5:
      pref_inverter_month_.save(&accumulated_energy_inverter_month_);
      break;
    case 6:
      pref_solar_today_.save(&accumulated_energy_solar_today_);
      break;
    case 7:
      pref_inverter_today_.save(&accumulated_energy_inverter_today_);
      break;
    default:
      energy_save_pending_ = false;
      energy_save_index_ = 0;
      return;
  }
  energy_save_index_++;
  if (energy_save_index_ < 8) {
    this->set_timeout("energy_save_step", ENERGY_SAVE_STEP_MS, [this]() { this->save_energy_step_(); });
  } else {
    energy_save_pending_ = false;
    energy_save_index_ = 0;
    ESP_LOGD(TAG, "Energy prefs saved (8 stepped NVS writes)");
  }
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
  if (priority_commands_.size() >= MAX_PRIORITY_QUEUE) {
    ESP_LOGW(TAG, "UART queue full (%u), dropped '%s'", (unsigned) priority_commands_.size(),
             cmd.c_str());
    return;
  }
  priority_commands_.push(cmd);
}

void SolarInverter::next_command_() {
  if (state_ != IDLE || !current_command_.empty())
    return;
  if (static_cast<int32_t>(millis() - next_send_allowed_ms_) < 0)
    return;

  if (!priority_commands_.empty()) {
    current_command_ = priority_commands_.front();
    priority_commands_.pop();
  } else {
    const size_t sz = poll_commands_.size();
    if (sz == 0)
      return;
    const uint32_t now = millis();
    for (size_t i = 0; i < sz; i++) {
      auto &cmd = poll_commands_[poll_index_];
      poll_index_ = (poll_index_ + 1) % sz;
      if (cmd.poll_disabled)
        continue;
      if (cmd.last_run_ms == 0 || now - cmd.last_run_ms >= cmd.interval_ms) {
        cmd.last_run_ms = now;
        current_command_ = cmd.command;
        break;
      }
    }
  }

  if (!current_command_.empty()) {
    this->send_command(current_command_);
    state_ = WAITING_RESPONSE;
  }
}

void SolarInverter::flush_rx_() {
  uint16_t n = 0;
  while (available()) {
    (void) read();
    if (++n > 512)
      break;
  }
  if (n > 0)
    ESP_LOGD(TAG, "Flushed %u leftover UART byte(s)", (unsigned) n);
  rx_buffer_.clear();
  receiving_ = false;
  raw_frame_pending_ = false;
  pending_raw_frame_.clear();
}

void SolarInverter::finish_command_(uint32_t settle_ms) {
  state_ = IDLE;
  current_command_.clear();
  next_send_allowed_ms_ = millis() + settle_ms;
}

bool SolarInverter::has_qbeqi_entities_() const {
  return equalization_enable_ != nullptr || equalization_voltage_ != nullptr ||
         equalization_over_time_ != nullptr || equalization_time_ != nullptr ||
         equalization_period_ != nullptr || equalization_active_ != nullptr ||
         equalization_max_current_ != nullptr || equalization_elapsed_time_ != nullptr;
}

CommandEntry *SolarInverter::find_poll_command_(const std::string &command) {
  for (auto &cmd : poll_commands_) {
    if (cmd.command == command)
      return &cmd;
  }
  return nullptr;
}

bool SolarInverter::uses_poll_nak_backoff_(const std::string &command) {
  return command == "QPIRI" || command == "QPIGS" || command == "QPIGS2" || command == "QPIWS";
}

uint32_t SolarInverter::response_timeout_for_(const std::string &command) const {
  // Довгі status-рядки на 2400 baud потребують більше часу на RX.
  if (command == "QPIGS" || command == "QPIGS2" || command == "QPIRI")
    return RESPONSE_TIMEOUT_LONG_MS;
  return RESPONSE_TIMEOUT_MS;
}

void SolarInverter::apply_nak_backoff_(const std::string &command) {
  CommandEntry *entry = this->find_poll_command_(command);
  if (entry == nullptr)
    return;
  if (entry->consecutive_naks < 255)
    entry->consecutive_naks++;

  if (command == "QBEQI") {
    if (entry->consecutive_naks >= QBEQI_NAK_DISABLE_AFTER) {
      entry->poll_disabled = true;
      ESP_LOGW(TAG,
               "QBEQI постійний NAK (%u) — опитування вимкнено (немає equalization?). "
               "Debug QBEQI лишається; сутності 32–37 можна закоментувати в YAML.",
               (unsigned) entry->consecutive_naks);
    } else {
      entry->interval_ms = 30000;
      ESP_LOGW(TAG, "QBEQI NAK (%u/%u) — наступне опитування через 30 с",
               (unsigned) entry->consecutive_naks, (unsigned) QBEQI_NAK_DISABLE_AFTER);
    }
    return;
  }

  if (uses_poll_nak_backoff_(command)) {
    uint32_t next = entry->interval_ms < POLL_NAK_INTERVAL_MIN_MS ? POLL_NAK_INTERVAL_MIN_MS
                                                                  : entry->interval_ms * 2;
    if (next > POLL_NAK_INTERVAL_MAX_MS)
      next = POLL_NAK_INTERVAL_MAX_MS;
    entry->interval_ms = next;
    ESP_LOGW(TAG, "%s NAK/timeout — повтор через %u мс, без флуду", command.c_str(),
             (unsigned) next);
  }
}

void SolarInverter::apply_inquiry_success_(const std::string &command) {
  CommandEntry *entry = this->find_poll_command_(command);
  if (entry == nullptr)
    return;
  entry->consecutive_naks = 0;
  if (entry->base_interval_ms != 0)
    entry->interval_ms = entry->base_interval_ms;
  if (entry->poll_disabled) {
    entry->poll_disabled = false;
    ESP_LOGI(TAG, "%s succeeded — polling re-enabled", command.c_str());
  }
}

bool SolarInverter::looks_like_status_line_(const std::string &payload) {
  // QPIGS / QPIRI: "222.5 49.9 230.3 ..."
  if (payload.size() < 8)
    return false;
  const unsigned char c0 = static_cast<unsigned char>(payload[0]);
  if (!std::isdigit(c0) && payload[0] != '-' && payload[0] != ' ')
    return false;
  return payload.find(' ') != std::string::npos && payload.find('.') != std::string::npos;
}

bool SolarInverter::payload_matches_command_(const std::string &command,
                                            const std::string &payload) const {
  if (payload == "ACK" || payload == "NAK")
    return true;

  const bool status_line = looks_like_status_line_(payload);

  if (command == "QFLAG") {
    if (payload.empty() || status_line)
      return false;
    return payload[0] == 'E' || payload[0] == 'D';
  }
  if (command == "QMOD")
    return payload.size() == 1 && std::isalpha(static_cast<unsigned char>(payload[0]));
  if (command == "QPIWS")
    return !payload.empty() && !status_line && (payload[0] == '0' || payload[0] == '1');
  if (command == "QPIGS" || command == "QPIGS2" || command == "QPIRI")
    return status_line;
  if (command == "QBEQI") {
    // "0 060 030 … 56.40 …" — перше поле 0/1, не напруга мережі QPIGS ("222.5").
    const size_t sp = payload.find(' ');
    if (sp == std::string::npos)
      return false;
    const std::string first = payload.substr(0, sp);
    return first == "0" || first == "1";
  }
  return true;
}

void SolarInverter::send_command(const std::string &cmd) {
  // Злити хвіст попередньої відповіді ДО TX, інакше він приліпиться до нової команди.
  this->flush_rx_();
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
  if (state_ != WAITING_RESPONSE || current_command_.empty()) {
    ESP_LOGD(TAG, "Відкинуто зайвий UART-кадр (немає in-flight команди)");
    return;
  }

  const std::string waiting = current_command_;

  if (response.size() < 5) {
    ESP_LOGW(TAG, "Короткий UART-кадр для [%s], відкинуто", waiting.c_str());
    return;
  }

  if (!check_crc(response)) {
    ESP_LOGW(TAG, "CRC помилка для [%s] (кадр відкинуто, чекаємо далі): %s", waiting.c_str(),
             response.c_str());
    // For SET probes, surface CRC_ERROR on HA while still waiting for a valid ACK/NAK/timeout.
    if (set_probe_pending_ && waiting == set_probe_command_) {
      if (this->debug_last_command_)
        this->debug_last_command_->publish_state(waiting);
      if (this->debug_last_response_)
        this->debug_last_response_->publish_state(response);
      if (this->debug_last_result_)
        this->debug_last_result_->publish_state("CRC_ERROR");
      ESP_LOGI(TAG, "DEBUG_SET_PROBE cmd=%s result=CRC_ERROR resp=%s", waiting.c_str(),
               response.c_str());
    }
    // Не переходимо до наступної команди — це часто хвіст QPIGS, не відповідь QFLAG.
    return;
  }

  std::string data = response.substr(1, response.length() - 4);  // зняти '(' і CRC+CR

  if (!this->payload_matches_command_(waiting, data)) {
    ESP_LOGW(TAG, "Відкинуто чужий кадр, очікуємо [%s]: %s", waiting.c_str(), data.c_str());
    return;
  }

  if (data == "ACK") {
    ESP_LOGI(TAG, "ACK для [%s]", waiting.c_str());
    if (set_probe_pending_ && waiting == set_probe_command_) {
      this->finish_set_probe_(waiting, "(ACK", "ACK", true);
      return;
    }
    this->publish_debug_(waiting, "(ACK", "ACK");
    this->finish_command_(INTER_COMMAND_GAP_MS);
    return;
  }
  if (data == "NAK") {
    ESP_LOGW(TAG, "NAK для [%s]", waiting.c_str());
    if (set_probe_pending_ && waiting == set_probe_command_) {
      this->finish_set_probe_(waiting, "(NAK", "NAK", true);
      return;
    }
    this->publish_debug_(waiting, "(NAK", "NAK");
    this->apply_nak_backoff_(waiting);
    this->finish_command_(POST_ERROR_SETTLE_MS);
    return;
  }

  ESP_LOGI(TAG, "DEBUG_INQUIRY [%s] -> %s", waiting.c_str(), data.c_str());
  this->publish_debug_(waiting, data, "DATA");
  this->apply_inquiry_success_(waiting);
  pending_results_.push({waiting, data});
  this->finish_command_(INTER_COMMAND_GAP_MS);
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
    this->process_qflag_parse_(payload);
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

  if (qpigs_publish_index_ == 0) {
    qpigs_parts_ = split_string(last_qpigs_data_, ' ');
    if (qpigs_parts_.size() < 21) {
      qpigs_ready_ = false;
      return;
    }
  }

  auto publish = [&](sensor::Sensor *s, int idx) {
    float v;
    if (idx < (int) qpigs_parts_.size() && safe_stof(qpigs_parts_[idx], v) && s)
      s->publish_state(v);
  };

  // Indices 0–15: one float sensor; 16–17: status bits split; 18–21: remaining fields.
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
    case 16: process_qpigs_status_bits_part_(qpigs_parts_[16], 0); break;
    case 17: process_qpigs_status_bits_part_(qpigs_parts_[16], 1); break;
    case 18: {
      float v;
      if (safe_stof(qpigs_parts_[17], v) && fan_on_voltage_offset_sensor_)
        fan_on_voltage_offset_sensor_->publish_state(v * 0.01f);
      break;
    }
    case 19:
      if (eeprom_version_text_)
        eeprom_version_text_->publish_state(qpigs_parts_[18]);
      break;
    case 20: publish(pv_charging_power_sensor_, 19); break;
    case 21: process_qpigs_flag_bits_(qpigs_parts_[20]); break;
  }

  qpigs_publish_index_++;
  if (qpigs_publish_index_ >= 22) {
    qpigs_ready_ = false;
    qpigs_publish_index_ = 0;
  }
}

// ────────────────────────────────────────────────────────────────
// Разбор bits b7..b0 (index 16) — split across two ticks
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qpigs_status_bits_part_(const std::string &bits, uint8_t part) {
  if (bits.length() != 8) return;
  auto b = [&](int i) { return bits[7 - i] == '1'; };

  if (part == 0) {
    if (pv_or_ac_powering_load_) pv_or_ac_powering_load_->publish_state(b(7));
    if (config_changed_)         config_changed_->publish_state(b(6));
    if (scc_fw_updated_)         scc_fw_updated_->publish_state(b(5));
    if (load_on_)                load_on_->publish_state(b(4));
    return;
  }

  if (charging_on_)     charging_on_->publish_state(b(2));
  if (scc_charging_on_) scc_charging_on_->publish_state(b(1));
  if (ac_charging_on_)  ac_charging_on_->publish_state(b(0));

  const int mode = (b(2) << 2) | (b(1) << 1) | b(0);
  if (charging_mode_sensor_) charging_mode_sensor_->publish_state(mode);
  if (charging_mode_text_) {
    const char *txt = "Unknown";
    switch (mode) {
      case 0: txt = "No charging"; break;
      case 5: txt = "AC only"; break;
      case 6: txt = "SCC only"; break;
      case 7: txt = "SCC + AC"; break;
    }
    charging_mode_text_->publish_state(txt);
  }
}

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
  if (qbeqi_publish_index_ == 0) {
    qbeqi_parts_ = split_string(last_qbeqi_data_, ' ');
  }
  auto &parts = qbeqi_parts_;
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
  if (qbeqi_publish_index_ >= qbeqi_parts_.size()) {
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
    case 16: publish_select(output_source_priority_, 16); break;
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
    case 28:  // Program 01 text sensors (after all numeric/select fields)
             if (16 < this->qpiri_parts_.size())
               this->publish_output_source_priority_(this->qpiri_parts_[16]);
             break;
  }
  

  qpiri_publish_index_++;
  if (qpiri_publish_index_ >= std::max(qpiri_parts_.size(), size_t{29})) {
    qpiri_ready_ = false;
    qpiri_publish_index_ = 0;
  }
}


// ────────────────────────────────────────────────────────────────
// Разбор  QMOD<cr>: Device Mode inquiry 
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qmod_(const std::string &payload) {
  if (payload.empty()) {
    ESP_LOGW(TAG, "Empty QMOD payload");
    return;
  }
  const char code = payload[0];
  const char *name = "Unknown";
  switch (code) {
    case 'P': name = "Power On"; break;
    case 'S': name = "Standby"; break;
    case 'L': name = "Line"; break;
    case 'B': name = "Battery"; break;
    case 'F': name = "Fault"; break;
    case 'H': name = "Power Saving"; break;
    case 'D': name = "Shutdown"; break;
    case 'C': name = "Charge"; break;
    case 'Y': name = "Bypass"; break;
    case 'E': name = "ECO"; break;
  }
  if (device_mode_sensor_) {
    char buf[2] = {code, 0};
    device_mode_sensor_->publish_state(buf);
  }
  if (device_mode_text_)
    device_mode_text_->publish_state(name);
}

// ────────────────────────────────────────────────────────────────
// QFLAG: cheap parse once, publish one switch per tick
// ────────────────────────────────────────────────────────────────
void SolarInverter::process_qflag_parse_(const std::string &payload) {
  if (payload.empty()) {
    ESP_LOGW(TAG, "Empty QFLAG payload");
    return;
  }
  if (looks_like_status_line_(payload) || (payload[0] != 'E' && payload[0] != 'D')) {
    ESP_LOGW(TAG, "Ignoring non-QFLAG payload: %s", payload.c_str());
    return;
  }

  memset(qflag_on_, 0, sizeof(qflag_on_));
  char mode = 0;
  for (unsigned char c : payload) {
    if (c == 'E' || c == 'D') {
      mode = static_cast<char>(c);
      continue;
    }
    if (mode == 'E')
      qflag_on_[c] = true;
    else if (mode == 'D')
      qflag_on_[c] = false;
  }

  last_qflag_data_ = payload;
  qflag_publish_index_ = 0;
  qflag_ready_ = true;
}

void SolarInverter::publish_next_qflag_chunk_() {
  if (!qflag_ready_)
    return;

  static const char kOrder[] = {'a', 'b', 'd', 'g', 'k', 'm', 'u', 'v', 'w', 'x', 'y', 'z'};
  if (qflag_publish_index_ >= sizeof(kOrder)) {
    qflag_ready_ = false;
    qflag_publish_index_ = 0;
    return;
  }

  const char flag = kOrder[qflag_publish_index_++];
  auto it = qflag_switches_.find(flag);
  if (it != qflag_switches_.end() && it->second != nullptr) {
    it->second->update_state_from_inverter(qflag_on_[static_cast<unsigned char>(flag)]);
  }

  if (qflag_publish_index_ >= sizeof(kOrder)) {
    qflag_ready_ = false;
    qflag_publish_index_ = 0;
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
  this->send_priority_command(cmd);
  this->send_priority_command("QFLAG");
}



void SolarInverter::add_inverter_select(int index, InverterSelect *sel) {
  (void) index;

  const std::string prefix = sel->get_command_prefix();
  const auto params = sel->get_parameters();
  const auto set_commands = sel->get_set_commands();
  const auto options = sel->get_options_list();
  const std::string field_name = sel->get_field_name();
  const std::string status_command = sel->get_status_command();

  sel->set_on_user_select_callback(
      [this, prefix, params, set_commands, options, field_name, status_command](const std::string &value) {
        auto it = std::find(options.begin(), options.end(), value);
        if (it == options.end()) {
          ESP_LOGW(TAG, "Value '%s' not found in options for '%s'", value.c_str(), field_name.c_str());
          return;
        }
        int idx = std::distance(options.begin(), it);
        if (idx < 0 || idx >= static_cast<int>(params.size())) {
          ESP_LOGW(TAG, "Index %d out of range for select '%s'", idx, field_name.c_str());
          return;
        }

        std::string command;
        if (idx < static_cast<int>(set_commands.size()))
          command = set_commands[idx];
        else if (!prefix.empty())
          command = prefix + params[idx];

        if (command.empty()) {
          ESP_LOGW(TAG,
                   "Select '%s': '%s' (QPIRI code %s) has no SET command — not sending invented POP/PGR. "
                   "Set the LCD, then Debug QPIRI.",
                   field_name.c_str(), value.c_str(), params[idx].c_str());
          if (!status_command.empty())
            this->send_priority_command(status_command);
          return;
        }

        this->send_priority_command(command);
        if (!status_command.empty())
          this->send_priority_command(status_command);
        ESP_LOGI(TAG, "Select '%s': '%s' SET '%s' (QPIRI code %s)", field_name.c_str(), value.c_str(),
                 command.c_str(), params[idx].c_str());
      });
}

void InverterInquiryButton::press_action() {
  if (this->parent_ == nullptr)
    return;
  if (this->set_probe_) {
    if (!this->set_probe_prefix_.empty())
      this->parent_->request_set_probe_with_nn(this->set_probe_prefix_);
    else
      this->parent_->request_set_probe(this->cmd_);
  } else if (this->dump_all_)
    this->parent_->request_debug_dump();
  else
    this->parent_->request_debug_inquiry(this->cmd_);
}

bool SolarInverter::is_safe_inquiry_(const std::string &cmd) {
  static const char *const kSafe[] = {
      "QPI",     "QID",      "QVFW",    "QVFW2",    "QVFW3",     "QMN",     "QGMN",   "QMOD",
      "QFLAG",   "QPIRI",    "QPIGS",   "QPIGS2",   "QPIWS",     "QBEQI",   "QT",     "QDI",
      "QOPPT",   "QCHPT",    "QMCHGCR", "QMUCHGCR", "QBOOT",     "QOPM",    "QET",    "QEY",
      "QEM",     "QED",      "QGO",
  };
  for (const char *s : kSafe) {
    if (cmd == s)
      return true;
  }
  return false;
}

bool SolarInverter::is_safe_set_probe_(const std::string &cmd) {
  // POP## / PCP## with exactly two digits (Voltronic NN). No free-form / PE/PD.
  // POP §3.12 documents 00/01/02; higher NN (e.g. POP03) are probe-only — never bind UtS select.
  // PCP §3.10 = charger priority (program 16).
  if (cmd.size() != 5)
    return false;
  if (!(cmd.compare(0, 3, "POP") == 0 || cmd.compare(0, 3, "PCP") == 0))
    return false;
  return std::isdigit(static_cast<unsigned char>(cmd[3])) &&
         std::isdigit(static_cast<unsigned char>(cmd[4]));
}

void SolarInverter::publish_debug_(const std::string &command, const std::string &response,
                                   const std::string &result) {
  // Hold SET probe ACK/NAK on HA entities (poller / auto QPIRI must not overwrite).
  if (this->hold_set_probe_debug_) {
    ESP_LOGD(TAG, "debug_last_* hold SET result; skip publish for %s (%s)", command.c_str(),
             result.c_str());
    return;
  }
  ESP_LOGI(TAG, "DEBUG_INQUIRY cmd=%s result=%s resp=%s", command.c_str(), result.c_str(),
           response.c_str());
  if (this->debug_last_command_)
    this->debug_last_command_->publish_state(command);
  if (this->debug_last_response_)
    this->debug_last_response_->publish_state(response.empty() ? result : response);
  if (this->debug_last_result_)
    this->debug_last_result_->publish_state(result);
}

void SolarInverter::finish_set_probe_(const std::string &command, const std::string &response,
                                      const std::string &result, bool queue_followups) {
  ESP_LOGI(TAG, "DEBUG_SET_PROBE cmd=%s result=%s resp=%s", command.c_str(), result.c_str(),
           response.c_str());
  // Always show the SET reply first on HA; hold until next manual debug/probe.
  this->hold_set_probe_debug_ = false;
  if (this->debug_last_command_)
    this->debug_last_command_->publish_state(command);
  if (this->debug_last_response_)
    this->debug_last_response_->publish_state(response.empty() ? result : response);
  if (this->debug_last_result_)
    this->debug_last_result_->publish_state(result);
  this->hold_set_probe_debug_ = true;

  this->set_probe_pending_ = false;
  this->set_probe_command_.clear();
  this->last_set_probe_done_ms_ = millis();
  this->flush_rx_();

  if (queue_followups) {
    // Re-read ratings + flags so HA program entities update; debug_last_* stays on SET.
    this->send_priority_command("QPIRI");
    this->send_priority_command("QFLAG");
  }
  this->finish_command_(POST_SET_PROBE_SETTLE_MS);
}

void SolarInverter::request_debug_inquiry(const std::string &cmd) {
  // Manual inquiry clears SET hold so user can see QPIRI etc. on debug_last_*.
  this->hold_set_probe_debug_ = false;
  if (!is_safe_inquiry_(cmd)) {
    ESP_LOGW(TAG, "Debug refused non-inquiry command '%s'", cmd.c_str());
    this->publish_debug_(cmd, "", "REFUSED");
    return;
  }
  ESP_LOGI(TAG, "DEBUG_INQUIRY queue %s", cmd.c_str());
  this->send_priority_command(cmd);
}

void SolarInverter::request_set_probe_with_nn(const std::string &prefix) {
  if (prefix != "POP" && prefix != "PCP") {
    ESP_LOGW(TAG, "SET probe refused — bad prefix '%s' (need POP or PCP)", prefix.c_str());
    this->hold_set_probe_debug_ = false;
    this->publish_debug_(prefix, "", "REFUSED");
    return;
  }
  if (this->debug_probe_nn_ == nullptr) {
    ESP_LOGW(TAG, "SET probe refused — debug_probe_nn not configured in YAML");
    this->hold_set_probe_debug_ = false;
    this->publish_debug_(prefix + "??", "", "REFUSED");
    return;
  }
  const float v = this->debug_probe_nn_->state;
  if (std::isnan(v) || v < 0.0f || v > 99.0f || v != std::floor(v)) {
    ESP_LOGW(TAG, "SET probe refused — invalid NN %.2f (need integer 0–99)", v);
    this->hold_set_probe_debug_ = false;
    this->publish_debug_(prefix + "??", "", "REFUSED");
    return;
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%s%02.0f", prefix.c_str(), v);
  this->request_set_probe(std::string(buf));
}

void SolarInverter::request_set_probe(const std::string &cmd) {
  if (!is_safe_set_probe_(cmd)) {
    ESP_LOGW(TAG, "SET probe refused unknown/unsafe command '%s' (need POP## or PCP##, two digits)",
             cmd.c_str());
    this->hold_set_probe_debug_ = false;
    this->publish_debug_(cmd, "", "REFUSED");
    return;
  }
  if (this->set_probe_pending_) {
    ESP_LOGW(TAG, "SET probe refused '%s' — already pending '%s'", cmd.c_str(),
             this->set_probe_command_.c_str());
    this->hold_set_probe_debug_ = false;
    this->publish_debug_(cmd, "", "REFUSED");
    return;
  }
  if (this->last_set_probe_done_ms_ != 0 &&
      millis() - this->last_set_probe_done_ms_ < SET_PROBE_COOLDOWN_MS) {
    ESP_LOGW(TAG, "SET probe refused '%s' — cooldown %u ms after last probe", cmd.c_str(),
             (unsigned) SET_PROBE_COOLDOWN_MS);
    this->hold_set_probe_debug_ = false;
    this->publish_debug_(cmd, "", "REFUSED");
    return;
  }
  this->set_probe_pending_ = true;
  this->set_probe_command_ = cmd;
  this->hold_set_probe_debug_ = false;
  ESP_LOGI(TAG, "DEBUG_SET_PROBE queue %s (then QPIRI+QFLAG)", cmd.c_str());
  // Optimistic: show command immediately so HA updates before ACK/NAK arrives.
  if (this->debug_last_command_)
    this->debug_last_command_->publish_state(cmd);
  if (this->debug_last_result_)
    this->debug_last_result_->publish_state("SENT");
  if (this->debug_last_response_)
    this->debug_last_response_->publish_state("…");
  this->send_priority_command(cmd);
}

void SolarInverter::request_debug_dump() {
  static const char *const kDump[] = {
      "QPI", "QID", "QVFW", "QVFW2", "QMN", "QGMN", "QMOD", "QFLAG", "QPIRI", "QPIGS",
      "QPIWS", "QBEQI", "QT", "QDI", "QOPPT", "QMCHGCR", "QMUCHGCR",
  };
  ESP_LOGI(TAG, "DEBUG_INQUIRY dump %u safe inquiries", (unsigned) (sizeof(kDump) / sizeof(kDump[0])));
  for (const char *cmd : kDump)
    this->request_debug_inquiry(cmd);
}

void SolarInverter::publish_output_source_priority_(const std::string &raw_code) {
  if (this->output_source_priority_code_)
    this->output_source_priority_code_->publish_state(raw_code);

  auto strip = [](std::string s) {
    size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0')
      i++;
    return s.substr(i);
  };
  const std::string code = strip(raw_code);
  const char *text = "Неизвестный код программы 01";
  if (code == "0")
    text = "USB — сеть сначала (Utility → Solar → Battery). Команда POP00.";
  else if (code == "1")
    text = "SUB — сначала солнце (Solar → Utility → Battery). Команда POP01.";
  else if (code == "2")
    text = "SBU — солнце, затем батарея (Solar → Battery → Utility). Команда POP02.";
  else if (code == "3")
    text = "UtS / SolarBatUtility* (QPIRI 3). MAX POP не документирует POP03 — SET с HA не шлём.";
  ESP_LOGI(TAG, "QPIRI[16] program 01 raw='%s' decoded='%s'", raw_code.c_str(), text);
  if (this->output_source_priority_text_)
    this->output_source_priority_text_->publish_state(text);
}


}  // namespace solar_inverter
}  // namespace esphome