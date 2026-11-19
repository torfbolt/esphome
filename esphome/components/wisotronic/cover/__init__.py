import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_AWNING, DEVICE_CLASS_BLIND, DEVICE_CLASS_WINDOW

from .. import CONF_WISOTRONIC_ID, WISOTRONIC_COMPONENT_SCHEMA, wisotronic_ns

DEPENDENCIES = ["uart"]

# Cover types
TYPES = {
    "awning_cover": None,
    "window_cover": None,
    "blind1_cover": None,
    "blind2_cover": None,
}

WisotronicCover = wisotronic_ns.class_("WisotronicCover", cover.Cover, cg.Component)

CONFIG_SCHEMA = WISOTRONIC_COMPONENT_SCHEMA.extend(
    {
        cv.Optional("awning_cover"): cover.cover_schema(
            WisotronicCover, device_class=DEVICE_CLASS_AWNING
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional("window_cover"): cover.cover_schema(
            WisotronicCover, device_class=DEVICE_CLASS_WINDOW
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional("blind1_cover"): cover.cover_schema(
            WisotronicCover, device_class=DEVICE_CLASS_BLIND
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional("blind2_cover"): cover.cover_schema(
            WisotronicCover, device_class=DEVICE_CLASS_BLIND
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_WISOTRONIC_ID])

    for type in TYPES:
        if type in config:
            conf = config[type]
            var = await cover.new_cover(conf)
            await cg.register_component(var, conf)
            cg.add(getattr(paren, f"set_{type}")(var))
            cg.add(var.set_parent(paren))
