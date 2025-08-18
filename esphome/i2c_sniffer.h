#pragma once
#include "esphome.h"
#include <cstring>

class I2CSniffer;

// Global pointers
static I2CSniffer* g_sniffer_instance = nullptr;
static volatile uint8_t* g_buffer = nullptr;
static volatile uint8_t* g_byte_num = nullptr;
static volatile uint8_t* g_bit_num = nullptr;
static volatile uint8_t* g_byte_tmp = nullptr;
static volatile uint8_t* g_i2c_status = nullptr;
static volatile uint32_t* g_last_edge_time = nullptr;
static volatile bool* g_wait_ack = nullptr;
static int g_sda_pin = 0;
static int g_scl_pin = 0;

static const uint8_t I2C_READY = 0;
static const uint8_t I2C_RECEIVING = 1;
static const uint8_t I2C_STOP = 2;
static const uint8_t I2C_BUSY = 8;
static const uint8_t BUF_SIZE = 32;

void IRAM_ATTR handle_scl_interrupt() {
    if (g_i2c_status == nullptr) return;
    if (*g_i2c_status != I2C_RECEIVING) {
        *g_last_edge_time = micros();
        return;
    }

    if (!(*g_wait_ack)) {
        *g_byte_tmp = (uint8_t)((*g_byte_tmp << 1) | (uint8_t)digitalRead(g_sda_pin));
        (*g_bit_num)++;
        if (*g_bit_num == 8) {
            if (*g_byte_num < BUF_SIZE)
                g_buffer[(*g_byte_num)++] = *g_byte_tmp;
            *g_byte_tmp = 0;
            *g_bit_num = 0;
            *g_wait_ack = true;
        }
    } else {
        *g_wait_ack = false;
    }

    *g_last_edge_time = micros();
}

void IRAM_ATTR handle_sda_interrupt() {
    if (g_i2c_status == nullptr) return;
    int scl = digitalRead(g_scl_pin);
    int sda = digitalRead(g_sda_pin);

    if (scl == HIGH) {
        if (sda == LOW) {
            *g_i2c_status = I2C_RECEIVING;
            *g_byte_num = 0;
            *g_bit_num = 0;
            *g_byte_tmp = 0;
            *g_wait_ack = false;
        } else {
            if (*g_i2c_status == I2C_RECEIVING || *g_i2c_status == I2C_BUSY)
                *g_i2c_status = I2C_STOP;
        }
    }

    *g_last_edge_time = micros();
}

class I2CSniffer : public Component, public CustomAPIDevice {
private:
    static const uint32_t I2C_TIMEOUT = 200;
    static const uint32_t DEVICE_OFF_TIMEOUT = 3000;
    static const uint8_t I2C_DEVICE_ADDRESS = 0x7E;
    static const uint8_t DIG_COUNT = 11;
    static const char* MATERIAL_NAME[12];
    static const uint8_t MATERIAL_XOR[12];

    const uint8_t DIGITS[DIG_COUNT] = {
        0xAF, 0xA0, 0xCB, 0xE9, 0xE4, 0x6D,
        0x6F, 0xA8, 0xEF, 0xED, 0x4F
    };

    // I2C buffers
    volatile uint8_t buffer[BUF_SIZE];
    volatile uint8_t byte_num = 0;
    volatile uint8_t bit_num = 0;
    volatile uint8_t byte_tmp = 0;
    volatile uint8_t i2c_status = I2C_READY;
    volatile uint32_t last_edge_time = 0;
    volatile bool wait_ack = false;

    // Last values
    uint8_t last_sv = 0, last_pv = 0, last_rh = 0;
    uint8_t last_hh = 255, last_mm = 255, last_ss = 255;  // 255 = not initialized
    int last_material_idx = -1;
    const char* last_cursor = "Unknown";
    const char* last_units = "Unknown";
    bool last_error_state = false;  // For tracking error state changes

    // Counters for filtering
    uint8_t sv_count = 0, pv_count = 0, rh_count = 0;
    uint8_t time_count = 0, material_count = 0, cursor_count = 0;

