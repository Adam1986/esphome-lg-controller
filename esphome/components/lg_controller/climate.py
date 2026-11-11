import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor, button, climate, number, select, sensor, switch, text_sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_RX_PIN,
    CONF_INTERNAL,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HOUR,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
)

CODEOWNERS = ["JanM321"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "button", "number", "switch", "sensor", "select", "text_sensor"]

lg_controller_ns = cg.esphome_ns.namespace("lg_controller")
LgController = lg_controller_ns.class_(
    "LgController", cg.Component, uart.UARTDevice, climate.Climate
)
LgNumber = lg_controller_ns.class_("LgNumber", number.Number, cg.Component)
LgSelect = lg_controller_ns.class_("LgSelect", select.Select, cg.Component)
LgSwitch = lg_controller_ns.class_("LgSwitch", switch.Switch, cg.Component)
LgPipeTemperatureRefreshButton = lg_controller_ns.class_(
    "LgPipeTemperatureRefreshButton", button.Button
)

CONF_FAHRENHEIT = "fahrenheit"
CONF_IS_SLAVE_CONTROLLER = "is_slave_controller"
CONF_TARGET_TEMPERATURE_STEP = "target_temperature_step"
CONF_OVERCOOLING_SETTING = "overcooling_setting"

CONF_TEMPERATURE_SENSOR = "temperature_sensor"

CONF_VANE1 = "vane1"
CONF_VANE2 = "vane2"
CONF_VANE3 = "vane3"
CONF_VANE4 = "vane4"
CONF_OVERHEATING = "overheating"

CONF_FAN_SPEED_SLOW = "fan_speed_slow"
CONF_FAN_SPEED_LOW = "fan_speed_low"
CONF_FAN_SPEED_MEDIUM = "fan_speed_medium"
CONF_FAN_SPEED_HIGH = "fan_speed_high"
CONF_SLEEP_TIMER = "sleep_timer"

CONF_ERROR_CODE = "error_code"
CONF_PIPE_TEMP_IN = "pipe_temp_in"
CONF_PIPE_TEMP_MID = "pipe_temp_mid"
CONF_PIPE_TEMP_OUT = "pipe_temp_out"
CONF_REFRESH_PIPE_TEMPERATURES = "refresh_pipe_temperatures"
CONF_POWER_CONSUMPTION = "power_consumption"
CONF_CURRENT_POWER = "current_power"
CONF_HUMIDITY = "humidity"
CONF_FAN_OPERATION_TIME = "fan_operation_time"
CONF_INDOOR_UNIT_OPERATION_TIME = "indoor_unit_operation_time"
CONF_PRECISE_ROOM_TEMPERATURE = "precise_room_temperature"
CONF_OVERCOOLING = "overcooling"
CONF_COOLING_THERMAL_OFF_FAN = "cooling_thermal_off_fan"
CONF_TEMPERATURE_CONTROL_STEP = "temperature_control_step"
CONF_STATIC_PRESSURE_STEP = "static_pressure_step"
CONF_CONTINUOUS_FAN = "continuous_fan"
CONF_TEMPERATURE_BASED_FAN_AUTO = "temperature_based_fan_auto"
CONF_FLOOR_CONSOLE_MONITORING = "floor_console_monitoring"
CONF_FLOOR_CONSOLE_FEATURES = "floor_console_features"
CONF_FLOOR_CONSOLE_REVERSED_AIRFLOW = "floor_console_reversed_airflow"
CONF_FLOOR_CONSOLE_PASSIVE_READBACK = "floor_console_passive_readback"
CONF_FLOOR_CONSOLE_SELECTIVE_CD_PROBE = "floor_console_selective_cd_probe"
CONF_FLOOR_CONSOLE_FAST_DISCHARGE = "floor_console_fast_discharge"
CONF_FLOOR_CONSOLE_OPTIMISTIC_DISCHARGE = "floor_console_optimistic_discharge"
CONF_FLOOR_CONSOLE_FAST_VERIFY_QUIET_SECONDS = "floor_console_fast_verify_quiet_seconds"
# EXPERIMENTAL: actively read full diagnostics while genuine Master sleep is active.
# Only affects Slave Type-B startup discovery; never touches normal climate/sleep writes.
CONF_SLAVE_SLEEP_SAFE_DISCOVERY = "slave_sleep_safe_discovery"
CONF_FILTER_TIME_REMAINING = "filter_time_remaining"
CONF_FILTER_REMAINING_PERCENT = "filter_remaining_percent"
CONF_FILTER_NOMINAL_LIFE_HOURS = "filter_nominal_life_hours"
CONF_FLOOR_CONSOLE_COMFORT_CONTROL = "floor_console_comfort_control"
CONF_FLOOR_CONSOLE_DIRECTION_CONTROL = "floor_console_direction_control"
CONF_FLOOR_CONSOLE_STEP_CONTROL = "floor_console_step_control"
CONF_FLOOR_CONSOLE_COMMAND_STATUS = "floor_console_command_status"
FLOOR_CONSOLE_OBSERVATIONS = {
    "floor_console_comfort_master": "Floor - Comfort Cooling (Master)",
    "floor_console_comfort_indoor": "Floor - Comfort Cooling (Indoor)",
    "floor_console_direction_master": "Floor - Discharge Direction (Master)",
    "floor_console_direction_indoor": "Floor - Discharge Direction (Indoor)",
    "floor_console_step_master": "Floor - Comfort Cooling Step (Master)",
    "floor_console_step_indoor": "Floor - Comfort Cooling Step (Indoor)",
    "floor_console_comfort_capability": "Floor - Comfort Cooling Capability (C9)",
}
CONF_ADVANCED_SETTINGS_RAW = "advanced_settings_raw"
CONF_EXTENDED_SETTINGS_RAW = "extended_settings_raw"

