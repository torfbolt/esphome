import esphome.codegen as cg
from esphome.components import text_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC

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
            cv.Optional(CONF_DEVICE_TIME): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    # Create the component instance
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_DEVICE_TIME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_TIME])
        cg.add(var.set_device_time(sens))