    // Candidates
    uint8_t sv_new = 0, pv_new = 0, rh_new = 0;
    uint8_t hh_new = 0, mm_new = 0, ss_new = 0;
    int material_new = -1;
    const char* cursor_new = nullptr;

    unsigned long last_packet_time = 0;
    unsigned long last_log_time = 0;
    bool device_on = false;
    bool time_initialized = false;  // Time initialization flag

    int scl_pin, sda_pin;

public:
    void setup() override {
        g_sniffer_instance = this;
        scl_pin = id(i2c_scl_pin);
        sda_pin = id(i2c_sda_pin);

        g_buffer = buffer;
        g_byte_num = &byte_num;
        g_bit_num = &bit_num;
        g_byte_tmp = &byte_tmp;
        g_i2c_status = &i2c_status;
        g_last_edge_time = &last_edge_time;
        g_sda_pin = sda_pin;
        g_scl_pin = scl_pin;
        g_wait_ack = &wait_ack;

        pinMode(scl_pin, INPUT_PULLUP);
        pinMode(sda_pin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(scl_pin), handle_scl_interrupt, RISING);
        attachInterrupt(digitalPinToInterrupt(sda_pin), handle_sda_interrupt, CHANGE);

        i2c_status = I2C_READY;
        wait_ack = false;
        last_packet_time = millis();

        // Initialize time as "00:00:00" at startup
        id(drying_time).publish_state("00:00:00");
        id(error_status).publish_state("OK");  // Initialize error status
        time_initialized = true;

        ESP_LOGI("i2c_sniffer", "Initialized on SCL=%d SDA=%d", scl_pin, sda_pin);
    }

