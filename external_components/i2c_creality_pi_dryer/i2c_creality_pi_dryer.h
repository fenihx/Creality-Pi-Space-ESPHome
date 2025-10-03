#pragma once
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <functional>

namespace esphome {
namespace i2c_creality_pi_dryer {

// Logging tag for ESP log system
static const char *const TAG = "i2c_creality_pi_dryer";

/**
 * Device operational states
 * Tracks the current state of the dryer device
 */
enum class DeviceState : uint8_t {
    OFF = 0,        // Device is powered off or disconnected
    STARTING = 1,   // Device is starting up (transitioning from off to idle)
    IDLE = 2,       // Device is on but not actively drying
    DRYING = 3,     // Device is actively drying filament
    ERROR = 4       // Device has encountered an error condition
};

/**
 * I2C communication status
 * Tracks the current state of I2C packet reception
 */
enum class I2CStatus : uint8_t {
    READY = 0,      // Ready to receive new data
    RECEIVING = 1,  // Currently receiving I2C packet
    STOP = 2,       // Stop condition detected, packet complete
    BUSY = 3        // Processing received packet
};

/**
 * Template structure for filtered sensor values
 * Implements debouncing by requiring multiple consecutive identical readings
 * before publishing to prevent noise and false readings
 * 
 * Template parameter T: The data type being filtered (uint8_t, int, const char*, etc.)
 */
template<typename T>
struct FilteredValue {
    T last_value;           // Last published/confirmed value
    T candidate;            // Current candidate value being evaluated
    uint8_t count = 0;      // Number of consecutive times candidate has been seen
    uint8_t min_repeats;    // Required consecutive matches before publishing
    bool initialized = false; // Whether any value has been published yet

    // Constructor
    FilteredValue(T initial, uint8_t repeats) : last_value(initial), candidate(initial), min_repeats(repeats) {}

    /**
     * Update filter with new value and potentially publish
     * 
     * @param new_val New value to evaluate
     * @param publisher Function to call when value is confirmed (publishes to sensor)
     * @param validator Optional validation function to check if value is valid
     * @return true if value was published, false otherwise
     */
    bool update(const T& new_val, std::function<void(T)> publisher, 
                std::function<bool(T)> validator = nullptr) {
        // Validate the new value if validator provided
        if (validator && !validator(new_val)) return false;
        
        if (new_val == candidate) {
            // Same value as candidate - increment count
            if (++count >= min_repeats) {
                // Reached required repetitions
                if (!initialized || new_val != last_value) {
                    last_value = new_val;
                    publisher(new_val);
                    initialized = true;
                }
                count = 0;  // Reset count after publishing
                return true;
            }
        } else {
            // New different value - reset with new candidate
            candidate = new_val;
            count = 1;
        }
        return false;
    }

    /**
     * Reset filter state
     * Used when device state changes or connection is lost
     */
    void reset() {
        count = 0;
        initialized = false;
    }
};

/**
 * Main component class for I2C Creality Pi Dryer
 * Monitors I2C communication between dryer display and mainboard
 * Extracts and publishes sensor data to Home Assistant
 */
class I2CCrealityPiDryer : public Component {
 public:
  // ESPHome component lifecycle methods
  void setup() override;                              // Initialize component
  void loop() override;                               // Main processing loop
  void dump_config() override;                        // Log configuration info
  float get_setup_priority() const override { return setup_priority::LATE; }  // Setup after I2C

