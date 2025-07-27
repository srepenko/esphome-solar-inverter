// ============================
// File: solar_inverter.h
// ============================

#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/preferences.h"
#include "inverter_switch.h"
#include "inverter_select.h"
#include "inverter_number.h"
#include "esphome/components/select/select.h"


#include <queue>
#include <vector>
#include <string>

namespace esphome {
namespace solar_inverter {

struct CommandEntry {
  std::string command;
  uint32_t interval_ms;   // интервал в миллисекундах
  uint32_t last_run_ms;   // время последнего запуска (millis())
};

struct PendingResult {
  std::string command;
  std::string payload;
};

struct Date {
  int day;
  int month;
  int year;
};

struct EnergyValue {
  float value;
  uint8_t day;
  uint8_t month;
  uint16_t year;
};

class SolarInverter : public uart::UARTDevice, public Component {
 public:
   //void add_inverter_select(InverterSelect *sel);
   void add_inverter_select(int index, InverterSelect *sel);
   //void set_select_sensor(const std::string &field_name, esphome::select::Select *select);

   // Сеттеры для конфигурационных сенсоров
   void set_protocol_id_sensor(text_sensor::TextSensor *sens) { protocol_id_sensor_ = sens; }
   void set_serial_number_sensor(text_sensor::TextSensor *sens) { serial_number_sensor_ = sens; }
 
   // Сеттеры для QMOD
   void set_device_mode_sensor(text_sensor::TextSensor *sens) { device_mode_sensor_ = sens; }
   void set_device_mode_text(text_sensor::TextSensor *sens) { device_mode_text_ = sens; }

  // Сеттеры для QFLAG бинарных сенсоров
  void set_buzzer_control(InverterSwitch *sw) { buzzer_control_ = sw; }
  void set_overload_bypass(InverterSwitch *sw) { overload_bypass_ = sw; }
  void set_display_escape_to_default_page(InverterSwitch *sw) { display_escape_to_default_page_ = sw; }
  void set_overload_restart(InverterSwitch *sw) { overload_restart_ = sw; }
  void set_over_temperature_restart(InverterSwitch *sw) { over_temperature_restart_ = sw; }
  void set_backlight_control(InverterSwitch *sw) { backlight_control_ = sw; }
  void set_alarm_primary_source_interrupt(InverterSwitch *sw) { alarm_primary_source_interrupt_ = sw; }
  void set_fault_code_record(InverterSwitch *sw) { fault_code_record_ = sw; }
  void set_power_saving(InverterSwitch *sw) { power_saving_ = sw; }
  void set_data_log_popup(InverterSwitch *sw) { data_log_popup_ = sw; }
  void set_grid_charge_enable(InverterSwitch *sw) { grid_charge_enable_ = sw; }
  void set_solar_feed_to_grid(InverterSwitch *sw) { solar_feed_to_grid_ = sw; }
  // QPIWS
  void set_warning_status_text_sensor(text_sensor::TextSensor *sensor) { this->warning_status_text_sensor_ = sensor; }