    void loop() override {
        // I2C timeout
        if (i2c_status == I2C_RECEIVING && (micros() - last_edge_time) > I2C_TIMEOUT) {
            i2c_status = (byte_num > 0) ? I2C_STOP : I2C_READY;
        }

        // Device timeout check
        if (millis() - last_packet_time > DEVICE_OFF_TIMEOUT) {
            if (device_on) {
                device_on = false;
                time_initialized = false;  // Reset initialization flag
                ESP_LOGI("i2c_sniffer", "Device OFF - timeout");
                id(dryer_status).publish_state("Off");
                id(set_temp).publish_state(NAN);
                id(current_temp).publish_state(NAN);
                id(humidity).publish_state(NAN);
                id(drying_time).publish_state("Unknown");
                id(material).publish_state("N/A");
                id(cursor_state).publish_state("N/A");
                id(temp_units).publish_state("N/A");
                id(error_status).publish_state("N/A");  // Reset error status
                
                // Reset last time values
                last_hh = 255;
                last_mm = 255;
                last_ss = 255;
                last_error_state = false;
            }
        }

        // Packet processing
        if (i2c_status == I2C_STOP) {
            i2c_status = I2C_BUSY;

            // Basic validation
            if (byte_num < 22 || buffer[0] != I2C_DEVICE_ADDRESS) {
                i2c_status = I2C_READY;
                return;
            }

            last_packet_time = millis();
            
            if (!device_on) {
                device_on = true;
                ESP_LOGI("i2c_sniffer", "Device ON - data received");
                
                // When device turns on, initialize time as "00:00:00"
                if (!time_initialized) {
                    id(drying_time).publish_state("00:00:00");
                    id(error_status).publish_state("OK");
                    time_initialized = true;
                    last_hh = 0;
                    last_mm = 0;
                    last_ss = 0;
                }
            }

            // Decode values
            uint8_t sv = decode_digit(buffer[3], buffer[4], true);
            uint8_t pv = decode_digit(buffer[5], buffer[6], true);
            uint8_t rh = decode_digit(buffer[14], buffer[15]);
            uint8_t hh = decode_digit(buffer[16], buffer[17]);
            uint8_t mm = decode_digit(buffer[18], buffer[19]);
            uint8_t ss = decode_digit(buffer[20], buffer[21]);
            int mat_idx = decode_material_idx(buffer);
            const char* cursor = get_cursor_name(buffer[2]);
            const char* units = get_units(buffer[7]);

            // Periodic logging for debugging
            if (millis() - last_log_time > 5000) {
                ESP_LOGI("i2c_sniffer", "Raw: SV=%d PV=%d RH=%d Time=%02d:%02d:%02d Mat=%d Cursor=%s",
                         sv, pv, rh, hh, mm, ss, mat_idx, cursor);
                last_log_time = millis();
            }

            // SV - simple filtering with counting
            if (sv < 225) {
                if (sv == sv_new) {
                    if (++sv_count >= 3) {
                        if (sv != last_sv) {
                            last_sv = sv;
                            id(set_temp).publish_state(sv);
                            ESP_LOGI("i2c_sniffer", "SV updated: %d", sv);
                        }
                        sv_count = 0;
                    }
                } else {
                    sv_new = sv;
                    sv_count = 1;
                }
            }

            // PV - processing with error checking
            if (pv == 225) {
                // Check if this is really an error - look for error code in buffer
                // "E" is usually DIGITS[10] = 0x4F
                uint8_t high = buffer[5] & 0xEF;
                if (high == (DIGITS[10] & 0xEF)) {
                    // lower byte - error, digit
                    for (uint8_t i = 0; i < DIG_COUNT; i++) {
                        if ((buffer[6] & 0xEF) == (DIGITS[i] & 0xEF)) {
                            char error_str[4];
                            snprintf(error_str, sizeof(error_str), "E%d", i);
                            id(error_status).publish_state(error_str);
                            id(current_temp).publish_state(NAN);
                            last_error_state = true;
                            ESP_LOGI("i2c_sniffer", "PV Error detected: %s", error_str);
                            break;
                        }
                    }
                }
            } else if (pv < 225) {
                if (pv != last_pv) {
                    last_pv = pv;
                    id(current_temp).publish_state(pv);
                    if (last_error_state) {
                        id(error_status).publish_state("OK");
                        last_error_state = false;
                        ESP_LOGI("i2c_sniffer", "PV recovered, error cleared");
                    }
                }
            }

            // RH - simple filtering
            if (rh < 225) {
                if (rh == rh_new) {
                    if (++rh_count >= 3) {
                        if (rh != last_rh) {
                            last_rh = rh;
                            id(humidity).publish_state(rh);
                            ESP_LOGI("i2c_sniffer", "RH updated: %d", rh);
                        }
                        rh_count = 0;
                    }
                } else {
                    rh_new = rh;
                    rh_count = 1;
                }
            }

            // Time - improved logic
            if (hh < 225 && mm < 225 && ss < 225 && hh <= 48 && mm <= 59 && ss <= 59) {
                if (hh == hh_new && mm == mm_new && ss == ss_new) {
                    if (++time_count >= 2) {
                        // Always update time, even if it's 00:00:00
                        last_hh = hh;
                        last_mm = mm;
                        last_ss = ss;
                        
                        char time_str[9];
                        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hh, mm, ss);
                        id(drying_time).publish_state(time_str);
                        
                        uint32_t secs = (uint32_t)hh * 3600 + mm * 60 + ss;
                        id(dryer_status).publish_state(secs > 0 ? "Drying" : "Idle");
                        
                        ESP_LOGI("i2c_sniffer", "Time updated: %s", time_str);
                        time_count = 0;
                    }
                } else {
                    hh_new = hh;
                    mm_new = mm;
                    ss_new = ss;
                    time_count = 1;
                }
            }

            // Material - simple filtering
            if (mat_idx >= 0) {
                if (mat_idx == material_new) {
                    if (++material_count >= 5) {
                        if (mat_idx != last_material_idx) {
                            last_material_idx = mat_idx;
                            id(material).publish_state(MATERIAL_NAME[mat_idx]);
                            ESP_LOGI("i2c_sniffer", "Material updated: %s", MATERIAL_NAME[mat_idx]);
                        }
                        material_count = 0;
                    }
                } else {
                    material_new = mat_idx;
                    material_count = 1;
                }
            }

            // Cursor - simple filtering
            if (strcmp(cursor, "Unknown") != 0) {
                if (cursor == cursor_new) {
                    if (++cursor_count >= 2) {
                        if (strcmp(cursor, last_cursor) != 0) {
                            last_cursor = cursor;
                            id(cursor_state).publish_state(cursor);
                        }
                        cursor_count = 0;
                    }
                } else {
                    cursor_new = cursor;
                    cursor_count = 1;
                }
            }

            // Temperature units - publish only on change
            if (strcmp(units, "Unknown") != 0 && strcmp(units, last_units) != 0) {
                last_units = units;
                id(temp_units).publish_state(units);
            }

            i2c_status = I2C_READY;
        }
    }

