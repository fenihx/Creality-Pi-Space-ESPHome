"""
ESPHome Custom Component: I2C Creality Pi Dryer
================================================
This component enables ESPHome to monitor I2C communication between
the Creality Pi Space Plus dryer's display and mainboard, extracting
real-time data about temperature, humidity, drying time, material type,
cursor state, and error status without interfering with normal operation.

Component Type: External Component
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor
from esphome.const import CONF_ID

# Component dependencies and auto-loading
# DEPENDENCIES: List of other ESPHome components this component depends on (empty = no dependencies)
# AUTO_LOAD: List of components to automatically load when this component is used (empty = none)
DEPENDENCIES = []
AUTO_LOAD = []

# Configuration constants
# These define the YAML configuration keys that users can specify
CONF_SCL_PIN = "scl_pin"                    # I2C clock line GPIO pin
CONF_SDA_PIN = "sda_pin"                    # I2C data line GPIO pin
CONF_ENABLE_PULLUP = "enable_pullup"        # Enable internal pull-up resistors on I2C pins
CONF_ENABLE_STATISTICS = "enable_statistics"  # Enable packet statistics tracking

# Sensor ID mapping constants
# These link the component's internal sensors to user-defined sensor IDs in YAML
CONF_SET_TEMP = "set_temp_id"              # Target/set temperature sensor
CONF_CURRENT_TEMP = "current_temp_id"      # Current/measured temperature sensor
CONF_HUMIDITY = "humidity_id"              # Humidity level sensor
CONF_DRYING_TIME = "drying_time_id"        # Remaining drying time text sensor
CONF_MATERIAL = "material_id"              # Selected material type text sensor
CONF_CURSOR = "cursor_id"                  # Menu cursor position text sensor
CONF_TEMP_UNITS = "temp_units_id"          # Temperature units (C/F) text sensor
CONF_ERROR_STATUS = "error_status_id"      # Error code/status text sensor
CONF_DRYER_STATUS = "dryer_status_id"      # Dryer operational status text sensor

# Namespace and class definition
# Creates the C++ namespace and class reference for code generation
i2c_creality_pi_dryer_ns = cg.esphome_ns.namespace("i2c_creality_pi_dryer")
I2CCrealityPiDryer = i2c_creality_pi_dryer_ns.class_("I2CCrealityPiDryer", cg.Component)

# Configuration schema
# Defines the structure and validation rules for YAML configuration
CONFIG_SCHEMA = cv.Schema({
    # Component ID - automatically generated or user-specified
    cv.GenerateID(): cv.declare_id(I2CCrealityPiDryer),
    
    # GPIO pin configuration
    # Optional: User can override default pins (GPIO22 for SCL, GPIO21 for SDA)
    # Valid range: 0-39 (ESP32 GPIO pin numbers)
    cv.Optional(CONF_SCL_PIN, default=22): cv.int_range(min=0, max=39),
    cv.Optional(CONF_SDA_PIN, default=21): cv.int_range(min=0, max=39),
    
    # Feature flags
    # Optional: Enable/disable internal pull-up resistors (default: disabled)
    cv.Optional(CONF_ENABLE_PULLUP, default=False): cv.boolean,
    # Optional: Enable/disable packet statistics logging (default: disabled)
    cv.Optional(CONF_ENABLE_STATISTICS, default=False): cv.boolean,
    
    # Required sensor references
    # These must be defined in the YAML configuration and linked to actual sensors
    # Numeric sensors (temperature and humidity)
    cv.Required(CONF_SET_TEMP): cv.use_id(sensor.Sensor),
    cv.Required(CONF_CURRENT_TEMP): cv.use_id(sensor.Sensor),
    cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
    
    # Text sensors (time, material, states, errors)
    cv.Required(CONF_DRYING_TIME): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_MATERIAL): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_CURSOR): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_TEMP_UNITS): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_ERROR_STATUS): cv.use_id(text_sensor.TextSensor),
    cv.Required(CONF_DRYER_STATUS): cv.use_id(text_sensor.TextSensor),
}).extend(cv.COMPONENT_SCHEMA)  # Extend with standard component schema (includes setup_priority, etc.)


async def to_code(config):
    
    # Create a new C++ variable for this component instance
    var = cg.new_Pvariable(config[CONF_ID])
    
    # Register this as an ESPHome component (enables setup(), loop(), etc.)
    await cg.register_component(var, config)

    # Configure GPIO pins for I2C monitoring
    # These setter calls generate C++ code: var->set_scl_pin(22);
    cg.add(var.set_scl_pin(config[CONF_SCL_PIN]))
    cg.add(var.set_sda_pin(config[CONF_SDA_PIN]))
    
    # Configure feature flags
    cg.add(var.set_enable_pullup(config[CONF_ENABLE_PULLUP]))
    cg.add(var.set_enable_statistics(config[CONF_ENABLE_STATISTICS]))

    # Link numeric sensors
    # Retrieve sensor references from config and link them to the component
    
    # Set temperature sensor (target/desired temperature)
    set_temp = await cg.get_variable(config[CONF_SET_TEMP])
    cg.add(var.set_set_temp_sensor(set_temp))

    # Current temperature sensor (measured/process temperature)
    current_temp = await cg.get_variable(config[CONF_CURRENT_TEMP])
    cg.add(var.set_current_temp_sensor(current_temp))

    # Humidity sensor (relative humidity percentage)
    humidity = await cg.get_variable(config[CONF_HUMIDITY])
    cg.add(var.set_humidity_sensor(humidity))

    # Link text sensors
    # Retrieve text sensor references and link them to the component
    
    # Drying time sensor (format: HH:MM:SS)
    drying_time = await cg.get_variable(config[CONF_DRYING_TIME])
    cg.add(var.set_drying_time_sensor(drying_time))

    # Material type sensor (e.g., "PLA", "ABS", "PETG")
    material = await cg.get_variable(config[CONF_MATERIAL])
    cg.add(var.set_material_sensor(material))

    # Cursor position sensor (e.g., "Idle", "Time", "Material", "SV", "PV")
    cursor = await cg.get_variable(config[CONF_CURSOR])
    cg.add(var.set_cursor_sensor(cursor))

    # Temperature units sensor (displays "C" or "F")
    temp_units = await cg.get_variable(config[CONF_TEMP_UNITS])
    cg.add(var.set_temp_units_sensor(temp_units))

    # Error status sensor (displays error codes like "E0", "E5", or "OK")
    error_status = await cg.get_variable(config[CONF_ERROR_STATUS])
    cg.add(var.set_error_status_sensor(error_status))
    
    # Dryer operational status sensor (displays "Off", "Idle", "Drying", or "Error")
    dryer_status = await cg.get_variable(config[CONF_DRYER_STATUS])
    cg.add(var.set_dryer_status_sensor(dryer_status))
