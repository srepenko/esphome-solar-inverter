# TTN-TECO-6.2KW Donгл для моніторингу інвертора

Цей проєкт — апаратно-програмний донгл для підключення до інвертора TTN TECO 6.2KW через інтерфейс RS232 з використанням плати ESP32-C3 SuperMini.

## 🔧 Складові

| Компонент | Опис | Посилання |
|----------|------|-----------|
| ESP32-C3 SuperMini | Головний контролер з Wi-Fi | [Посилання](https://www.aliexpress.com/item/1005007479144456.html) |
| RS232 TTL MAX3232 | Преобразувач рівнів для RS232 ↔ TTL | [Посилання](https://www.aliexpress.com/item/4000370825055.html) |
| DC-DC перетворювач | Зниження напруги 12–24V → 5V 3A | [Посилання](https://www.aliexpress.com/item/1005007092498838.html) |

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
esphome:
  name: ttn-inverter
  includes:
    - custom_components/solar_inverter
...

uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 2400

external_components:
  - source: github://your-repo/solar_inverter
    components: [solar_inverter]

solar_inverter:
  uart_id: uart_bus