private:
    // Simple digit decoding using Hamming distance
    uint8_t decode_digit(uint8_t high, uint8_t low, bool is_temp = false) {
        uint8_t high_m = high & 0xEF;
        uint8_t low_m = low & 0xEF;

        uint8_t dh = 255, dl = 255;

        // Look for exact match or with distance 1
        for (uint8_t i = 0; i < DIG_COUNT; i++) {
            uint8_t digit_m = DIGITS[i] & 0xEF;
            
            // For high digit
            if (digit_m == high_m) {
                dh = i * 10;
            } else if (dh == 255 && hamming_distance(digit_m, high_m) == 1) {
                dh = i * 10;
            }
            
            // For low digit
            if (digit_m == low_m) {
                dl = i;
            } else if (dl == 255 && hamming_distance(digit_m, low_m) == 1) {
                dl = i;
            }
        }

        if (dh == 255 || dl == 255) return 255;
        if (dh == 100) return 225; // ERR
        if (is_temp && (high & 0x10)) dh += 100;
        return dh + dl;
    }

    // Simple Hamming distance calculation
    uint8_t hamming_distance(uint8_t a, uint8_t b) {
        uint8_t xor_val = a ^ b;
        uint8_t count = 0;
        while (xor_val) {
            count += xor_val & 1;
            xor_val >>= 1;
        }
        return count;
    }

    // Material decoding
    int decode_material_idx(const volatile uint8_t* buf) {
        uint8_t mat_xor = 0;
        for (int i = 8; i < 14; i++) mat_xor ^= buf[i];

        // First look for exact match
        for (int i = 0; i < 12; i++) {
            if (mat_xor == MATERIAL_XOR[i]) return i;
        }

        // If no exact match, look for distance 1
        for (int i = 0; i < 12; i++) {
            if (hamming_distance(mat_xor, MATERIAL_XOR[i]) <= 1) return i;
        }

        return -1;
    }

    // Cursor determination
    const char* get_cursor_name(uint8_t val) {
        uint8_t cur = val & 0x8E;
        if (cur == 0x00) return "Idle";
        if (cur == 0x02) return "Time";
        if (cur == 0x04) return "Material";
        if (cur == 0x08) return "SV";
        if (cur == 0x80) return "PV";
        return "Unknown";
    }

    // Units determination
    const char* get_units(uint8_t val) {
        if (val == 0xE5) return "°C";
        if (val == 0xEA) return "°F";
        return "Unknown";
    }
};

// Global function (not used yet)
void enter_sniffer_fast_mode() {
    if (g_sniffer_instance) {
        // Can add functionality later
    }
}

// Material tables
const uint8_t I2CSniffer::MATERIAL_XOR[12] = {
    0x27, 0x37, 0xEB, 0xFD, 0xE3, 0x19,
    0x83, 0xFE, 0xF3, 0x93, 0x97, 0xE1
};

const char* I2CSniffer::MATERIAL_NAME[12] = {
    "ABS", "ASA", "PETG", "PC", "PA", "PET",
    "PLA-CF", "PETG-CF", "PA-CF", "PLA", "TPU", "PP"
};
