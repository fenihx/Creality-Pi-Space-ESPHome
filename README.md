# Creality-Pi-Space-ESPHome

Monitor and control your Creality Pi Space (and Plus version) filament dryer using ESP32 and ESPHome.

---

## English

### Overview

Monitor and control your Creality Pi Space (and Plus version) filament dryer using ESP32 and ESPHome.

- **Monitor:** temperature, humidity, drying time, material, dryer status.
- **Control:** 4 buttons (Power, Up, Down, Set) via Home Assistant.
- **I2C protocol sniffer:** intercepts and decodes communication between the display and mainboard.
- **Web interface:** for local monitoring.
- **Home Assistant integration.**

---

### Components

- **ESP32 D1 Wemos** — main controller.
- **Level Shifter (logic level converter)** — **important!** Used to safely connect ESP32 (3.3V logic) to the 5V logic of the dryer’s mainboard and buttons.  
  *Note: Using a proper level shifter is critical to avoid damaging the ESP32 and to ensure reliable operation.*
- **Transistors:**  
  - 4 × BC547 (NPN)  
  - 4 × BC557 (PNP)  
  - 4 × 1kΩ resistors (for base current limiting)  
  *Note: Button control is implemented via transistors, but using optocouplers might be a better/safer solution for galvanic isolation.*
- **AC/DC Power Supply 220V→5V (HiLink HLK-PM01 or similar)**  
  *Note: The power supply is connected directly to 220V AC from the mainboard. Be careful and ensure proper insulation and safety!*

---

### ⚠️ Project Status

This project is **experimental and was assembled quickly from available parts**.  
Some code "hacks" were required to stabilize readings, possibly due to a suboptimal or mismatched level shifter.  
**In the future, the project may be redesigned** for better reliability and safety.

---

### How it works

ESP32 connects to the I2C lines between the display and the dryer’s mainboard, intercepts and decodes packets, and publishes the data to Home Assistant via ESPHome.  
Button control is implemented by simulating button presses (shorting contacts to +5V via transistors).

---

### Wiring Diagram

<img width="1000" height="617" alt="image" src="https://github.com/user-attachments/assets/3a440bef-1829-472f-9547-99838fd50d7f" />


---

### Installation

1. **Assemble the circuit** according to the diagram above.
2. **Flash ESP32** with ESPHome (tested in version 2024.12.2).
   - Main config: `esphome/creality-pi-space-plus.yaml`
   - Custom component: `esphome/i2c_sniffer.h`
3. Add the device to Home Assistant.
4. Configure automations as you like.

---

### Files

- `esphome/creality-pi-space-plus.yaml` — main ESPHome config.
- `esphome/i2c_sniffer.h` — custom I2C sniffer component.

---

### Home Assistant Example Dashboard

<img width="323" height="827" alt="CrealityPiSpaceHA" src="https://github.com/user-attachments/assets/1bbb87d5-0b7f-45e9-9904-477ada4f148c" />

---

### Credits

- **[Ermakov/CrealitySpacePiAutomation](https://github.com/Ermakov/CrealitySpacePiAutomation)** — for ideas and packet decoding logic for Creality Pi Space.

---

### License

MIT License

---

### TODO

- [ ] Add device photos
- [ ] Describe Home Assistant automations

---

### Feedback

Open an Issue for questions or suggestions.

---

---

## Русский

### Описание

Мониторинг и управление сушилкой филамента Creality Pi Space (и Plus версии) через ESP32 и ESPHome.

- **Мониторинг:** температуры, влажности, времени сушки, материала, статуса сушилки.
- **Управление:** 4 кнопки (Power, Up, Down, Set) через Home Assistant.
- **Перехват и расшифровка I2C** между экраном и материнской платой.
- **Веб-интерфейс** для локального мониторинга.
- **Интеграция с Home Assistant.**

---

### Список компонентов

- **ESP32 D1 Wemos** — основной контроллер.
- **Level Shifter (преобразователь уровней)** — **важно!** Используется для согласования 3.3В логики ESP32 и 5В логики материнской платы сушилки и кнопок.  
  *Важно: Использование преобразователя уровней обязательно для защиты ESP32 и корректной работы!*
- **Транзисторы:**  
  - 4 × BC547 (NPN)  
  - 4 × BC557 (PNP)  
  - 4 × резистора 1кОм (ограничение тока базы)  
  *Примечание: Управление кнопками реализовано через транзисторы, но для лучшей развязки можно использовать оптопары.*
- **Блок питания 220В→5В AC/DC (HiLink HLK-PM01 или аналогичный)**  
  *Внимание: Блок питания подключается напрямую к 220В от материнской платы. Соблюдайте меры безопасности и изоляцию!*

---

### ⚠️ Статус проекта

**Проект сырой, делался быстро и из того, что было под рукой.**  
В коде есть "костыли" для стабилизации показаний, возможно из-за неудачного или неподходящего преобразователя логики.  
**В будущем возможна переработка схемы и кода для повышения надежности и безопасности.**

---

### Как это работает

ESP32 подключается к линиям I2C между дисплеем и платой сушилки, перехватывает пакеты, расшифровывает их и публикует значения в Home Assistant через ESPHome.  
Управление реализовано эмуляцией нажатий кнопок (замыкание на +5В через транзисторы).

---

### Схема подключения

<img width="1000" height="617" alt="image" src="https://github.com/user-attachments/assets/3a440bef-1829-472f-9547-99838fd50d7f" />


---

### Установка

1. Соберите схему по схеме выше.
2. Залейте прошивку на ESP32 через ESPHome (проверено на версии 2024.12.2).
   - Основной конфиг: `esphome/creality-pi-space-plus.yaml`
   - Кастомный компонент: `esphome/i2c_sniffer.h`
3. Добавьте устройство в Home Assistant.
4. Настройте автоматизации по своему вкусу.

---

### Файлы

- `esphome/creality-pi-space-plus.yaml` — основной конфиг ESPHome.
- `esphome/i2c_sniffer.h` — кастомный компонент для перехвата и расшифровки I2C.

---

### Пример Home Assistant Dashboard

<img width="323" height="827" alt="CrealityPiSpaceHA" src="https://github.com/user-attachments/assets/1bbb87d5-0b7f-45e9-9904-477ada4f148c" />

---

### Благодарности

- **[Ermakov/CrealitySpacePiAutomation](https://github.com/Ermakov/CrealitySpacePiAutomation)** — за идеи и наработки по обработке I2C-пакетов для сушилки Creality Pi Space.

---

### Лицензия

MIT License

---

### TODO

- [ ] Добавить фотографии устройства
- [ ] Описать настройку автоматизаций в Home Assistant

---

### Обратная связь

Пишите вопросы и предложения в Issues.

---