  // Sensor setter methods (called during component initialization)
  void set_set_temp_sensor(sensor::Sensor *sensor) { set_temp_sensor_ = sensor; }
  void set_current_temp_sensor(sensor::Sensor *sensor) { current_temp_sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { humidity_sensor_ = sensor; }
  void set_drying_time_sensor(text_sensor::TextSensor *sensor) { drying_time_sensor_ = sensor; }
  void set_material_sensor(text_sensor::TextSensor *sensor) { material_sensor_ = sensor; }
  void set_cursor_sensor(text_sensor::TextSensor *sensor) { cursor_sensor_ = sensor; }
  void set_temp_units_sensor(text_sensor::TextSensor *sensor) { temp_units_sensor_ = sensor; }
  void set_error_status_sensor(text_sensor::TextSensor *sensor) { error_status_sensor_ = sensor; }
  void set_dryer_status_sensor(text_sensor::TextSensor *sensor) { dryer_status_sensor_ = sensor; }

  // Configuration setter methods
  void set_scl_pin(uint8_t pin) { scl_pin_ = pin; }
  void set_sda_pin(uint8_t pin) { sda_pin_ = pin; }
  void set_enable_pullup(bool enable) { enable_pullup_ = enable; }
  void set_enable_statistics(bool enable) { enable_statistics_ = enable; }

  // Public members for ISR (Interrupt Service Routine) access
  // These must be volatile as they are modified in interrupt context
  volatile uint8_t buffer_[32];                       // I2C packet receive buffer
  volatile uint8_t byte_num_{0};                      // Current byte position in buffer
  volatile uint8_t bit_num_{0};                       // Current bit position in byte
  volatile uint8_t byte_tmp_{0};                      // Temporary byte being assembled
  volatile uint8_t i2c_status_{static_cast<uint8_t>(I2CStatus::READY)};  // Current I2C state
  volatile uint32_t last_edge_time_{0};               // Timestamp of last I2C edge (for timeout)
  volatile bool wait_ack_{false};                     // Waiting for ACK bit flag
  uint8_t scl_pin_;                                   // SCL GPIO pin number
  uint8_t sda_pin_;                                   // SDA GPIO pin number

 protected:
  // Timing and protocol constants
  static constexpr uint32_t I2C_TIMEOUT_US = 200;         // I2C timeout in microseconds
  static constexpr uint32_t DEVICE_TIMEOUT_MS = 3000;     // Device disconnection timeout (3 seconds)
  static constexpr uint32_t LOG_INTERVAL_MS = 30000;      // Debug logging interval (30 seconds)
  static constexpr uint8_t I2C_DEVICE_ADDRESS = 0x7E;     // Expected I2C device address
  static constexpr uint8_t BUFFER_SIZE = 32;              // Size of receive buffer
  static constexpr uint8_t DIG_COUNT = 11;                // Number of digit patterns (0-9 + E)
  static constexpr uint8_t MATERIAL_COUNT = 12;           // Number of supported materials

  // Static lookup tables for decoding
  static const uint8_t DIGITS[DIG_COUNT];                 // 7-segment digit bit patterns
  static const uint8_t MATERIAL_XOR[MATERIAL_COUNT];      // XOR checksums for material identification
  static const char* MATERIAL_NAME[MATERIAL_COUNT];       // Material name strings

  // Configuration flags
  bool enable_pullup_{false};        // Enable internal pull-up resistors on I2C pins
  bool enable_statistics_{true};     // Enable packet statistics tracking

  // Sensor pointers (linked during component initialization)
  sensor::Sensor *set_temp_sensor_{nullptr};              // Target temperature sensor
  sensor::Sensor *current_temp_sensor_{nullptr};          // Current temperature sensor
  sensor::Sensor *humidity_sensor_{nullptr};              // Humidity sensor
  text_sensor::TextSensor *drying_time_sensor_{nullptr};  // Remaining time text sensor
  text_sensor::TextSensor *material_sensor_{nullptr};     // Material type text sensor
  text_sensor::TextSensor *cursor_sensor_{nullptr};       // Menu cursor position text sensor
  text_sensor::TextSensor *temp_units_sensor_{nullptr};   // Temperature units (C/F) text sensor
  text_sensor::TextSensor *error_status_sensor_{nullptr}; // Error code text sensor
  text_sensor::TextSensor *dryer_status_sensor_{nullptr}; // Dryer status text sensor

  // Statistics structure for packet tracking
  struct Statistics {
    uint32_t total_interrupts = 0;   // Total I2C interrupts received
    uint32_t valid_packets = 0;      // Valid packets processed
    uint32_t invalid_packets = 0;    // Invalid/malformed packets rejected
  };
  Statistics stats_;

  // Value filters (debouncing with configurable repeat counts)
  FilteredValue<uint8_t> set_temp_filter_{0, 5};          // Set temp: 5 repeats required
  FilteredValue<uint8_t> process_temp_filter_{0, 5};      // Current temp: 5 repeats required
  FilteredValue<uint8_t> humidity_filter_{255, 3};        // Humidity: 3 repeats required
  FilteredValue<uint32_t> time_filter_{0, 2};             // Time: 2 repeats required
  FilteredValue<int> material_filter_{-1, 10};            // Material: 10 repeats (slow change)
  FilteredValue<const char*> cursor_filter_{"Unknown", 3}; // Cursor: 3 repeats required

  /**
   * Error state tracking structure
   * Implements robust error detection with debouncing to prevent false error triggers
   */
  struct ErrorState {
    char last_error[4] = "OK";          // Last confirmed error code
    char candidate_error[4] = "OK";     // Candidate error being evaluated
    uint8_t error_count = 0;            // Consecutive error readings count
    uint8_t clear_count = 0;            // Consecutive normal readings count
    uint8_t min_error_repeats = 3;      // Min repeats to confirm error (prevents false positives)
    uint8_t min_clear_repeats = 10;      // Min repeats to clear error (prevents flickering)
    bool error_active = false;          // Whether error is currently active
  };
  ErrorState error_state_;

  // Device state tracking
  DeviceState device_state_{DeviceState::OFF};  // Current device operational state
  bool last_error_state_{false};                // Previous error state (for edge detection)
  uint8_t last_rh_{255};                        // Last humidity value (for change detection)
  const char* last_units_{"Unknown"};           // Last temperature units (C/F)
  const char* last_cursor_{"Unknown"};          // Last cursor position

  // Timing variables
  uint32_t last_packet_time_{0};    // Timestamp of last valid packet (for timeout detection)
  uint32_t last_log_time_{0};       // Timestamp of last debug log (for periodic logging)
  uint32_t last_yield_time_{0};     // Timestamp of last yield() call (for WiFi/API responsiveness)

  // Validation lambda functions for filtering
  // Temperature validator: < 225 (not error code) and <= 80°C (reasonable range)
  std::function<bool(uint8_t)> validate_temp_ = [](uint8_t val) { 
    return val < 225 && val <= 80; 
  };
  
  // Time validator: <= 172800 seconds (48 hours max)
  std::function<bool(uint32_t)> validate_time_ = [](uint32_t val) { 
    return val <= 172800;
  };
  
  // Material index validator: 0-11 (12 materials supported)
  std::function<bool(int)> validate_material_ = [](int idx) { 
    return idx >= 0 && idx < 12; 
  };
  
  // Cursor validator: not "Unknown"
  std::function<bool(const char*)> validate_cursor_ = [](const char* cur) { 
    return strcmp(cur, "Unknown") != 0; 
  };

  // Protected method declarations
  
  /**
   * Handle I2C and device timeouts
   * Called in loop() to detect communication loss
   */
  void handle_timeouts();
  
  /**
   * Process received I2C packet
   * Decodes data and updates sensors
   */
  void process_packet();
  
  /**
   * Decode 7-segment display digit from two bytes
   * @param high High byte of digit (tens place)
   * @param low Low byte of digit (ones place)
   * @param is_temp Whether this is a temperature value (affects decoding)
   * @return Decoded numeric value or 255 if invalid
   */
  uint8_t decode_digit(uint8_t high, uint8_t low, bool is_temp = false);
  
  /**
   * Decode material index from buffer XOR checksum
   * @param buf Pointer to I2C packet buffer
   * @return Material index (0-11) or -1 if unknown
   */
  int decode_material_idx(const volatile uint8_t* buf);
  
  /**
   * Get cursor name from byte value
   * @param val Cursor state byte
   * @return Cursor name string ("Idle", "Time", "Material", etc.)
   */
  const char* get_cursor_name(uint8_t val);
  
  /**
   * Get temperature units from byte value
   * @param val Units byte
   * @return Units string ("C", "F", or "Unknown")
   */
  const char* get_units(uint8_t val);
  
  /**
   * Calculate Hamming distance between two bytes
   * Used for error correction in digit decoding
   * @param a First byte
   * @param b Second byte
   * @return Number of differing bits
   */
  uint8_t hamming_distance(uint8_t a, uint8_t b);
  
  /**
   * Handle error code detection and filtering
   * Implements robust error detection with debouncing
   * @param pv Process value (225 = error indicator)
   * @param high_byte High byte of error code
   * @param low_byte Low byte of error code
   */
  void handle_pv_error(uint8_t pv, uint8_t high_byte, uint8_t low_byte);
  
  /**
   * Filter and publish all sensor values
   * Applies filtering and validation before publishing
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
  void filter_and_publish_values(uint8_t sv, uint8_t pv, uint8_t rh, 
                                 uint8_t hh, uint8_t mm, uint8_t ss, 
                                 int mat_idx, const char* cursor, 
                                 const char* units);
  
  /**
   * Handle device state transitions
   * @param new_state New device state
   */
  void handle_device_state_change(DeviceState new_state);
  
  /**
   * Reset all value filters
   * Called when device reconnects or state changes
   */
  void reset_filters();
  
  /**
   * Reset all state variables
   * Called when device disconnects
   */
  void reset_all_states();

  // Friend declarations for ISR functions (allow access to private members)
  friend void IRAM_ATTR handle_scl_interrupt();  // SCL (clock) interrupt handler
  friend void IRAM_ATTR handle_sda_interrupt();  // SDA (data) interrupt handler
};

}  // namespace i2c_creality_pi_dryer
}  // namespace esphome
