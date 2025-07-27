# TTN-TECO-6.2KW Dongle для моніторингу інвертора

Цей проєкт — апаратно-програмний донгл для підключення до інвертора TTN TECO 6.2KW через інтерфейс RS232 з використанням плати ESP32-C3 SuperMini.

## 🔧 Складові

| Компонент | Опис | Посилання |
|----------|------|-----------|
| ESP32-C3 SuperMini | Головний контролер з Wi-Fi | [Посилання](https://www.aliexpress.com/item/1005007479144456.html) |
| RS232 TTL MAX3232 | Преобразувач рівнів для RS232 ↔ TTL | [Посилання](https://www.aliexpress.com/item/4000370825055.html) |
| DC-DC перетворювач | Зниження напруги 12–24V → 5V 3A | [Посилання](https://www.aliexpress.com/item/1005007092498838.html) |

https://github.com/srepenko/esphome-solar-inverter/blob/main/components/solar_inverter/img/0-02-05-2faf52d0b74a87d8961dcf2dfb64cb0b8cecf41dd8493fadb3c749a5221d0289_a2796bc580be1cd5.jpg

## ⚙️ Призначення

Цей донгл дозволяє:

- зчитувати дані з інвертора TTN-TECO через UART/RS232;
- передавати дані в Home Assistant через ESPHome;
- вести локальний або хмарний моніторинг енергії.

## 🛠️ Підключення

1. **Живлення:**
   - 12–24V подається на вхід DC-DC.
   - Вихід 5V підключається до ESP32-C3.

2. **Зв’язок:**
   - TX/RX з інвертора (RS232) підключаються до входу MAX3232.
   - TTL-виходи MAX3232 підключаються до UART ESP32-C3 (рекомендовано: RX = GPIO20, TX = GPIO21 або інші доступні порти).

3. **Завантаження прошивки:**
   - Проєкт ESPHome (див. нижче).
   - Можна прошити через `USB-UART` адаптер або OTA.

## 📦 Програмна частина

Прошивка розроблена на базі ESPHome та підтримує:

- Автоматичне зчитування даних через команду `QPIGS`;
- Відображення сенсорів у Home Assistant;
- MQTT (опційно);
- Веб-інтерфейс з живими даними (опційно).

Деталі реалізації див. у [каталозі `custom_components/solar_inverter`](custom_components/solar_inverter).

## 🧾 Приклад `ttn-inverter.yaml`

```yaml
substitutions:
  name: ttn-inverter
  inv_id: ttn-inverter
  device_description: "Hybrid inverter via MAX communication"
  external_components_source: github://srepenko/esphome-solar-inverter@main

esphome:
  name: ${name}
  comment: ${device_description}
  min_version: 2024.6.0
  platformio_options:
    board_build.f_flash: 40000000L
    board_build.flash_mode: dio
    board_build.flash_size: 4MB

esp32:
  variant: ESP32C3
  board: seeed_xiao_esp32c3
  framework:
    type: arduino
...

external_components:
  - source: ${external_components_source}
    refresh: 0s

uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 2400

solar_inverter:
  uart_id: uart_bus
