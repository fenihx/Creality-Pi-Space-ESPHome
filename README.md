# Creality-Pi-Space-ESPHome

[English version](#english-version)

Мониторинг и управление сушилкой для филамента **Creality Pi Space** (и версии **Plus**) с помощью `ESP32` и `ESPHome`.
---

## Обзор

Интеграция сушилки `Creality Pi Space/Plus` в умный дом через `Home Assistant` и `ESPHome`.

<img width="515" height="223" alt="Creality Pi Space Dryer with ESPHome integration" src="https://github.com/user-attachments/assets/98645b09-a166-46d3-8322-99571a4b2b81" />

---

## Возможности

### Мониторинг
- 📊 Текущая и заданная температура
- 💧 Влажность
- ⏱️ Оставшееся время сушки
- 🎯 Выбранный материал
- 🔌 Статус устройства
- ⚠️ Код ошибки (**E4**)

### Управление
- 🔘 Эмуляция 4 кнопок (Power, Up, Down, Set)
- 🤖 Автоматический полный цикл сушки
- 🌡️ Автосушка по влажности
- 🔌 Автовключение питания
- 📱 Полное управление через Home Assistant

### Технические особенности
- 🔍 Перехват протокола I2C между дисплеем и платой управления
- 🌐 Веб-интерфейс ESPHome для локального мониторинга
- 📡 Полная интеграция с Home Assistant
- ⚡ Работа в реальном времени

---

## Компоненты

### Обязательные:
- **ESP32 D1 Wemos** — основной контроллер
- **Преобразователь логических уровней (8-канальный)** — *критически важно!* Для безопасного подключения `ESP32` (3.3В) к логике сушилки (5В)
- **Транзисторы для управления кнопками:** `IRLM6402` (P-channel MOSFET)

---

## ⚠️ Статус проекта

Проект является **экспериментальным** и собран из доступных компонентов. В коде использованы программные «хаки» для стабилизации показаний из-за неидеальной работы преобразователя уровней.

---

## Принцип работы

- `ESP32` подключается к шине I2C между дисплеем и платой управления.
- Перехватывает и декодирует пакеты данных в реальном времени.
- Публикует состояния в Home Assistant через API ESPHome.
- Управляет кнопками через транзисторы, замыкая их на +5В.

---

## Схема подключения

![Схема подключения](./wiring_diagram.png)

---

## Установка

> ⚠️ **Внимание!**
> Вы используете этот проект **на свой страх и риск**. Автор не несет ответственности за возможные повреждения вашего оборудования.

### Шаги:
1.  Соберите схему согласно диаграмме.
2.  Прошейте `ESP32` через ESPHome (протестировано на версии `2024.12.2`):
    -   Основной конфиг: `esphome/creality-pi-space-plus-v2.yaml`
    -   Кастомный компонент: `esphome/i2c_sniffer.h` (необходимо положить в ту же папку, что и yaml-файл)
3.  Добавьте устройство в Home Assistant.
4.  Настройте автоматизации и интерфейс по своему усмотрению.

---

## Новые функции (v2)

### 🤖 Автоматический полный цикл
- Автоматически включает сушилку.
- Выбирает нужный материал.
- Устанавливает время сушки.
- Запускает процесс одной кнопкой в Home Assistant.

### 🌡️ Автосушка по влажности
- Запускает сушку при превышении заданного порога влажности.
- Настраиваемый порог (20–80%).
- Настраиваемая длительность (1–48 ч).
- Период охлаждения: 1 час между циклами для корректных измерений.
- Можно отключить в настройках устройства.

### 🔌 Автовключение
- Поддерживает сушилку всегда включенной.
- Автоматически включает устройство, если оно было выключено.
- Проверка статуса каждые 30 секунд.
- Можно отключить в настройках.

### 📊 Улучшенный мониторинг
- Отображение ошибок (`E4` — ошибка нагревателя).
- Индикация выполнения скриптов.
- Отображение статусов автоматических функций.

---

## Скриншоты и примеры

Пример карточки для Home Assistant:

![HA Card](./HA_card.gif)

> Для создания такой карточки в Home Assistant потребуется установить кастомный компонент `button-card` через HACS.

Пример 3D-разводки печатной платы:

![3D View](./3d_view.png)

---

## Благодарности

-   [Ermakov/CrealitySpacePiAutomation](https://github.com/Ermakov/CrealitySpacePiAutomation) — за первоначальную идею и логику декодирования пакетов.

---

## Лицензия

[MIT License](LICENSE)

---

## Обратная связь

Открывайте **Issues** в репозитории для вопросов, предложений или сообщений об ошибках.

---
---

## English Version <a name="english-version"></a>

[Русская версия](#)

Monitoring and control of **Creality Pi Space** (and **Plus** version) filament dryer via `ESP32` and `ESPHome`.
---

### Overview

Integration of the `Creality Pi Space/Plus` dryer into a smart home system using `Home Assistant` and `ESPHome`.

<img width="515" height="223" alt="Creality Pi Space Dryer with ESPHome integration" src="https://github.com/user-attachments/assets/98645b09-a166-46d3-8322-99571a4b2b81" />

---

### Features

#### Monitoring:
- 📊 Current and target temperature
- 💧 Humidity
- ⏱️ Drying time remaining
- 🎯 Selected material
- 🔌 Device status
- ⚠️ Error code (**E4**)

#### Control:
- 🔘 4 button emulation (Power, Up, Down, Set)
- 🤖 Automatic full drying cycle
- 🌡️ Humidity-based auto-drying
- 🔌 Automatic power-on
- 📱 Full control via Home Assistant

#### Technical Features:
- 🔍 I2C protocol interception between the display and the control board
- 🌐 ESPHome web interface for local monitoring
- 📡 Full Home Assistant integration
- ⚡ Real-time operation

---

### Components

#### Required:
- **ESP32 D1 Wemos** — main controller
- **8-channel Logic Level Shifter** — *critical!* For safe connection between `ESP32` (3.3V) and the dryer's logic (5V)
- **Transistors for button control:** `IRLM6402` (P-channel MOSFET)

---

### ⚠️ Project Status

This project is **experimental** and built from available components. The code includes software "hacks" to stabilize readings due to a non-ideal level shifter performance.

---

### How It Works

- The `ESP32` connects to the I2C bus between the display and the control board.
- It intercepts and decodes data packets in real-time.
- It publishes states to Home Assistant via the ESPHome API.
- It controls the buttons via transistors by shorting them to +5V.

---

### Wiring Diagram

![Wiring Diagram](./wiring_diagram.png)

---

### Installation

> ⚠️ **Warning:**
> Use this project at your own risk. The author is not responsible for any potential damage to your equipment.

#### Steps:
1.  Assemble the circuit according to the diagram.
2.  Flash the `ESP32` via ESPHome (tested with version `2024.12.2`):
    -   Main config: `esphome/creality-pi-space-plus-v2.yaml`
    -   Custom component: `esphome/i2c_sniffer.h` (must be placed in the same folder as the yaml file)
3.  Add the device to Home Assistant.
4.  Configure automations and the UI as desired.

---

### New in v2

#### 🤖 Full Auto Cycle
- Automatically powers on the dryer.
- Selects the material.
- Sets the drying time.
- Starts the process with a single button in Home Assistant.

#### 🌡️ Humidity-based Drying
- Starts drying when the humidity threshold is exceeded.
- Configurable threshold (20-80%).
- Configurable duration (1-48h).
- Cooldown period: 1 hour between cycles for accurate measurements.
- Can be disabled in the device settings.

#### 🔌 Auto Power On
- Keeps the dryer always powered on.
- Automatically turns the device on if it was turned off.
- Status check every 30 seconds.
- Can be disabled in the settings.

#### 📊 Enhanced Monitoring
- Error display (`E4` - heating error).
- Script execution indication.
- Status display for automatic functions.

---

### Screenshots & Examples

Home Assistant Card Example:

![HA Card](./HA_card.gif)

> To create this card in Home Assistant, you need to install the `button-card` custom component via HACS.

3D PCB Layout Example:

![3D View](./3d_view.png)

---

### Credits

-   [Ermakov/CrealitySpacePiAutomation](https://github.com/Ermakov/CrealitySpacePiAutomation) — for the initial idea and packet decoding logic.

---

### License

[MIT License](LICENSE)

---

### Feedback

Please open **Issues** in the repository for questions, suggestions, or bug reports.