# Read-only installer diagnostics. These only register entities in Slave mode.
CONF_INSTALLER_CENTRAL_CONTROL_ADDRESS_STATUS = "installer_central_control_address_status"
CONF_INSTALLER_OVERHEATING_STATUS = "installer_overheating_status"
CONF_INSTALLER_OVERCOOLING_STATUS = "installer_overcooling_status"
CONF_INSTALLER_COOLING_THERMAL_OFF_FAN_STATUS = "installer_cooling_thermal_off_fan_status"
CONF_INSTALLER_TEMPERATURE_CONTROL_STEP_STATUS = "installer_temperature_control_step_status"
CONF_INSTALLER_CONTINUOUS_FAN_STATUS = "installer_continuous_fan_status"
CONF_INSTALLER_TEMPERATURE_BASED_FAN_AUTO_STATUS = "installer_temperature_based_fan_auto_status"
CONF_INSTALLER_FAN_SPEED_SLOW_STATUS = "installer_fan_speed_slow_status"
CONF_INSTALLER_FAN_SPEED_LOW_STATUS = "installer_fan_speed_low_status"
CONF_INSTALLER_FAN_SPEED_MEDIUM_STATUS = "installer_fan_speed_medium_status"
CONF_INSTALLER_FAN_SPEED_HIGH_STATUS = "installer_fan_speed_high_status"

INSTALLER_TEXT_DIAGNOSTICS = (
    CONF_INSTALLER_CENTRAL_CONTROL_ADDRESS_STATUS,
    CONF_INSTALLER_OVERHEATING_STATUS,
    CONF_INSTALLER_OVERCOOLING_STATUS,
    CONF_INSTALLER_COOLING_THERMAL_OFF_FAN_STATUS,
    CONF_INSTALLER_TEMPERATURE_CONTROL_STEP_STATUS,
    CONF_INSTALLER_CONTINUOUS_FAN_STATUS,
    CONF_INSTALLER_TEMPERATURE_BASED_FAN_AUTO_STATUS,
)
INSTALLER_NUMBER_DIAGNOSTICS = (
    CONF_INSTALLER_FAN_SPEED_SLOW_STATUS,
    CONF_INSTALLER_FAN_SPEED_LOW_STATUS,
    CONF_INSTALLER_FAN_SPEED_MEDIUM_STATUS,
    CONF_INSTALLER_FAN_SPEED_HIGH_STATUS,
)

