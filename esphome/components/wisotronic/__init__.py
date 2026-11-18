import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@torfbolt"]
AUTO_LOAD = ["sensor", "cover"]
MULTI_CONF = True

CONF_WISOTRONIC_ID = "wisotronic_id"

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
    cv.Schema({cv.GenerateID(): cv.declare_id(WisotronicCustomComponent)})
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    # Create the component instance
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
