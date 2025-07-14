#include "esphome.h"

class I2CSniffer : public Component, public UARTDevice {
public:
  I2CSniffer(UARTComponent *parent) : UARTDevice(parent) {}

  Sensor *sv_sensor = nullptr;
  Sensor *pv_sensor = nullptr;
  Sensor *humidity_sensor = nullptr;

  TextSensor *material_sensor = nullptr;
  TextSensor *cursor_sensor = nullptr;
  TextSensor *units_sensor = nullptr;
  TextSensor *time_sensor = nullptr;

  GPIOPin *scl_pin;
  GPIOPin *sda_pin;

  static const int BufSizeSoft = 32;
  byte BufferSoft[BufSizeSoft];
  byte byteNUM = 0;
  byte bitNUM = 0;
  byte byteTMP = 0;
  byte i2cStatus = 3;

  unsigned long i2cTimeSoft = 0;
  enum I2CStatus { READY = 0, RECEIVING = 1, STOP = 2, START = 3 };

  byte lastSCL = 1;

  void setup() override {
    pinMode(scl_pin->get_pin(), INPUT_PULLUP);
    pinMode(sda_pin->get_pin(), INPUT_PULLUP);
  }

  void loop() override {
    byte scl = digitalRead(scl_pin->get_pin());
    if (scl == HIGH && lastSCL == LOW) {
      irqSCL();
    }
    lastSCL = scl;

    if ((micros() - i2cTimeSoft > 500) && i2cStatus == START) {
      i2cStatus = READY;
    }

    if (i2cStatus == STOP) {
      process_buffer();
      i2cStatus = START;
      byteNUM = 0;
      bitNUM = 0;
      byteTMP = 0;
    }
  }

  void irqSCL() {
    if (i2cStatus == READY) {
      i2cStatus = RECEIVING;
      byteNUM = 0;
      bitNUM = 0;
      byteTMP = 0;
    }

    if (i2cStatus == RECEIVING) {
      if (bitNUM < 8) {
        byteTMP = (byteTMP << 1) | digitalRead(sda_pin->get_pin());
        bitNUM++;
      } else {
        if (byteNUM < BufSizeSoft) {
          BufferSoft[byteNUM++] = byteTMP;
        }
        byteTMP = 0;
        bitNUM = 0;
      }
      if (byteNUM >= BufSizeSoft) {
        i2cStatus = STOP;
      }
    }

    i2cTimeSoft = micros();
  }

  byte decodeDigit(byte high, byte low, bool isTemp = false) {
    const byte DigCount = 11;
    const byte Digits[DigCount] = {
      0xAF, 0xA0, 0xCB, 0xE9, 0xE4, 0x6D,
      0x6F, 0xA8, 0xEF, 0xED, 0x4F
    };
    byte dh = 255, dl = 255;

    for (byte i = 0; i < DigCount; i++) {
      if ((high & 0xEF) == (Digits[i] & 0xEF)) dh = i * 10;
      if ((low & 0xEF) == (Digits[i] & 0xEF)) dl = i;
    }

    if (dh == 255 || dl == 255) return 255;
    if (dh == 100) return 225;
    if (isTemp && (high & 0x10)) dh += 100;
    return dh + dl;
  }

  const char* matchMaterial(const byte* data) {
    struct Material {
      const char* name;
      const byte data[6];
    };

    const Material materials[] = {
      {"ABS",     {0xFF, 0xDF, 0x07, 0x00, 0x00, 0x00}},
      {"PETG",    {0xFD, 0xE5, 0xF1, 0x02, 0x00, 0x00}},
      {"PC",      {0xFD, 0x01, 0x01, 0x00, 0x00, 0x00}},
      {"PA",      {0xED, 0x0F, 0x01, 0x00, 0x00, 0x00}},
      {"PET",     {0xFD, 0xE5, 0x01, 0x00, 0x00, 0x00}},
      {"PLA-CF",  {0x7D, 0xE1, 0x0F, 0xF4, 0xE0, 0x04}},
      {"PETG-CF", {0xFD, 0xE5, 0xF1, 0x02, 0xF5, 0xE0}},
      {"PA-CF",   {0xED, 0x0F, 0xF5, 0xE0, 0x04, 0x00}},
      {"PLA",     {0x7D, 0xE1, 0x0F, 0x00, 0x00, 0x00}},
      {"TPU",     {0xE1, 0x7D, 0x0B, 0x00, 0x00, 0x00}},
      {"PP",      {0xED, 0x0D, 0x01, 0x00, 0x00, 0x00}},
      {"ASA",     {0xDF, 0xE7, 0x0F, 0x00, 0x00, 0x00}}
    };

    for (const auto& mat : materials) {
      bool match = true;
      for (byte i = 0; i < 6; i++) {
        if (data[i] != mat.data[i]) {
          match = false;
          break;
        }
      }
      if (match) return mat.name;
    }
    return "Unknown";
  }

  void process_buffer() {
    if (byteNUM < 22) return;

    byte sv = decodeDigit(BufferSoft[3], BufferSoft[4], true);
    byte pv = decodeDigit(BufferSoft[5], BufferSoft[6], true);
    byte rh = decodeDigit(BufferSoft[14], BufferSoft[15]);

    byte hh = decodeDigit(BufferSoft[16], BufferSoft[17]);
    byte mm = decodeDigit(BufferSoft[18], BufferSoft[19]);
    byte ss = decodeDigit(BufferSoft[20], BufferSoft[21]);

    const char* material = matchMaterial(&BufferSoft[8]);

    const char* cursor = "Unknown";
    switch (BufferSoft[2]) {
      case 0x70: cursor = "Idle"; break;
      case 0x72: cursor = "Time"; break;
      case 0x74: cursor = "Material"; break;
      case 0x78: cursor = "SV"; break;
      case 0xF0: cursor = "PV"; break;
    }

    const char* units = (BufferSoft[7] == 0xE5 ? "C" :
                         BufferSoft[7] == 0xEA ? "F" : "Unknown");

    if(sv_sensor) sv_sensor->publish_state(sv);
    if(pv_sensor) pv_sensor->publish_state(pv);
    if(humidity_sensor) humidity_sensor->publish_state(rh);

    if(material_sensor) material_sensor->publish_state(material);
    if(cursor_sensor) cursor_sensor->publish_state(cursor);
    if(units_sensor) units_sensor->publish_state(units);

    if(hh != 255 && mm != 255 && ss != 255 && time_sensor){
      char timebuf[9];
      sprintf(timebuf, "%02d:%02d:%02d", hh, mm, ss);
      time_sensor->publish_state(timebuf);
    }
  }
};