CONF_DEFROST = "defrost"
CONF_PREHEAT = "preheat"
CONF_OUTDOOR = "outdoor"
CONF_AUTO_DRY_ACTIVE = "auto_dry_active"

CONF_PURIFIER = "purifier"
CONF_INTERNAL_THERMISTOR = "internal_thermistor"
CONF_AUTO_DRY = "auto_dry"

VANE_OPTIONS = ["0 (Default)", "1 (Up)", "2", "3", "4", "5", "6 (Down)"]
FLOOR_CONSOLE_VANE_OPTIONS = ["6 (Up)", "5", "4", "3", "2", "1 (Down)", "0 (Default)"]
OVERHEATING_OPTIONS = ["0 (Default)", "1 (+4C/+6C)", "2 (+2C/+4C)", "3 (-1C/+1C)", "4 (-0.5C/+0.5C)"]
OVERCOOLING_OPTIONS = ["0 (Default: +0.5C/-0.5C)", "1 (+6C/+4C)", "2 (+4C/+2C)", "3 (+1C/-1C)"]
TEMPERATURE_CONTROL_STEP_OPTIONS = ["0 (1C)", "1 (0.5C)"]
COOLING_THERMAL_OFF_FAN_OPTIONS = ["0 (Fan Low)", "1 (Fan Off)", "2 (Fan Setting)"]

# Prefer floor_console_* in new YAML. Accept older floorstanding_* keys so
# existing installations can update the component without editing device YAML.
# Resolve aliases BEFORE the main schema inserts optional default values.
LEGACY_FLOOR_CONSOLE_KEYS = {
    "floorstanding_comfort_capability": "floor_console_comfort_capability",
    "floorstanding_comfort_control": "floor_console_comfort_control",
    "floorstanding_comfort_indoor": "floor_console_comfort_indoor",
    "floorstanding_comfort_master": "floor_console_comfort_master",
    "floorstanding_command_status": "floor_console_command_status",
    "floorstanding_direction_control": "floor_console_direction_control",
    "floorstanding_direction_indoor": "floor_console_direction_indoor",
    "floorstanding_direction_master": "floor_console_direction_master",
    "floorstanding_features": "floor_console_features",
    "floorstanding_monitoring": "floor_console_monitoring",
    "floorstanding_reversed_airflow": "floor_console_reversed_airflow",
    "floorstanding_step_control": "floor_console_step_control",
    "floorstanding_step_indoor": "floor_console_step_indoor",
    "floorstanding_step_master": "floor_console_step_master",
}


def _normalize_floor_console_keys(config):
    if not isinstance(config, dict):
        return config
    normalized = dict(config)
    for old_key, new_key in LEGACY_FLOOR_CONSOLE_KEYS.items():
        if old_key not in normalized:
            continue
        if new_key in normalized:
            raise cv.Invalid(
                f"Use either '{new_key}' or its legacy alias '{old_key}', not both"
            )
        normalized[new_key] = normalized.pop(old_key)
    return normalized