   // Сеттеры для QPIGS сенсоров (числовые)
   void set_grid_voltage_sensor(sensor::Sensor *sens) { grid_voltage_sensor_ = sens; }
   void set_grid_freq_sensor(sensor::Sensor *sens) { grid_freq_sensor_ = sens; }
   void set_ac_output_voltage_sensor(sensor::Sensor *sens) { ac_output_voltage_sensor_ = sens; }
   void set_ac_output_freq_sensor(sensor::Sensor *sens) { ac_output_freq_sensor_ = sens; }
   void set_output_apparent_power_sensor(sensor::Sensor *sens) { output_apparent_power_sensor_ = sens; }
   void set_output_active_power_sensor(sensor::Sensor *sens) { output_active_power_sensor_ = sens; }
   void set_output_load_percent_sensor(sensor::Sensor *sens) { output_load_percent_sensor_ = sens; }
   void set_bus_voltage_sensor(sensor::Sensor *sens) { bus_voltage_sensor_ = sens; }
   void set_battery_voltage_sensor(sensor::Sensor *sens) { battery_voltage_sensor_ = sens; }
   void set_battery_charging_current_sensor(sensor::Sensor *sens) { battery_charging_current_sensor_ = sens; }
   void set_battery_capacity_sensor(sensor::Sensor *sens) { battery_capacity_sensor_ = sens; }
   void set_inverter_temp_sensor(sensor::Sensor *sens) { inverter_temp_sensor_ = sens; }
   void set_pv_input_current_sensor(sensor::Sensor *sens) { pv_input_current_sensor_ = sens; }
   void set_pv_input_voltage_sensor(sensor::Sensor *sens) { pv_input_voltage_sensor_ = sens; }
   void set_battery_voltage_from_scc_sensor(sensor::Sensor *sens) { battery_voltage_from_scc_sensor_ = sens; }
   void set_battery_discharge_current_sensor(sensor::Sensor *sens) { battery_discharge_current_sensor_ = sens; }
   void set_pv_charging_power_sensor(sensor::Sensor *sens) { pv_charging_power_sensor_ = sens; }
   void set_fan_on_voltage_offset_sensor(sensor::Sensor *sens) { fan_on_voltage_offset_sensor_ = sens; }
 
   // EEPROM версия (text)
   void set_eeprom_version_text(text_sensor::TextSensor *sens) { eeprom_version_text_ = sens; }
 
   // Бинарные сенсоры b7..b0
   void set_pv_or_ac_powering_load(binary_sensor::BinarySensor *sens) { pv_or_ac_powering_load_ = sens; }
   void set_config_changed(binary_sensor::BinarySensor *sens) { config_changed_ = sens; }
   void set_scc_fw_updated(binary_sensor::BinarySensor *sens) { scc_fw_updated_ = sens; }
   void set_load_on(binary_sensor::BinarySensor *sens) { load_on_ = sens; }
   void set_charging_on(binary_sensor::BinarySensor *sens) { charging_on_ = sens; }
   void set_scc_charging_on(binary_sensor::BinarySensor *sens) { scc_charging_on_ = sens; }
   void set_ac_charging_on(binary_sensor::BinarySensor *sens) { ac_charging_on_ = sens; }
   void set_charging_mode_text_sensor(text_sensor::TextSensor *sensor) {this->charging_mode_text_ = sensor;}
 
   // Комбинированный режим зарядки
   void set_charging_mode_sensor(sensor::Sensor *sens) { charging_mode_sensor_ = sens; }
   void set_charging_mode_text(text_sensor::TextSensor *sens) { charging_mode_text_ = sens; }
 
   // Бинарные сенсоры b10..b8
   void set_charging_to_float(binary_sensor::BinarySensor *sens) { charging_to_float_ = sens; }
   void set_inverter_on(binary_sensor::BinarySensor *sens) { inverter_on_ = sens; }
   void set_dustproof_installed(binary_sensor::BinarySensor *sens) { dustproof_installed_ = sens; }
 
   // История генерации энергии
   void set_energy_solar_today_sensor(sensor::Sensor *sens) { energy_solar_today_sensor_ = sens; }
   void set_energy_solar_month_sensor(sensor::Sensor *sens) { energy_solar_month_sensor_ = sens; }
   void set_energy_solar_year_sensor(sensor::Sensor *sens) { energy_solar_year_sensor_ = sens; }
   void set_energy_solar_total_sensor(sensor::Sensor *sens) { energy_solar_total_sensor_ = sens; }
 
   void set_energy_inverter_today_sensor(sensor::Sensor *sens) { energy_inverter_today_sensor_ = sens; }
   void set_energy_inverter_month_sensor(sensor::Sensor *sens) { energy_inverter_month_sensor_ = sens; }
   void set_energy_inverter_year_sensor(sensor::Sensor *sens) { energy_inverter_year_sensor_ = sens; }
   void set_energy_inverter_total_sensor(sensor::Sensor *sens) { energy_inverter_total_sensor_ = sens; }

