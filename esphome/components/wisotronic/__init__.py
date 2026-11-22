import esphome.codegen as cg
from esphome.components import cover, sensor, text_sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_OUTDOOR_TEMPERATURE as CONF_OUTDOOR_TEMPERATURE,
    CONF_WIND_SPEED as CONF_WIND_SPEED,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DOMAIN = "wisotronic"
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@torfbolt"]
AUTO_LOAD = ["sensor", "cover", "text_sensor"]
MULTI_CONF = True

CONF_WISOTRONIC_ID = "wisotronic_id"
CONF_DEVICE_TIME = "device_time"
CONF_AWNING = "awning"
CONF_BLIND1 = "blind1"
CONF_BLIND2 = "blind2"
CONF_CONTROL_UNIT_TEMPERATURE = "control_unit_temperature"
CONF_INDOOR_TEMPERATURE = "indoor_temperature"
CONF_INDOOR_HUMIDITY = "indoor_humidity"
CONF_RAIN = "rain"
CONF_PHOTO1_BRIGHTNESS = "photo1_brightness"
CONF_PHOTO2_BRIGHTNESS = "photo2_brightness"
CONF_PHOTO3_BRIGHTNESS = "photo3_brightness"
CONF_TWILIGHT_BRIGHTNESS = "twilight_brightness"

DOMAIN = "wisotronic"
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@torfbolt"]
AUTO_LOAD = ["sensor", "cover", "text_sensor"]
MULTI_CONF = True

CONF_WISOTRONIC_ID = "wisotronic_id"
CONF_DEVICE_TIME = "device_time"

# Create the esphome namespace and component class
wisotronic_ns = cg.esphome_ns.namespace("wisotronic")
WisotronicCustomComponent = wisotronic_ns.class_(
    "WisotronicCustomComponent", cg.Component, uart.UARTDevice
)
WisotronicCover = wisotronic_ns.class_("WisotronicCover", cover.Cover, cg.Component)

# Base schema for the component
WISOTRONIC_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_WISOTRONIC_ID): cv.use_id(WisotronicCustomComponent),
    }
)

# Complete YAML schema for the component
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WisotronicCustomComponent),
            cv.Optional("device_time"): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional("awning"): cover.cover_schema(WisotronicCover),
            cv.Optional("window"): cover.cover_schema(WisotronicCover),
            cv.Optional("blind1"): cover.cover_schema(WisotronicCover),
            cv.Optional("blind2"): cover.cover_schema(WisotronicCover),
            cv.Optional("control_unit_temperature"): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional("outdoor_temperature"): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional("indoor_temperature"): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=1,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional("indoor_humidity"): sensor.sensor_schema(
                unit_of_measurement="%",
                accuracy_decimals=1,
                device_class="humidity",
                state_class="measurement",
            ),
            cv.Optional("wind_speed"): sensor.sensor_schema(
                unit_of_measurement="m/s",
                accuracy_decimals=1,
                device_class="wind_speed",
                state_class="measurement",
            ),
            cv.Optional("rain"): sensor.sensor_schema(
                accuracy_decimals=0,
            ),
            cv.Optional("photo1_brightness"): sensor.sensor_schema(
                unit_of_measurement="lx",
                accuracy_decimals=0,
                device_class="illuminance",
                state_class="measurement",
            ),
            cv.Optional("photo2_brightness"): sensor.sensor_schema(
                unit_of_measurement="lx",
                accuracy_decimals=0,
                device_class="illuminance",
                state_class="measurement",
            ),
            cv.Optional("photo3_brightness"): sensor.sensor_schema(
                unit_of_measurement="lx",
                accuracy_decimals=0,
                device_class="illuminance",
                state_class="measurement",
            ),
            cv.Optional("twilight_brightness"): sensor.sensor_schema(
                unit_of_measurement="lx",
                accuracy_decimals=1,
                device_class="illuminance",
                state_class="measurement",
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if "device_time" in config:
        sens = await text_sensor.new_text_sensor(config["device_time"])
        cg.add(var.set_device_time(sens))

    if "awning" in config:
        awning = await cover.new_cover(config["awning"])
        cg.add(var.set_awning_cover(awning))
        cg.add(awning.set_parent(var))

    if "window" in config:
        window = await cover.new_cover(config["window"])
        cg.add(var.set_window_cover(window))
        cg.add(window.set_parent(var))

    if "blind1" in config:
        blind1 = await cover.new_cover(config["blind1"])
        cg.add(var.set_blind1_cover(blind1))
        cg.add(blind1.set_parent(var))

    if "blind2" in config:
        blind2 = await cover.new_cover(config["blind2"])
        cg.add(var.set_blind2_cover(blind2))
        cg.add(blind2.set_parent(var))

    # Wire up sub-sensors
    sensors = sensor.sub_sensors(config)
    await sensors("control_unit_temperature", var.set_control_unit_temperature_sensor)
    await sensors("outdoor_temperature", var.set_outdoor_temperature_sensor)
    await sensors("indoor_temperature", var.set_indoor_temperature_sensor)
    await sensors("indoor_humidity", var.set_indoor_humidity_sensor)
    await sensors("wind_speed", var.set_wind_speed_sensor)
    await sensors("rain", var.set_rain_sensor)
    await sensors("photo1_brightness", var.set_photo1_brightness_sensor)
    await sensors("photo2_brightness", var.set_photo2_brightness_sensor)
    await sensors("photo3_brightness", var.set_photo3_brightness_sensor)
    await sensors("twilight_brightness", var.set_twilight_brightness_sensor)
