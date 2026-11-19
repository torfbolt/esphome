import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from . import CONF_WISOTRONIC_ID, WISOTRONIC_COMPONENT_SCHEMA

DEPENDENCIES = ["uart"]

# ------------------------------------------------------------------------------
# List of all sensors in the hub
# ------------------------------------------------------------------------------
TYPES = [
    "control_unit_temperature",
    "outdoor_temperature",
    "indoor_temperature",
    "indoor_humidity",
    "wind_speed",
    "rain",
    "photo1_brightness",
    "photo2_brightness",
    "photo3_brightness",
    "twilight_brightness",
]

# Map sensor types to their full sensor schema
SENSOR_SCHEMAS = {
    "control_unit_temperature": sensor.sensor_schema(
        unit_of_measurement="°C",
        device_class="temperature",
        accuracy_decimals=1,
    ),
    "outdoor_temperature": sensor.sensor_schema(
        unit_of_measurement="°C",
        device_class="temperature",
        accuracy_decimals=1,
    ),
    "indoor_temperature": sensor.sensor_schema(
        unit_of_measurement="°C",
        device_class="temperature",
        accuracy_decimals=1,
    ),
    "indoor_humidity": sensor.sensor_schema(
        unit_of_measurement="%",
        device_class="humidity",
        accuracy_decimals=1,
    ),
    "wind_speed": sensor.sensor_schema(
        unit_of_measurement="m/s",
        device_class="wind_speed",
        accuracy_decimals=1,
    ),
    "rain": sensor.sensor_schema(
        accuracy_decimals=0,
        icon="mdi:weather-rainy",
    ),
    "photo1_brightness": sensor.sensor_schema(
        unit_of_measurement="lx",
        device_class="illuminance",
        accuracy_decimals=0,
    ),
    "photo2_brightness": sensor.sensor_schema(
        unit_of_measurement="lx",
        device_class="illuminance",
        accuracy_decimals=0,
    ),
    "photo3_brightness": sensor.sensor_schema(
        unit_of_measurement="lx",
        device_class="illuminance",
        accuracy_decimals=0,
    ),
    "twilight_brightness": sensor.sensor_schema(
        unit_of_measurement="lx",
        device_class="illuminance",
        accuracy_decimals=1,
    ),
}

# Generate config schema for sensors
CONFIG_SCHEMA = WISOTRONIC_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): SENSOR_SCHEMAS[type] for type in TYPES}
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_WISOTRONIC_ID])

    for type in TYPES:
        if type in config:
            conf = config[type]
            var = await sensor.new_sensor(conf)
            cg.add(getattr(hub, f"set_{type}_sensor")(var))