CONFIG_SCHEMA = climate.climate_schema(LgController).extend(
    {
        cv.Required(CONF_RX_PIN): pins.gpio_input_pin_schema,

        cv.Required(CONF_FAHRENHEIT): cv.boolean,
        cv.Required(CONF_IS_SLAVE_CONTROLLER): cv.boolean,
        cv.Optional(CONF_TARGET_TEMPERATURE_STEP, default=0.5): cv.one_of(0.5, 1.0, float=True),
        # Installer setting 27 to write to the unit (0-3). -1 or omitted = echo the unit's value.
        cv.Optional(CONF_OVERCOOLING_SETTING, default=-1): cv.int_range(min=-1, max=3),

        cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),

        cv.Required(CONF_VANE1): select.select_schema(LgSelect),
        cv.Required(CONF_VANE2): select.select_schema(LgSelect),
        cv.Required(CONF_VANE3): select.select_schema(LgSelect),
        cv.Required(CONF_VANE4): select.select_schema(LgSelect),
        cv.Required(CONF_OVERHEATING): select.select_schema(LgSelect),

        cv.Required(CONF_FAN_SPEED_SLOW): number.number_schema(LgNumber),
        cv.Required(CONF_FAN_SPEED_LOW): number.number_schema(LgNumber),
        cv.Required(CONF_FAN_SPEED_MEDIUM): number.number_schema(LgNumber),
        cv.Required(CONF_FAN_SPEED_HIGH): number.number_schema(LgNumber),
        cv.Required(CONF_SLEEP_TIMER): number.number_schema(LgNumber),

        cv.Required(CONF_ERROR_CODE): sensor.sensor_schema(accuracy_decimals=0),
        cv.Required(CONF_PIPE_TEMP_IN): sensor.sensor_schema(),
        cv.Required(CONF_PIPE_TEMP_MID): sensor.sensor_schema(),
        cv.Required(CONF_PIPE_TEMP_OUT): sensor.sensor_schema(),
        # Slave-only momentary read button. Defaulted so existing YAML works
        # unchanged; master controllers do not register the additional entity.
        cv.Optional(CONF_REFRESH_PIPE_TEMPERATURES,
                    default={"name": "Refresh Pipe Temperatures"}):
            button.button_schema(
                LgPipeTemperatureRefreshButton,
                entity_category="diagnostic",
                icon="mdi:refresh",
            ),
        cv.Optional(CONF_POWER_CONSUMPTION): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_CURRENT_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FAN_OPERATION_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_INDOOR_UNIT_OPERATION_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_PRECISE_ROOM_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        # Installer setting 27 (overcooling) as select dropdown.
        cv.Optional(CONF_OVERCOOLING): select.select_schema(LgSelect),
        cv.Optional(CONF_COOLING_THERMAL_OFF_FAN): select.select_schema(LgSelect),
        # Installer settings: 17 (0 = 1 °C, 1 = 0.5 °C control), 32 (static pressure
        # step), 46 (continuous fan), 51 (temperature-based fan speed auto).
        cv.Optional(CONF_TEMPERATURE_CONTROL_STEP): select.select_schema(LgSelect),
        cv.Optional(CONF_STATIC_PRESSURE_STEP): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_CONTINUOUS_FAN): switch.switch_schema(LgSwitch),
        cv.Optional(CONF_TEMPERATURE_BASED_FAN_AUTO): switch.switch_schema(LgSwitch),
        # Raw hex of the unit's 0xCD frame and 0xCE.00/10/11/12 sub-frames, which are only
        # partly documented. Published only when they change.
        cv.Optional(CONF_ADVANCED_SETTINGS_RAW): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_EXTENDED_SETTINGS_RAW): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_FLOOR_CONSOLE_MONITORING, default=False): cv.boolean,
        cv.Optional(CONF_FLOOR_CONSOLE_FEATURES, default=False): cv.boolean,
        # This only changes select labels and UI-index translation. Raw LG codes
        # on the bus and CA/AA/2A protocol handling remain unchanged.
        cv.Optional(CONF_FLOOR_CONSOLE_REVERSED_AIRFLOW, default=False): cv.boolean,
        # Optional bounded experiment: listen for spontaneous indoor CD before a full 28 refresh.
        cv.Optional(CONF_FLOOR_CONSOLE_PASSIVE_READBACK, default=False): cv.boolean,
        # HIGHLY EXPERIMENTAL: send one 2D.80 candidate following a genuine
        # discharge write. Type-D selective reads are NOT documented/verified.
        cv.Optional(CONF_FLOOR_CONSOLE_SELECTIVE_CD_PROBE, default=False): cv.boolean,
        cv.Optional(CONF_FLOOR_CONSOLE_FAST_DISCHARGE, default=False): cv.boolean,
        # V2.0.6: opt-in immediate HA dropdown state for requested direction;
        # only used for Slave fast discharge, not other floor/normal controls.
        cv.Optional(CONF_FLOOR_CONSOLE_OPTIMISTIC_DISCHARGE, default=False): cv.boolean,
        # V2.0.5: postpone full settings readback during bursts of control
        # changes. 45s preserves previous fast-mode behaviour; 120s is an
        # optional Garden Office experiment, never enabled by default.
        cv.Optional(CONF_FLOOR_CONSOLE_FAST_VERIFY_QUIET_SECONDS, default=45): cv.int_range(min=45, max=180),
        # V2 default ON: proved functional in two active-reservation live tests.
        # Retain an optional per-device override; do not need this in either base.
        cv.Optional(CONF_SLAVE_SLEEP_SAFE_DISCOVERY, default=True): cv.boolean,
        # Indoor CC provides 12-bit remaining filter life in HOURS. Never
        # publish an inferred value if no genuine indoor CC reports arrive.
        cv.Optional(CONF_FILTER_TIME_REMAINING): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        # Percentage is only an estimate and requires a configured lifetime;
        # 0 disables percentage calculation. LG units have different lifetimes.
        cv.Optional(CONF_FILTER_NOMINAL_LIFE_HOURS, default=0): cv.int_range(min=0, max=4095),
        cv.Optional(CONF_FILTER_REMAINING_PERCENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_FLOOR_CONSOLE_COMFORT_CONTROL,
                    default={"name": "Floor - Comfort Cooling"}): switch.switch_schema(LgSwitch),
        cv.Optional(CONF_FLOOR_CONSOLE_DIRECTION_CONTROL,
                    default={"name": "Floor - Discharge Direction"}): select.select_schema(LgSelect),
        cv.Optional(CONF_FLOOR_CONSOLE_STEP_CONTROL,
                    default={"name": "Floor - Comfort Cooling Step"}): select.select_schema(LgSelect),
        cv.Optional(CONF_FLOOR_CONSOLE_COMMAND_STATUS,
                    default={"name": "Floor - Command Status"}): text_sensor.text_sensor_schema(entity_category="diagnostic"),
        **{cv.Optional(key, default={"name": label}):
           text_sensor.text_sensor_schema(entity_category="diagnostic")
           for key, label in FLOOR_CONSOLE_OBSERVATIONS.items()},

        # Current installer values read from CA/CB/CE on the wired bus.
        # In Slave mode they are real HA sensors, never switches/selects/numbers.
        # Defaults mean existing custom climate YAML files also get these
        # read-only diagnostics automatically in Slave mode (no YAML edits).
        **{cv.Optional(key, default=lambda key=key: {"name": "Installer - " + key[len("installer_"):-len("_status")].replace("_", " ").title()}):
           text_sensor.text_sensor_schema(entity_category="diagnostic")
           for key in INSTALLER_TEXT_DIAGNOSTICS},
        **{cv.Optional(key, default=lambda key=key: {"name":
               "Installer - " + key[len("installer_"):-len("_status")].replace("_", " ").title() + " (raw)"}):
           sensor.sensor_schema(accuracy_decimals=0, entity_category="diagnostic")
           for key in INSTALLER_NUMBER_DIAGNOSTICS},

        cv.Required(CONF_DEFROST): binary_sensor.binary_sensor_schema(),
        cv.Required(CONF_PREHEAT): binary_sensor.binary_sensor_schema(),
        cv.Required(CONF_OUTDOOR): binary_sensor.binary_sensor_schema(),
        cv.Required(CONF_AUTO_DRY_ACTIVE): binary_sensor.binary_sensor_schema(),

        cv.Required(CONF_PURIFIER): switch.switch_schema(LgSwitch),
        cv.Required(CONF_INTERNAL_THERMISTOR): switch.switch_schema(LgSwitch),
        cv.Required(CONF_AUTO_DRY): switch.switch_schema(LgSwitch),
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