  // Сеттеры для QBEQI 
  void set_equalization_enable(InverterSelect *s) { equalization_enable_ = s; }
  void set_equalization_voltage(InverterNumber *s) { equalization_voltage_ = s; }
  void set_equalization_over_time(InverterNumber *s) { equalization_over_time_ = s; }
  void set_equalization_time(InverterNumber *s) { equalization_time_ = s; }
  void set_equalization_period(InverterNumber *s) { equalization_period_ = s; }

  void set_equalization_active(InverterSelect *s) { equalization_active_ = s; }

  void set_equalization_max_current(sensor::Sensor *s) { equalization_max_current_ = s; }
  void set_equalization_elapsed_time(sensor::Sensor *s) { equalization_elapsed_time_ = s; }

  // ────────────────────────────────────────────────────────────
  // ── Сенсоры конфигурации (QPI, QID, QMOD, …)               ──
  // ────────────────────────────────────────────────────────────
  text_sensor::TextSensor *protocol_id_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  text_sensor::TextSensor *device_mode_sensor_{nullptr};
  text_sensor::TextSensor *device_mode_text_{nullptr};

  // ────────────────────────────────────────────────────────────
  // ── Сенсоры конфигурации (QFLAG)               ──
  // ────────────────────────────────────────────────────────────
  InverterSwitch *buzzer_control_{nullptr};
  InverterSwitch *overload_bypass_{nullptr};
  InverterSwitch *display_escape_to_default_page_{nullptr};
  InverterSwitch *overload_restart_{nullptr};
  InverterSwitch *over_temperature_restart_{nullptr};
  InverterSwitch *backlight_control_{nullptr};
  InverterSwitch *alarm_primary_source_interrupt_{nullptr};
  InverterSwitch *fault_code_record_{nullptr};
  InverterSwitch *power_saving_{nullptr};
  InverterSwitch *data_log_popup_{nullptr};
  InverterSwitch *solar_feed_to_grid_{nullptr};
  InverterSwitch *grid_charge_enable_{nullptr};

  // ────────────────────────────────────────────────────────────
  // ── Сенсоры QPIGS (числовые)                               ──
  // ────────────────────────────────────────────────────────────
  sensor::Sensor *grid_voltage_sensor_{nullptr};            // 0
  sensor::Sensor *grid_freq_sensor_{nullptr};               // 1
  sensor::Sensor *ac_output_voltage_sensor_{nullptr};       // 2
  sensor::Sensor *ac_output_freq_sensor_{nullptr};          // 3
  sensor::Sensor *output_apparent_power_sensor_{nullptr};   // 4
  sensor::Sensor *output_active_power_sensor_{nullptr};     // 5
  sensor::Sensor *output_load_percent_sensor_{nullptr};     // 6
  sensor::Sensor *bus_voltage_sensor_{nullptr};             // 7
  sensor::Sensor *battery_voltage_sensor_{nullptr};         // 8
  sensor::Sensor *battery_charging_current_sensor_{nullptr};// 9
  sensor::Sensor *battery_capacity_sensor_{nullptr};        // 10
  sensor::Sensor *inverter_temp_sensor_{nullptr};           // 11
  sensor::Sensor *pv_input_current_sensor_{nullptr};        // 12
  sensor::Sensor *pv_input_voltage_sensor_{nullptr};        // 13
  sensor::Sensor *battery_voltage_from_scc_sensor_{nullptr};// 14
  sensor::Sensor *battery_discharge_current_sensor_{nullptr};// 15
  sensor::Sensor *pv_charging_power_sensor_{nullptr};       // 19
  sensor::Sensor *fan_on_voltage_offset_sensor_{nullptr};   // 17 (QQ *0.01)

  // QPIWS
  text_sensor::TextSensor *warning_status_text_sensor_{nullptr};
  
