import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_FAN,
    ICON_GAUGE,
)

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

FanModeSelect = broan_ns.class_("FanModeSelect", select.Select)
FanSpeedSelect = broan_ns.class_("FanSpeedSelect", select.Select)
DefrostModeSelect = broan_ns.class_("DefrostModeSelect", select.Select)

CONF_FAN_MODE = 'fan_mode'
CONF_FAN_SPEED = 'fan_speed'
CONF_DEFROST_MODE = 'defrost_mode'

# Keep in sync with FAN_MODE_* and g_rgFanSpeedNames in broan.h
FAN_MODES = [
    "Off",
    "Air Exchange",
    "Intermittent",
    "Intermittent + Recirculate",
    "Turbo",
    "Humidity",
    "Recirculate",
    "Smart",
    "Override",  # Reported by the ERV when an auxiliary remote is used. Can't be set.
]

FAN_SPEEDS = [
    "Minimum",
    "Medium",
    "High",
]

# Keep in sync with DEFROST_MODE_* in broan.h (installer menu, 12:50)
DEFROST_MODES = [
    "Discretion",  # 0x02, factory setting
    "Plus",  # 0x01, extended defrost for colder regions
]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),

    cv.Optional(CONF_FAN_MODE): select.select_schema(
        FanModeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_GAUGE,
    ),
    cv.Optional(CONF_FAN_SPEED): select.select_schema(
        FanSpeedSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_FAN,
    ),
    cv.Optional(CONF_DEFROST_MODE): select.select_schema(
        DefrostModeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:snowflake-melt",
    ),
}


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])
    if fan_mode_config := config.get(CONF_FAN_MODE):
        s = await select.new_select(
            fan_mode_config,
            options=FAN_MODES,
        )
        await cg.register_parented(s, config[CONF_BROAN_ID])
        cg.add(broan_component.set_fan_mode_select(s))

    if fan_speed_config := config.get(CONF_FAN_SPEED):
        s = await select.new_select(fan_speed_config, options=FAN_SPEEDS)
        await cg.register_parented(s, config[CONF_BROAN_ID])
        cg.add(broan_component.set_fan_speed_select(s))

    if defrost_mode_config := config.get(CONF_DEFROST_MODE):
        s = await select.new_select(defrost_mode_config, options=DEFROST_MODES)
        await cg.register_parented(s, config[CONF_BROAN_ID])
        cg.add(broan_component.set_defrost_mode_select(s))
