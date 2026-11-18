import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_AWNING, DEVICE_CLASS_WINDOW, DEVICE_CLASS_BLIND

from .. import CONF_WISOTRONIC_ID, WISOTRONIC_COMPONENT_SCHEMA, wisotronic_ns

DEPENDENCIES = ["uart"]

# Cover types
CONF_AWNING = "awning_cover"
CONF_WINDOW = "window_cover"
CONF_BLIND1 = "blind1_cover"
CONF_BLIND2 = "blind2_cover"

TYPES = {
    CONF_AWNING: None,
    CONF_WINDOW: None,
    CONF_BLIND1: None,
    CONF_BLIND2: None,
}

WisotronicCover = wisotronic_ns.class_("WisotronicCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = WISOTRONIC_COMPONENT_SCHEMA.extend({
    cv.Optional(CONF_AWNING): cover.cover_schema(WisotronicCover,
        device_class=DEVICE_CLASS_AWNING).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_WINDOW): cover.cover_schema(WisotronicCover,
        device_class=DEVICE_CLASS_WINDOW).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_BLIND1): cover.cover_schema(WisotronicCover,
        device_class=DEVICE_CLASS_BLIND).extend(cv.COMPONENT_SCHEMA),
    cv.Optional(CONF_BLIND2): cover.cover_schema(WisotronicCover,
        device_class=DEVICE_CLASS_BLIND).extend(cv.COMPONENT_SCHEMA),
})

async def to_code(config):
    paren = await cg.get_variable(config[CONF_WISOTRONIC_ID])

    for type in TYPES:
        if type in config:
            conf = config[type]
            var = await cover.new_cover(conf)
            await cg.register_component(var, conf)
            cg.add(getattr(paren, f"set_{type}")(var))
            cg.add(var.set_parent(paren))