  // QBEQI параметры
  InverterSelect *equalization_enable_{nullptr};        // B: 0/1
  InverterNumber *equalization_time_{nullptr};          // CCC: минуты
  InverterNumber *equalization_period_{nullptr};        // DDD: дни
  sensor::Sensor *equalization_max_current_{nullptr};   // EEE: A
  InverterNumber *equalization_voltage_{nullptr};       // GG.GG: V
  InverterNumber *equalization_over_time_{nullptr};     // III: минуты
  InverterSelect *equalization_active_{nullptr};        // J: 0/1
  sensor::Sensor *equalization_elapsed_time_{nullptr};  // KKKK: часы

  // QPIRI параметры
  std::vector<std::string> qpiri_parts_;

  sensor::Sensor *grid_rating_voltage_{nullptr};            // BBB.B  V
  sensor::Sensor *grid_rating_current_{nullptr};            // CC.C   A
  InverterNumber *ac_output_rating_voltage_{nullptr};       // DDD.D  V
  InverterNumber *ac_output_rating_frequency_{nullptr};     // EE.E   Hz
  InverterNumber *ac_output_rating_current_{nullptr};       // FF.F   A
  InverterNumber *ac_output_apparent_power_{nullptr};       // HHHH   VA
  InverterNumber *ac_output_active_power_{nullptr};         // IIII   W
  InverterNumber *battery_rating_voltage_{nullptr};         // JJ.J   V
  InverterNumber *battery_recharge_voltage_{nullptr};       // KK.K   V
  InverterNumber *battery_undervoltage_{nullptr};           // JJ.J   V
  InverterNumber *battery_bulk_voltage_{nullptr};           // KK.K   V
  InverterNumber *battery_float_voltage_{nullptr};          // LL.L   V
  InverterSelect *battery_type_{nullptr};                   // O      enum
  InverterNumber *max_ac_charging_current_{nullptr};        // PPP    A
  InverterNumber *max_charging_current_{nullptr};           // QQ0    A
  InverterSelect *input_voltage_range_{nullptr};            // O      enum
  InverterSelect *output_source_priority_{nullptr};         // P      enum
  InverterSelect *charger_source_priority_{nullptr};        // Q      enum
  sensor::Sensor *parallel_max_number_{nullptr};            // R      count
  InverterSelect *machine_type_{nullptr};                   // SS     enum
  InverterSelect *topology_{nullptr};                       // T      enum
  InverterSelect *output_mode_{nullptr};                    // U      enum
  InverterNumber *battery_redischarge_voltage_{nullptr};    // VV.V   V
  InverterSelect *pv_ok_condition_{nullptr};                // W      enum
  InverterSelect *pv_power_balance_{nullptr};               // X      enum
  sensor::Sensor *neizvestno_{nullptr};                     // X.XX   ?
  InverterNumber *grid_tie_current_{nullptr};               // YY     A
  InverterNumber *operation_logic_{nullptr};                // Zz.z   ?

  // Сеттеры для QPIRI
  // sensor::Sensor (read-only)
  void set_grid_rating_voltage(sensor::Sensor *s) { grid_rating_voltage_ = s; }
  void set_grid_rating_current(sensor::Sensor *s) { grid_rating_current_ = s; }
  void set_parallel_max_number(sensor::Sensor *s) { parallel_max_number_ = s; }
  void set_neizvestno(sensor::Sensor *s) { neizvestno_ = s; }

  // InverterNumber (writable)
  void set_battery_rating_voltage(InverterNumber *s) { battery_rating_voltage_ = s; }
  void set_ac_output_rating_voltage(InverterNumber *s) { ac_output_rating_voltage_ = s; }
  void set_ac_output_rating_frequency(InverterNumber *s) { ac_output_rating_frequency_ = s; }
  void set_ac_output_rating_current(InverterNumber *s) { ac_output_rating_current_ = s; }
  void set_ac_output_apparent_power(InverterNumber *s) { ac_output_apparent_power_ = s; }
  void set_ac_output_active_power(InverterNumber *s) { ac_output_active_power_ = s; }
  void set_battery_recharge_voltage(InverterNumber *s) { battery_recharge_voltage_ = s; }
  void set_battery_undervoltage(InverterNumber *s) { battery_undervoltage_ = s; }
  void set_battery_bulk_voltage(InverterNumber *s) { battery_bulk_voltage_ = s; }
  void set_battery_float_voltage(InverterNumber *s) { battery_float_voltage_ = s; }
  void set_max_ac_charging_current(InverterNumber *s) { max_ac_charging_current_ = s; }
  void set_max_charging_current(InverterNumber *s) { max_charging_current_ = s; }
  void set_battery_redischarge_voltage(InverterNumber *s) { battery_redischarge_voltage_ = s; }
  void set_grid_tie_current(InverterNumber *s) { grid_tie_current_ = s; }
  void set_operation_logic(InverterNumber *s) { operation_logic_ = s; }

