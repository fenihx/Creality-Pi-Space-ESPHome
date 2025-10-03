#include "i2c_creality_pi_dryer.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <driver/gpio.h>

namespace esphome {
namespace i2c_creality_pi_dryer {

// ============================================================================
// Static Lookup Tables
// ============================================================================

/**
 * 7-segment display digit patterns
 * Index 0-9 represents digits 0-9, index 10 represents letter 'E' (for errors)
 * Each byte represents the segments that should be lit for that digit
 */
const uint8_t I2CCrealityPiDryer::DIGITS[DIG_COUNT] = {
    0xAF, 0xA0, 0xCB, 0xE9, 0xE4, 0x6D,
    0x6F, 0xA8, 0xEF, 0xED, 0x4F
};

/**
 * Material identification XOR checksums
 * Each material type has a unique XOR checksum calculated from bytes 8-13
 * Used to identify which material is currently selected on the dryer
 */
const uint8_t I2CCrealityPiDryer::MATERIAL_XOR[12] = {
    0x27, 0x37, 0xEB, 0xFD, 0xE3, 0x19,
    0x83, 0xFE, 0xF3, 0x93, 0x97, 0xE1
};

/**
 * Material name strings
 * Maps material index to human-readable name
 * Order must match MATERIAL_XOR array
 */
const char* I2CCrealityPiDryer::MATERIAL_NAME[12] = {
    "ABS", "ASA", "PETG", "PC", "PA", "PET",
    "PLA-CF", "PETG-CF", "PA-CF", "PLA", "TPU", "PP"
};

// ============================================================================
// Interrupt Service Routines (ISR)
// ============================================================================

/**
 * Global instance pointer for ISR access
 * ISRs cannot be class methods, so we use a global pointer
 */
static I2CCrealityPiDryer* g_instance = nullptr;

/**
 * SCL (Clock) interrupt handler
 * Triggered on rising edge of SCL signal
 * Reads SDA line to capture data bits
 * 
 * IRAM_ATTR: Places function in IRAM for faster execution (critical for timing)
 */
void IRAM_ATTR handle_scl_interrupt() {
    if (!g_instance) return;
    
    uint32_t now = micros();
    I2CStatus status = static_cast<I2CStatus>(g_instance->i2c_status_);
    
    // Only process if actively receiving data
    if (status != I2CStatus::RECEIVING) {
        g_instance->last_edge_time_ = now;
        return;
    }
    
    if (!g_instance->wait_ack_) {
        // Read SDA line and shift into byte buffer
        g_instance->byte_tmp_ = (g_instance->byte_tmp_ << 1) | gpio_get_level((gpio_num_t)g_instance->sda_pin_);
        g_instance->bit_num_++;
        
        // Complete byte received (8 bits)
        if (g_instance->bit_num_ == 8) {
            if (g_instance->byte_num_ < I2CCrealityPiDryer::BUFFER_SIZE) {
                g_instance->buffer_[g_instance->byte_num_++] = g_instance->byte_tmp_;
            }
            g_instance->byte_tmp_ = 0;
            g_instance->bit_num_ = 0;
            g_instance->wait_ack_ = true;  // Next bit will be ACK
        }
    } else {
        // Skip ACK bit
        g_instance->wait_ack_ = false;
    }
    
    g_instance->last_edge_time_ = now;
}

/**
 * SDA (Data) interrupt handler
 * Triggered on both edges of SDA signal
 * Detects I2C START and STOP conditions
 * 
 * START condition: SDA falls while SCL is high
 * STOP condition: SDA rises while SCL is high
 */
void IRAM_ATTR handle_sda_interrupt() {
    if (!g_instance) return;
    
    int scl = gpio_get_level((gpio_num_t)g_instance->scl_pin_);
    int sda = gpio_get_level((gpio_num_t)g_instance->sda_pin_);
    
    if (scl == 1) {  // SCL is HIGH
        if (sda == 0) {  // SDA is LOW - START condition detected
            g_instance->i2c_status_ = static_cast<uint8_t>(I2CStatus::RECEIVING);
            g_instance->byte_num_ = 0;
            g_instance->bit_num_ = 0;
            g_instance->byte_tmp_ = 0;
            g_instance->wait_ack_ = false;
        } else {  // SDA is HIGH - STOP condition detected
            I2CStatus status = static_cast<I2CStatus>(g_instance->i2c_status_);
            if (status == I2CStatus::RECEIVING || status == I2CStatus::BUSY) {
                g_instance->i2c_status_ = static_cast<uint8_t>(I2CStatus::STOP);
            }
        }
    }
    
    g_instance->last_edge_time_ = micros();
}

// ============================================================================
// Component Lifecycle Methods
// ============================================================================

/**
 * Component setup - called once during initialization
 * Configures GPIO pins and attaches interrupt handlers
 */
void I2CCrealityPiDryer::setup() {
    ESP_LOGI(TAG, "Initializing I2C sniffer on SCL=GPIO%d, SDA=GPIO%d", scl_pin_, sda_pin_);
    
    // Set global instance pointer for ISR access
    g_instance = this;
    
    // Configure GPIO pins as inputs
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;  // Disable interrupts during configuration
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << scl_pin_) | (1ULL << sda_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = enable_pullup_ ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Install GPIO ISR service (required before adding handlers)
    gpio_install_isr_service(0);
    
    // Attach interrupt handlers
    // SCL: Trigger on rising edge (when data is stable)
    gpio_set_intr_type((gpio_num_t)scl_pin_, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add((gpio_num_t)scl_pin_, (gpio_isr_t)handle_scl_interrupt, nullptr);
    
    // SDA: Trigger on any edge (to detect START/STOP conditions)
    gpio_set_intr_type((gpio_num_t)sda_pin_, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add((gpio_num_t)sda_pin_, (gpio_isr_t)handle_sda_interrupt, nullptr);
    
    // Initialize state
    i2c_status_ = static_cast<uint8_t>(I2CStatus::READY);
    last_packet_time_ = millis();
    last_yield_time_ = millis();
    
    // Publish initial sensor states
    if (drying_time_sensor_) drying_time_sensor_->publish_state("00:00:00");
    if (error_status_sensor_) error_status_sensor_->publish_state("OK");
    if (dryer_status_sensor_) dryer_status_sensor_->publish_state("Off");
    if (temp_units_sensor_) temp_units_sensor_->publish_state("C");
    
    ESP_LOGI(TAG, "I2C sniffer initialized successfully");
}

/**
 * Main processing loop - called repeatedly
 * Handles timeouts and packet processing
 */
void I2CCrealityPiDryer::loop() {
    // Yield to WiFi/API every 50ms to maintain responsiveness
    uint32_t current_millis = millis();
    if (current_millis - last_yield_time_ >= 50) {
        yield();
        last_yield_time_ = current_millis;
    }
    
    // Check for timeouts
    handle_timeouts();
    
    // Process complete packets
    if (i2c_status_ == static_cast<uint8_t>(I2CStatus::STOP)) {
        process_packet();
    }
}

// ============================================================================
// Timeout Handling
// ============================================================================

/**
 * Handle I2C communication and device timeouts
 * Detects when communication is lost or device is disconnected
 */
void I2CCrealityPiDryer::handle_timeouts() {
    uint32_t current_micros = micros();
    uint32_t current_millis = millis();
    
    // I2C communication timeout (200 microseconds)
    if (i2c_status_ == static_cast<uint8_t>(I2CStatus::RECEIVING)) {
        if ((current_micros - last_edge_time_) > I2C_TIMEOUT_US) {
            // Timeout occurred - mark packet as complete if data received
            i2c_status_ = (byte_num_ > 0) ? static_cast<uint8_t>(I2CStatus::STOP) : static_cast<uint8_t>(I2CStatus::READY);
        }
    }
    
    // Device disconnection timeout (3 seconds)
    if ((current_millis - last_packet_time_) > DEVICE_TIMEOUT_MS) {
        if (device_state_ != DeviceState::OFF) {
            handle_device_state_change(DeviceState::OFF);
            reset_all_states();
            
            // Publish "disconnected" states to all sensors
            if (dryer_status_sensor_) dryer_status_sensor_->publish_state("Off");
            if (set_temp_sensor_) set_temp_sensor_->publish_state(NAN);
            if (current_temp_sensor_) current_temp_sensor_->publish_state(NAN);
            if (humidity_sensor_) humidity_sensor_->publish_state(NAN);
            if (drying_time_sensor_) drying_time_sensor_->publish_state("Unknown");
            if (material_sensor_) material_sensor_->publish_state("N/A");
            if (cursor_sensor_) cursor_sensor_->publish_state("N/A");
            if (temp_units_sensor_) temp_units_sensor_->publish_state("N/A");
            if (error_status_sensor_) error_status_sensor_->publish_state("N/A");
        }
    }
}

// ============================================================================
// Packet Processing
// ============================================================================

/**
 * Process received I2C packet
 * Validates packet, decodes values, and publishes to sensors
 */
void I2CCrealityPiDryer::process_packet() {
    i2c_status_ = static_cast<uint8_t>(I2CStatus::BUSY);
    
    // Validate packet length and address
    if (byte_num_ < 22 || buffer_[0] != I2C_DEVICE_ADDRESS) {
        stats_.invalid_packets++;
        i2c_status_ = static_cast<uint8_t>(I2CStatus::READY);
        return;
    }
    
    stats_.valid_packets++;
    last_packet_time_ = millis();
    
    // Update device state if coming from OFF
    if (device_state_ == DeviceState::OFF) {
        handle_device_state_change(DeviceState::STARTING);
    }
    
    // Decode all values from packet buffer
    uint8_t sv = decode_digit(buffer_[3], buffer_[4], true);    // Set value (target temp)
    uint8_t pv = decode_digit(buffer_[5], buffer_[6], true);    // Process value (current temp)
    uint8_t rh = decode_digit(buffer_[14], buffer_[15]);        // Relative humidity
    uint8_t hh = decode_digit(buffer_[16], buffer_[17]);        // Hours
    uint8_t mm = decode_digit(buffer_[18], buffer_[19]);        // Minutes
    uint8_t ss = decode_digit(buffer_[20], buffer_[21]);        // Seconds
    int mat_idx = decode_material_idx(buffer_);                 // Material index
    const char* cursor = get_cursor_name(buffer_[2]);           // Cursor position
    const char* units = get_units(buffer_[7]);                  // Temperature units
    
    // Periodic debug logging (every 30 seconds)
    if ((millis() - last_log_time_) > LOG_INTERVAL_MS) {
        ESP_LOGD(TAG, "SV=%d PV=%d RH=%d Time=%02d:%02d:%02d Mat=%d Err=%s", 
                 sv, pv, rh, hh, mm, ss, mat_idx,
                 error_state_.error_active ? error_state_.last_error : "OK");
        last_log_time_ = millis();
    }
    
    // Handle error detection
    handle_pv_error(pv, buffer_[5], buffer_[6]);
    
    // Filter and publish all values
    filter_and_publish_values(sv, pv, rh, hh, mm, ss, mat_idx, cursor, units);
    
    i2c_status_ = static_cast<uint8_t>(I2CStatus::READY);
}

// ============================================================================
// Error Handling with Debouncing
// ============================================================================

/**
 * Handle error code detection with robust filtering
 * Implements debouncing to prevent false error triggers from I2C noise
 * 
 * Error format on display: "E" + digit (e.g., "E0", "E5")
 * pv == 225 indicates error display mode
 * 
 * @param pv Process value (225 = error mode)
 * @param high_byte High digit byte (letter 'E')
 * @param low_byte Low digit byte (error number 0-9)
 */
void I2CCrealityPiDryer::handle_pv_error(uint8_t pv, uint8_t high_byte, uint8_t low_byte) {
    // Check if error code is displayed (pv == 225 means "E" on display)
    if (pv == 225) {
        // Mask decimal point bit (0x10) for accurate comparison
        uint8_t high = high_byte & 0xEF;
        
        // Verify high digit is 'E' (letter E corresponds to DIGITS[10])
        if (high == (DIGITS[10] & 0xEF)) {
            // Find low digit (error number)
            for (uint8_t i = 0; i < DIG_COUNT; i++) {
                if ((low_byte & 0xEF) == (DIGITS[i] & 0xEF)) {
                    // Format error code string
                    char error_code[4];
                    snprintf(error_code, sizeof(error_code), "E%d", i);
                    
                    // Filter: check if matches candidate
                    if (strcmp(error_code, error_state_.candidate_error) == 0) {
                        error_state_.error_count++;
                        error_state_.clear_count = 0;  // Reset clear counter
                        reset_filters();
                        // If error repeated enough times, confirm and publish
                        if (error_state_.error_count >= error_state_.min_error_repeats) {
                            if (!error_state_.error_active || 
                                strcmp(error_code, error_state_.last_error) != 0) {
                                
                                // Publish error to sensor
                                if (error_status_sensor_) {
                                    error_status_sensor_->publish_state(error_code);
                                }
                                // Clear temperature sensor during error
                                if (current_temp_sensor_) {
                                    current_temp_sensor_->publish_state(NAN);
                                }
                                
                                strncpy(error_state_.last_error, error_code, sizeof(error_state_.last_error));
                                error_state_.error_active = true;
                                
                                ESP_LOGW(TAG, "Error confirmed: %s (after %d repeats)", 
                                         error_code, error_state_.error_count);
                                
                                // Update device state to ERROR
                                handle_device_state_change(DeviceState::ERROR);
                            }
                            error_state_.error_count = 0;  // Reset after publishing
                        }
                    } else {
                        // New error candidate detected
                        strncpy(error_state_.candidate_error, error_code, sizeof(error_state_.candidate_error));
                        error_state_.error_count = 1;
                        error_state_.clear_count = 0;
                    }
                    return;
                }
            }
            
            // Unrecognized error pattern - likely noise
            ESP_LOGD(TAG, "Unrecognized error pattern: high=0x%02X low=0x%02X", high_byte, low_byte);
        }
    } else {
        // Normal PV value (not error)
        if (error_state_.error_active) {
            // Increment normal reading counter
            error_state_.clear_count++;
            error_state_.error_count = 0;
            
            // If enough consecutive normal readings, clear error
            if (error_state_.clear_count >= error_state_.min_clear_repeats) {
                if (error_status_sensor_) {
                    error_status_sensor_->publish_state("OK");
                }
                
                strncpy(error_state_.last_error, "OK", sizeof(error_state_.last_error));
                strncpy(error_state_.candidate_error, "OK", sizeof(error_state_.candidate_error));
                error_state_.error_active = false;
                error_state_.clear_count = 0;
                
                ESP_LOGI(TAG, "Error cleared after %d normal readings", error_state_.min_clear_repeats);
                
                // Restore normal device state
                if (device_state_ == DeviceState::ERROR) {
                    handle_device_state_change(DeviceState::IDLE);
                }
            }
        } else {
            // No error, reset counters
            strncpy(error_state_.candidate_error, "OK", sizeof(error_state_.candidate_error));
            error_state_.error_count = 0;
            error_state_.clear_count = 0;
        }
    }
}

// ============================================================================
// Value Filtering and Publishing
// ============================================================================

/**
 * Filter and publish all sensor values
 * Applies debouncing filters to prevent publishing noisy/unstable readings
 * 
 * @param sv Set value (target temperature)
 * @param pv Process value (current temperature)
 * @param rh Relative humidity
 * @param hh Hours
 * @param mm Minutes
 * @param ss Seconds
 * @param mat_idx Material index
 * @param cursor Cursor position string
 * @param units Temperature units string
 */
void I2CCrealityPiDryer::filter_and_publish_values(uint8_t sv, uint8_t pv, uint8_t rh, 
                                                   uint8_t hh, uint8_t mm, uint8_t ss, 
                                                   int mat_idx, const char* cursor, 
                                                   const char* units) {
    // Set Temperature (with jump protection)
    if (sv < 225 && sv <= 80) {
        // Reject jumps > 5°C (likely noise or error)
        if (set_temp_filter_.initialized && abs((int)sv - (int)set_temp_filter_.last_value) > 5) {
            // Ignore jump
        } else {
            if (sv == set_temp_filter_.candidate) {
                if (++set_temp_filter_.count >= set_temp_filter_.min_repeats) {
                    if (!set_temp_filter_.initialized || sv != set_temp_filter_.last_value) {
                        set_temp_filter_.last_value = sv;
                        if (set_temp_sensor_) set_temp_sensor_->publish_state((float)sv);
                        set_temp_filter_.initialized = true;
                    }
                    set_temp_filter_.count = 0;
                }
            } else {
                set_temp_filter_.candidate = sv;
                set_temp_filter_.count = 1;
            }
        }
    }
    
    // Current Temperature (with improved jump protection)
    if (pv < 225 && pv <= 80 && !error_state_.error_active) {
        bool is_jump = process_temp_filter_.initialized && 
                    abs((int)pv - (int)process_temp_filter_.last_value) > 5;
        
        
        if (pv == process_temp_filter_.candidate) {
            process_temp_filter_.count++;
        } else {
            process_temp_filter_.candidate = pv;
            process_temp_filter_.count = 1;
        }
        
        
        int required = is_jump ? 10 : process_temp_filter_.min_repeats;
        
        if (process_temp_filter_.count >= required) {
            if (!process_temp_filter_.initialized || pv != process_temp_filter_.last_value) {
                process_temp_filter_.last_value = pv;
                if (current_temp_sensor_) current_temp_sensor_->publish_state((float)pv);
                process_temp_filter_.initialized = true;
                
                if (is_jump) {
                    ESP_LOGI(TAG, "Temperature jump accepted after %d repeats: %d°C", 
                            process_temp_filter_.count, pv);
                }
            }
            process_temp_filter_.count = 0;
        }
    }
    
    // Humidity (3 repeats required)
    if (rh < 225) {
        if (rh == humidity_filter_.candidate) {
            if (++humidity_filter_.count >= humidity_filter_.min_repeats) {
                if (!humidity_filter_.initialized || rh != humidity_filter_.last_value) {
                    humidity_filter_.last_value = rh;
                    if (humidity_sensor_) humidity_sensor_->publish_state(rh);
                    humidity_filter_.initialized = true;
                }
                humidity_filter_.count = 0;
            }
        } else {
            humidity_filter_.candidate = rh;
            humidity_filter_.count = 1;
        }
    }
    
    // Drying Time (2 repeats required)
    if (hh < 225 && hh <= 48 && mm < 60 && ss < 60) {
        uint32_t total_seconds = (uint32_t)hh * 3600 + mm * 60 + ss;
        
        if (total_seconds == time_filter_.candidate) {
            if (++time_filter_.count >= time_filter_.min_repeats) {
                if (!time_filter_.initialized || total_seconds != time_filter_.last_value) {
                    time_filter_.last_value = total_seconds;
                    
                    // Format time string (HH:MM:SS)
                    char time_str[9];
                    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hh, mm, ss);
                    if (drying_time_sensor_) drying_time_sensor_->publish_state(time_str);
                    
                    // Update device state based on time (Drying if > 0, Idle if 0)
                    DeviceState new_state = (total_seconds > 0) ? DeviceState::DRYING : DeviceState::IDLE;
                    if (device_state_ != DeviceState::ERROR) {
                        handle_device_state_change(new_state);
                    }
                    
                    // Update dryer status sensor
                    if (dryer_status_sensor_) {
                        const char* status = (total_seconds > 0) ? "Drying" : "Idle";
                        dryer_status_sensor_->publish_state(status);
                    }
                    
                    time_filter_.initialized = true;
                }
                time_filter_.count = 0;
            }
        } else {
            time_filter_.candidate = total_seconds;
            time_filter_.count = 1;
        }
    }
    
    // Material (slow stabilization - 10 repeats required)
    if (mat_idx >= 0) {
        if (mat_idx == material_filter_.candidate) {
            if (++material_filter_.count >= material_filter_.min_repeats) {
                if (!material_filter_.initialized || mat_idx != material_filter_.last_value) {
                    material_filter_.last_value = mat_idx;
                    if (material_sensor_) material_sensor_->publish_state(MATERIAL_NAME[mat_idx]);
                    material_filter_.initialized = true;
                }
                material_filter_.count = 0;
            }
        } else {
            material_filter_.candidate = mat_idx;
            material_filter_.count = 1;
        }
    }
    
    // Cursor (slow stabilization - 3 repeats required)
    if (strcmp(cursor, "Unknown") != 0) {
        if (strcmp(cursor, cursor_filter_.candidate) == 0) {
            if (++cursor_filter_.count >= cursor_filter_.min_repeats) {
                if (!cursor_filter_.initialized || strcmp(cursor, cursor_filter_.last_value) != 0) {
                    cursor_filter_.last_value = cursor;
                    if (cursor_sensor_) cursor_sensor_->publish_state(cursor);
                    cursor_filter_.initialized = true;
                }
                cursor_filter_.count = 0;
            }
        } else {
            cursor_filter_.candidate = cursor;
            cursor_filter_.count = 1;
        }
    }
    
    // Temperature Units (immediate - publish only on change)
    if (strcmp(units, "Unknown") != 0 && strcmp(units, last_units_) != 0) {
        last_units_ = units;
        if (temp_units_sensor_) temp_units_sensor_->publish_state(units);
    }
}

// ============================================================================
// State Management
// ============================================================================

/**
 * Handle device state transitions
 * @param new_state New device state to transition to
 */
void I2CCrealityPiDryer::handle_device_state_change(DeviceState new_state) {
    if (device_state_ != new_state) {
        device_state_ = new_state;
    }
}

/**
 * Reset all value filters
 * Called when device reconnects or state changes significantly
 */
void I2CCrealityPiDryer::reset_filters() {
    set_temp_filter_.reset();
    process_temp_filter_.reset();
    humidity_filter_.reset();
    time_filter_.reset();
    material_filter_.reset();
    cursor_filter_.reset();
}

/**
 * Reset all state variables
 * Called when device disconnects
 */
void I2CCrealityPiDryer::reset_all_states() {
    reset_filters();
    last_error_state_ = false;
    last_rh_ = 255;
    last_units_ = "Unknown";
    last_cursor_ = "Unknown";
    
    // Reset error state
    strncpy(error_state_.last_error, "OK", sizeof(error_state_.last_error));
    strncpy(error_state_.candidate_error, "OK", sizeof(error_state_.candidate_error));
    error_state_.error_count = 0;
    error_state_.clear_count = 0;
    error_state_.error_active = false;
}

// ============================================================================
// Decoding Functions
// ============================================================================

/**
 * Decode 7-segment display digit from two bytes
 * Uses lookup table and Hamming distance for error correction
 * 
 * @param high High byte (tens digit)
 * @param low Low byte (ones digit)
 * @param is_temp Whether this is a temperature value (affects special handling)
 * @return Decoded numeric value (0-99) or special codes: 255=invalid, 225=error
 */
uint8_t I2CCrealityPiDryer::decode_digit(uint8_t high, uint8_t low, bool is_temp) {
    // Mask decimal point bit for comparison
    uint8_t high_m = high & 0xEF;
    uint8_t low_m = low & 0xEF;
    
    uint8_t dh = 255, dl = 255;  // Initialize as invalid
    
    // Search for matching digit patterns with error correction
    for (uint8_t i = 0; i < DIG_COUNT; i++) {
        uint8_t digit_m = DIGITS[i] & 0xEF;
        
        // Check high digit (tens place)
        if (digit_m == high_m) {
            dh = i * 10;
        } else if (dh == 255 && hamming_distance(digit_m, high_m) == 1) {
            // Allow 1-bit error (error correction)
            dh = i * 10;
        }
        
        // Check low digit (ones place)
        if (digit_m == low_m) {
            dl = i;
        } else if (dl == 255 && hamming_distance(digit_m, low_m) == 1) {
            // Allow 1-bit error (error correction)
            dl = i;
        }
    }
    
    // Validate decode results
    if (dh == 255 || dl == 255) return 255;  // Invalid digit
    if (dh == 100) return 225;  // Letter 'E' detected (error code)
    if (is_temp && (high & 0x10)) dh += 100;  // Handle temperatures > 99°C
    
    return dh + dl;
}

/**
 * Decode material index from XOR checksum
 * Calculates XOR of bytes 8-13 and matches against known material checksums
 * 
 * @param buf Pointer to I2C packet buffer
 * @return Material index (0-11) or -1 if unknown
 */
int I2CCrealityPiDryer::decode_material_idx(const volatile uint8_t* buf) {
    // Calculate XOR checksum from bytes 8-13
    uint8_t mat_xor = 0;
    for (int i = 8; i < 14; i++) mat_xor ^= buf[i];
    
    // First pass: exact match
    for (int i = 0; i < 12; i++) {
        if (mat_xor == MATERIAL_XOR[i]) return i;
    }
    
    // Second pass: allow 1-bit error (error correction)
    for (int i = 0; i < 12; i++) {
        if (hamming_distance(mat_xor, MATERIAL_XOR[i]) <= 1) return i;
    }
    
    return -1;  // Unknown material
}

/**
 * Get cursor position name from byte value
 * Determines which menu item is currently selected
 * 
 * @param val Cursor state byte
 * @return Cursor position string ("Idle", "Time", "Material", "SV", "PV")
 */
const char* I2CCrealityPiDryer::get_cursor_name(uint8_t val) {
    uint8_t cur = val & 0x8E;  // Mask relevant bits
    if (cur == 0x00) return "Idle";
    if (cur == 0x02) return "Time";
    if (cur == 0x04) return "Material";
    if (cur == 0x08) return "SV";
    if (cur == 0x80) return "PV";
    return "Unknown";
}

/**
 * Get temperature units from byte value
 * 
 * @param val Units byte
 * @return Units string ("C" for Celsius, "F" for Fahrenheit)
 */
const char* I2CCrealityPiDryer::get_units(uint8_t val) {
    if (val == 0xE5) return "C";
    if (val == 0xEA) return "F";
    return "Unknown";
}

/**
 * Calculate Hamming distance between two bytes
 * Counts number of differing bits (used for error correction)
 * 
 * @param a First byte
 * @param b Second byte
 * @return Number of bits that differ (0-8)
 */
uint8_t I2CCrealityPiDryer::hamming_distance(uint8_t a, uint8_t b) {
    uint8_t xor_val = a ^ b;
    uint8_t count = 0;
    while (xor_val) {
        count += xor_val & 1;
        xor_val >>= 1;
    }
    return count;
}

/**
 * Dump component configuration to log
 * Called during ESPHome startup to display configuration
 */
void I2CCrealityPiDryer::dump_config() {
    ESP_LOGCONFIG(TAG, "I2C Creality Pi Dryer:");
    ESP_LOGCONFIG(TAG, "  SCL Pin: GPIO%d", scl_pin_);
    ESP_LOGCONFIG(TAG, "  SDA Pin: GPIO%d", sda_pin_);
    ESP_LOGCONFIG(TAG, "  Pullup: %s", enable_pullup_ ? "enabled" : "disabled");
    ESP_LOGCONFIG(TAG, "  Statistics: %s", enable_statistics_ ? "enabled" : "disabled");
}

}  // namespace i2c_creality_pi_dryer
}  // namespace esphome