# Convert legacy names before the canonical schema validates and supplies defaults.
CONFIG_SCHEMA = cv.All(_normalize_floor_console_keys, CONFIG_SCHEMA)


async def to_code(config):
    rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])

    if CONF_TEMPERATURE_SENSOR in config:
        temperature_sensor = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
    else:
        temperature_sensor = cg.nullptr

    # Hide entities not supported for 'slave' controllers.
    if config[CONF_IS_SLAVE_CONTROLLER]:
        # Experimental installer ESP calibrations are user-writeable, but
        # all writes are gated on live unit CA + safe A8/C8 state in C++.
        config[CONF_OVERHEATING][CONF_INTERNAL] = True
        config[CONF_INTERNAL_THERMISTOR][CONF_INTERNAL] = True

    vane_options = (FLOOR_CONSOLE_VANE_OPTIONS if config[CONF_FLOOR_CONSOLE_REVERSED_AIRFLOW]
                    else VANE_OPTIONS)
    vane1 = await select.new_select(config[CONF_VANE1], options=vane_options)
    vane2 = await select.new_select(config[CONF_VANE2], options=vane_options)
    vane3 = await select.new_select(config[CONF_VANE3], options=vane_options)
    vane4 = await select.new_select(config[CONF_VANE4], options=vane_options)
    overheating = await select.new_select(config[CONF_OVERHEATING], options=OVERHEATING_OPTIONS)

    fan_speed_slow = await number.new_number(config[CONF_FAN_SPEED_SLOW], min_value=0, max_value=255, step=1)
    fan_speed_low = await number.new_number(config[CONF_FAN_SPEED_LOW], min_value=0, max_value=255, step=1)
    fan_speed_medium = await number.new_number(config[CONF_FAN_SPEED_MEDIUM], min_value=0, max_value=255, step=1)
    fan_speed_high = await number.new_number(config[CONF_FAN_SPEED_HIGH], min_value=0, max_value=255, step=1)
    sleep_timer = await number.new_number(config[CONF_SLEEP_TIMER], min_value=0, max_value=480, step=1)

    error_code = await sensor.new_sensor(config[CONF_ERROR_CODE])
    pipe_temp_in = await sensor.new_sensor(config[CONF_PIPE_TEMP_IN])
    pipe_temp_mid = await sensor.new_sensor(config[CONF_PIPE_TEMP_MID])
    pipe_temp_out = await sensor.new_sensor(config[CONF_PIPE_TEMP_OUT])
    if CONF_POWER_CONSUMPTION in config:
        power_consumption = await sensor.new_sensor(config[CONF_POWER_CONSUMPTION])
    else:
        power_consumption = cg.nullptr
    if CONF_CURRENT_POWER in config:
        current_power = await sensor.new_sensor(config[CONF_CURRENT_POWER])
    else:
        current_power = cg.nullptr
    if CONF_HUMIDITY in config:
        humidity = await sensor.new_sensor(config[CONF_HUMIDITY])
    else:
        humidity = cg.nullptr
    if CONF_FAN_OPERATION_TIME in config:
        fan_operation_time = await sensor.new_sensor(config[CONF_FAN_OPERATION_TIME])
    else:
        fan_operation_time = cg.nullptr
    if CONF_INDOOR_UNIT_OPERATION_TIME in config:
        indoor_unit_operation_time = await sensor.new_sensor(config[CONF_INDOOR_UNIT_OPERATION_TIME])
    else:
        indoor_unit_operation_time = cg.nullptr
    if CONF_PRECISE_ROOM_TEMPERATURE in config:
        precise_room_temperature = await sensor.new_sensor(config[CONF_PRECISE_ROOM_TEMPERATURE])
    else:
        precise_room_temperature = cg.nullptr
    if not config[CONF_IS_SLAVE_CONTROLLER] and CONF_OVERCOOLING in config:
        overcooling = await select.new_select(config[CONF_OVERCOOLING], options=OVERCOOLING_OPTIONS)
    else:
        overcooling = cg.nullptr
    if not config[CONF_IS_SLAVE_CONTROLLER] and CONF_COOLING_THERMAL_OFF_FAN in config:
        cooling_thermal_off_fan = await select.new_select(
            config[CONF_COOLING_THERMAL_OFF_FAN], options=COOLING_THERMAL_OFF_FAN_OPTIONS
        )
    else:
        cooling_thermal_off_fan = cg.nullptr

    defrost = await binary_sensor.new_binary_sensor(config[CONF_DEFROST])
    preheat = await binary_sensor.new_binary_sensor(config[CONF_PREHEAT])
    outdoor = await binary_sensor.new_binary_sensor(config[CONF_OUTDOOR])
    auto_dry_active = await binary_sensor.new_binary_sensor(config[CONF_AUTO_DRY_ACTIVE])

    purifier = await switch.new_switch(config[CONF_PURIFIER])
    internal_thermistor = await switch.new_switch(config[CONF_INTERNAL_THERMISTOR])
    auto_dry = await switch.new_switch(config[CONF_AUTO_DRY])

    var = cg.new_Pvariable(config[CONF_ID], rx_pin, temperature_sensor,
                           vane1, vane2, vane3, vane4, overheating,
                           fan_speed_slow, fan_speed_low, fan_speed_medium, fan_speed_high,
                           sleep_timer,
                           error_code, pipe_temp_in, pipe_temp_mid, pipe_temp_out,
                           power_consumption, current_power,
                           humidity, fan_operation_time, indoor_unit_operation_time,
                           precise_room_temperature,
                           overcooling, cooling_thermal_off_fan,
                           defrost, preheat, outdoor, auto_dry_active,
                           purifier, internal_thermistor, auto_dry,
                           config[CONF_FAHRENHEIT], config[CONF_IS_SLAVE_CONTROLLER])
    # The button is registered automatically in Slave mode. Its press handler
    # never sends on UART directly: it queues the existing timed Type-B read.
    if config[CONF_IS_SLAVE_CONTROLLER]:
        await button.new_button(config[CONF_REFRESH_PIPE_TEMPERATURES], var)

    # Never use an explicit installer overcooling target while acting as Slave.
    cg.add(var.set_overcooling_target(-1 if config[CONF_IS_SLAVE_CONTROLLER] else config[CONF_OVERCOOLING_SETTING]))
    cg.add(var.set_target_temperature_step(config[CONF_TARGET_TEMPERATURE_STEP]))
    if not config[CONF_IS_SLAVE_CONTROLLER] and CONF_TEMPERATURE_CONTROL_STEP in config:
        step_sel = await select.new_select(
            config[CONF_TEMPERATURE_CONTROL_STEP], options=TEMPERATURE_CONTROL_STEP_OPTIONS
        )
        cg.add(var.set_temperature_control_step_select(step_sel))
    if CONF_STATIC_PRESSURE_STEP in config:
        sens = await sensor.new_sensor(config[CONF_STATIC_PRESSURE_STEP])
        cg.add(var.set_static_pressure_step_sensor(sens))
    if not config[CONF_IS_SLAVE_CONTROLLER] and CONF_CONTINUOUS_FAN in config:
        cont_fan = await switch.new_switch(config[CONF_CONTINUOUS_FAN])
        cg.add(var.set_continuous_fan_switch(cont_fan))
    if not config[CONF_IS_SLAVE_CONTROLLER] and CONF_TEMPERATURE_BASED_FAN_AUTO in config:
        fan_auto = await switch.new_switch(config[CONF_TEMPERATURE_BASED_FAN_AUTO])
        cg.add(var.set_temperature_based_fan_auto_switch(fan_auto))
    for key in (CONF_ADVANCED_SETTINGS_RAW, CONF_EXTENDED_SETTINGS_RAW):
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_text_sensor")(sens))
    # Opt-in read-only floor console observations, on either Master or Slave.
    # Master/indoor sources are distinct and are never inferred from local writes.
    cg.add(var.set_floor_console_monitoring(config[CONF_FLOOR_CONSOLE_MONITORING]))
    cg.add(var.set_floor_console_features(config[CONF_FLOOR_CONSOLE_FEATURES]))
    cg.add(var.set_floor_console_reversed_airflow(config[CONF_FLOOR_CONSOLE_REVERSED_AIRFLOW]))
    cg.add(var.set_floor_console_passive_readback(config[CONF_FLOOR_CONSOLE_PASSIVE_READBACK]))
    cg.add(var.set_floor_console_selective_cd_probe(config[CONF_FLOOR_CONSOLE_SELECTIVE_CD_PROBE]))
    cg.add(var.set_floor_console_fast_discharge(config[CONF_FLOOR_CONSOLE_FAST_DISCHARGE]))
    if config[CONF_FLOOR_CONSOLE_OPTIMISTIC_DISCHARGE] and not (
        config[CONF_FLOOR_CONSOLE_FEATURES] and config[CONF_IS_SLAVE_CONTROLLER]
        and config[CONF_FLOOR_CONSOLE_FAST_DISCHARGE]
    ):
        raise cv.Invalid("floor_console_optimistic_discharge requires Floor Console features, Slave mode and fast discharge")
    cg.add(var.set_floor_console_optimistic_discharge(config[CONF_FLOOR_CONSOLE_OPTIMISTIC_DISCHARGE]))
    cg.add(var.set_floor_console_fast_verify_quiet_seconds(config[CONF_FLOOR_CONSOLE_FAST_VERIFY_QUIET_SECONDS]))
    cg.add(var.set_slave_sleep_safe_discovery(config[CONF_SLAVE_SLEEP_SAFE_DISCOVERY]))
    if CONF_FILTER_TIME_REMAINING in config:
        filt = await sensor.new_sensor(config[CONF_FILTER_TIME_REMAINING])
        cg.add(var.set_filter_time_remaining_sensor(filt))
    cg.add(var.set_filter_nominal_life_hours(config[CONF_FILTER_NOMINAL_LIFE_HOURS]))
    if CONF_FILTER_REMAINING_PERCENT in config:
        if config[CONF_FILTER_NOMINAL_LIFE_HOURS] == 0:
            raise cv.Invalid("filter_remaining_percent needs filter_nominal_life_hours > 0")
        filt_percent = await sensor.new_sensor(config[CONF_FILTER_REMAINING_PERCENT])
        cg.add(var.set_filter_remaining_percent_sensor(filt_percent))
    if config[CONF_FLOOR_CONSOLE_FEATURES]:
        comfort = await switch.new_switch(config[CONF_FLOOR_CONSOLE_COMFORT_CONTROL])
        direction = await select.new_select(config[CONF_FLOOR_CONSOLE_DIRECTION_CONTROL],
                                            options=["Both", "Top", "Bottom"])
        step = await select.new_select(config[CONF_FLOOR_CONSOLE_STEP_CONTROL],
                                       options=["Step 1", "Step 2", "Step 3"])
        cg.add(var.set_floor_console_controls(comfort, direction, step))
        status = await text_sensor.new_text_sensor(config[CONF_FLOOR_CONSOLE_COMMAND_STATUS])
        cg.add(var.set_floor_console_command_status_text_sensor(status))
    if config[CONF_FLOOR_CONSOLE_MONITORING] or config[CONF_FLOOR_CONSOLE_FEATURES]:
        for key in FLOOR_CONSOLE_OBSERVATIONS:
            obs = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_text_sensor")(obs))

    # Read-only entities are only registered when the compiled controller is Slave.
    # Unavailable values remain unknown until a genuine indoor-unit frame arrives.
    if config[CONF_IS_SLAVE_CONTROLLER]:
        for key in INSTALLER_TEXT_DIAGNOSTICS:
            if key in config:
                diag = await text_sensor.new_text_sensor(config[key])
                cg.add(getattr(var, f"set_{key}_text_sensor")(diag))
        for key in INSTALLER_NUMBER_DIAGNOSTICS:
            if key in config:
                diag = await sensor.new_sensor(config[key])
                cg.add(getattr(var, f"set_{key}_sensor")(diag))
    await climate.register_climate(var, config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