  // InverterSelect (writable enums)
  void set_battery_type(InverterSelect *s) { battery_type_ = s; }
  void set_input_voltage_range(InverterSelect *s) { input_voltage_range_ = s; }
  void set_output_source_priority(InverterSelect *s) { output_source_priority_ = s; }
  void set_charger_source_priority(InverterSelect *s) { charger_source_priority_ = s; }
  void set_machine_type(InverterSelect *s) { machine_type_ = s; }
  void set_topology(InverterSelect *s) { topology_ = s; }
  void set_output_mode(InverterSelect *s) { output_mode_ = s; }
  void set_pv_ok_condition(InverterSelect *s) { pv_ok_condition_ = s; }
  void set_pv_power_balance(InverterSelect *s) { pv_power_balance_ = s; }

  void publish_sensor(sensor::Sensor *sensor, int index);
  void publish_number(InverterNumber *number, int index);
  void publish_select(InverterSelect *sel, int index);
 

  // EEPROM version (text)
  text_sensor::TextSensor *eeprom_version_text_{nullptr};    // 18

  // ────────────────────────────────────────────────────────────
  // ── Бинарные сенсоры b7..b0 (index 16)                     ──
  // ────────────────────────────────────────────────────────────
  binary_sensor::BinarySensor *pv_or_ac_powering_load_{nullptr}; // b7
  binary_sensor::BinarySensor *config_changed_{nullptr};         // b6
  binary_sensor::BinarySensor *scc_fw_updated_{nullptr};         // b5
  binary_sensor::BinarySensor *load_on_{nullptr};                // b4
  binary_sensor::BinarySensor *charging_on_{nullptr};            // b2
  binary_sensor::BinarySensor *scc_charging_on_{nullptr};        // b1
  binary_sensor::BinarySensor *ac_charging_on_{nullptr};         // b0
  
  // Комбинированный режим (b2b1b0)
  sensor::Sensor *charging_mode_sensor_{nullptr};           // код 0/5/6/7
  text_sensor::TextSensor *charging_mode_text_{nullptr};

  // ────────────────────────────────────────────────────────────
  // ── Бинарные сенсоры b10..b8 (index 20)                    ──
  // ────────────────────────────────────────────────────────────
  binary_sensor::BinarySensor *charging_to_float_{nullptr};  // b10
  binary_sensor::BinarySensor *inverter_on_{nullptr};        // b9
  binary_sensor::BinarySensor *dustproof_installed_{nullptr};// b8

  // ────────────────────────────────────────────────────────────
  // ── История генерации                                      ──
  // ────────────────────────────────────────────────────────────
  sensor::Sensor *energy_solar_today_sensor_{nullptr};
  sensor::Sensor *energy_solar_month_sensor_{nullptr};
  sensor::Sensor *energy_solar_year_sensor_{nullptr};
  sensor::Sensor *energy_solar_total_sensor_{nullptr};

  sensor::Sensor *energy_inverter_today_sensor_{nullptr};
  sensor::Sensor *energy_inverter_month_sensor_{nullptr};
  sensor::Sensor *energy_inverter_year_sensor_{nullptr};
  sensor::Sensor *energy_inverter_total_sensor_{nullptr};

  /* ---------- энерго‑счётчики, которые хотим сохранять ---------- */
  float accumulated_energy_solar_today_{0};
  float accumulated_energy_solar_month_{0};
  float accumulated_energy_solar_year_{0};
  float accumulated_energy_solar_total_{0.0f};

