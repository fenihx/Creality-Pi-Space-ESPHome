# Creality-Pi-Space-ESPHome
Monitor and control your Creality Pi Space  (and Plus version) filament dryer using ESP32 and ESPHome.


# Creality Pi Space Plus ESPHome Monitor

## English |

---

## Overview

Monitor and control your Creality Pi Space (and Plus version) filament dryer using ESP32 and ESPHome.

- Monitor: temperature, humidity, drying time, material, dryer status.
- Control: 4 buttons (Power, Up, Down, Set) via Home Assistant.
- I2C protocol sniffer: intercepts and decodes communication between the display and mainboard.
- Web interface: for local monitoring.
- Home Assistant integration.

---

## How it works

ESP32 connects to the I2C lines between the display and the dryer’s mainboard, intercepts and decodes packets, and publishes the data to Home Assistant via ESPHome.  
Button control is implemented by simulating button presses (shorting contacts to +5V via transistors).

---

## Wiring Diagram

![Wiring diagram]

- SCL — GPIO22 (ESP32)
- SDA — GPIO21 (ESP32)
- Buttons — GPIO16, 17, 27, 25 (see yaml)
- Power — 5V (ESP32 Wemos D1)

---

## Installation

1. **Assemble the circuit** according to the diagram above.
2. **Flash ESP32** with ESPHome (tested in version 2024.12.2).
   - Main config: esphome/creality-pi-space-plus.yaml
   - Custom component: esphome/i2c_sniffer.h
3. Add the device to Home Assistant.
4. Configure automations as you like.

---

## Files

- esphome/creality-pi-space-plus.yaml — main ESPHome config.
- esphome/i2c_sniffer.h — custom I2C sniffer component.

---

## Home Assistant Example Dashboard

<img width="323" height="827" alt="CrealityPiSpaceHA" src="https://github.com/user-attachments/assets/1bbb87d5-0b7f-45e9-9904-477ada4f148c" />


---

## Credits

- **[Ermakov/CrealitySpacePiAutomation](https://github.com/Ermakov/CrealitySpacePiAutomation)** — for ideas and packet decoding logic for Creality Pi Space.

---

## License

MIT License

---

## TODO

- [ ] Add wiring diagram 
- [ ] Add device photos
- [ ] Describe Home Assistant automations

---

## Feedback

Open an Issue for questions or suggestions.

---

---

## Описание

Мониторинг и управление сушилкой филамента Creality Pi Space ( и Plus версии) через ESP32 и ESPHome.

- Мониторинг: температуры, влажности, времени сушки, материала, статуса сушилки.
- Управление: 4 кнопки (Power, Up, Down, Set) через Home Assistant.
- Перехват и расшифровка I2C** между экраном и материнской платой.
- Веб-интерфейс для локального мониторинга.
- Интеграция с Home Assistant.

---

## Как это работает

ESP32 подключается к линиям I2C между дисплеем и платой сушилки, перехватывает пакеты, расшифровывает их и публикует значения в Home Assistant через ESPHome.  
Управление реализовано эмуляцией нажатий кнопок (замыкание на +5В через транзисторы).

---

## Схема подключения

- SCL — GPIO22 (ESP32)
- SDA** — GPIO21 (ESP32)
- Кнопки — GPIO16, 17, 27, 25
- Питание — 5В (ESP32 Wemos D1)

---

## Установка

1. Соберите схему.
2. Залейте прошивку на ESP32 через ESPHome.
   - Файл: esphome/creality-pi-space-plus.yaml
   - Кастомный компонент: esphome/i2c_sniffer.h
3. Добавьте устройство в Home Assistant.
4. Настройте автоматизации по своему вкусу.

---

## Файлы

- esphome/creality-pi-space-plus.yaml — основной конфиг ESPHome.
- esphome/i2c_sniffer.h` — кастомный компонент для перехвата и расшифровки I2C.

---

## Пример Home Assistant Dashboard

<img width="323" height="827" alt="CrealityPiSpaceHA" src="https://github.com/user-attachments/assets/1bbb87d5-0b7f-45e9-9904-477ada4f148c" />

---

## Благодарности

- **[Ermakov/CrealitySpacePiAutomation](https://github.com/Ermakov/CrealitySpacePiAutomation)** — за идеи и наработки по обработке I2C-пакетов для сушилки Creality Pi Space.

---

## Лицензия

MIT License

---

## TODO

- [ ] Добавить схему подключения (wiring_diagram.png)
- [ ] Добавить фотографии устройства
- [ ] Описать настройку автоматизаций в Home Assistant

---

## Обратная связь

Пишите вопросы и предложения в Issues.