  float accumulated_energy_inverter_today_{0};
  float accumulated_energy_inverter_month_{0};
  float accumulated_energy_inverter_year_{0};
  float accumulated_energy_inverter_total_{0.0f};

  int last_day_{-1};
  int last_month_{-1};
  int last_year_{-1};

  Date get_current_date();        // если нужна дата — объявите структуру Date

  /* ---------- объекты‑ключи в NVS ---------- */
  ESPPreferenceObject pref_solar_total_;
  ESPPreferenceObject pref_inverter_total_;
  ESPPreferenceObject pref_solar_year_;
  ESPPreferenceObject pref_inverter_year_;
  ESPPreferenceObject pref_solar_month_;
  ESPPreferenceObject pref_inverter_month_;
  ESPPreferenceObject pref_solar_today_;
  ESPPreferenceObject pref_inverter_today_;


  /* ---------- методы ---------- */
  void save_energy_to_eeprom_();
  void load_energy_from_eeprom_();   // новый

  // ────────────────────────────────────────────────────────────
  // ── Жизненный цикл                                         ──
  // ────────────────────────────────────────────────────────────
  void setup() override;
  void loop() override;

  // ────────────────────────────────────────────────────────────
  // ── API для внешних модулей                                ──
  // ────────────────────────────────────────────────────────────
  void add_poll_command(const std::string &cmd, uint32_t interval_ms);
  void send_priority_command(const std::string &cmd);
  void update_energy_history_();
 private:
  
  InverterSelect *select_;

  // Словарь для QFLAG переключателей
  std::map<char, InverterSwitch*> qflag_switches_;

  // Отправка команды включения/выключения флага
  void set_flag(char flag, bool enabled);
  
  // Обработчик изменения состояния переключателя
  void on_flag_changed(char flag, bool enabled) { set_flag(flag, enabled); }

  // ───────────────────────── Internal state ──────────────────
  enum State { IDLE, WAITING_RESPONSE } state_{IDLE};
  std::queue<std::string> priority_commands_;
  std::vector<CommandEntry> poll_commands_;
  size_t poll_index_{0};
  std::queue<PendingResult> pending_results_;

  std::string current_command_;
  std::string rx_buffer_;
  bool receiving_{false};
  uint32_t last_send_{0};
  bool ready_{false};
  bool ack_received_{false};


  //  ─── Ответы, ожидающие публикации ───
  std::string last_qpigs_data_;
  bool qpigs_ready_{false};
  size_t qpigs_publish_index_{0};

  // Для пошаговой публикации QBEQI
  std::string last_qbeqi_data_;
  bool qbeqi_ready_{false};
  size_t qbeqi_publish_index_{0};

  // Для пошаговой публикации QPIRI
  std::string last_qpiri_data_;
  bool qpiri_ready_{false};
  size_t qpiri_publish_index_{0};

  //  ─── Таймауты ───
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 3000;

  //  ─── Внутренние методы ───
  void next_command_();
  void send_command(const std::string &cmd);
  void process_raw_response(const std::string &response);
  void process_result(const std::string &command, const std::string &payload);
  
  //  Публикация частями
  void publish_next_qpigs_chunk_();
  void process_qpigs_status_bits_(const std::string &bits);
  void process_qpigs_flag_bits_(const std::string &bits);
  void process_qmod_(const std::string &payload);
  void process_qflag_(const std::string &payload);
  std::string decode_qpiws_(const std::string &bits);
  void publish_next_qbeqi_chunk_();
  void publish_next_qpiri_chunk_();
  void setup_qflag_switches();

  //  CRC / utils
  static uint16_t calculate_crc(const std::string &cmd);
  static uint16_t cal_crc_half(const uint8_t *data, size_t len);
  bool check_crc(const std::string &response);

  static std::vector<std::string> split_string(const std::string &s, char delimiter);
  static bool safe_stof(const std::string &s, float &value);
 protected:
  std::map<int, InverterSelect*> inverter_selects_by_index_;
};

}  // namespace solar_inverter
}  // namespace esphome