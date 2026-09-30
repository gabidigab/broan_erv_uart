import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    DEVICE_CLASS_HUMIDITY,
    ENTITY_CATEGORY_CONFIG,
    ICON_FAN,
    ICON_WATER,
    ICON_TIMER,
    UNIT_MINUTE,
    UNIT_PERCENT,
)

UNIT_CFM = "CFM"

from .. import CONF_BROAN_ID, BroanComponent, broan_ns

HumiditySetpointNumber = broan_ns.class_("HumiditySetpointNumber", number.Number)
IntermittentPeriodNumber = broan_ns.class_("IntermittentPeriodNumber", number.Number)
FlowSetpointNumber = broan_ns.class_("FlowSetpointNumber", number.Number)

CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
CONF_INT_PERIOD = "intermittent_period"

# Installer flow setpoints: (config key, BroanFanSpeed, BroanFlowSide)
FLOW_SETPOINTS = [
    ("minimum_supply_flow", 0, 0),
    ("minimum_exhaust_flow", 0, 1),
    ("medium_supply_flow", 1, 0),
    ("medium_exhaust_flow", 1, 1),
    ("high_supply_flow", 2, 0),
    ("high_exhaust_flow", 2, 1),
]

# Default range: VanEE V180H75RT datasheet, 65 to 193 CFM
FLOW_SETPOINT_SCHEMA = number.number_schema(
    FlowSetpointNumber,
    entity_category=ENTITY_CATEGORY_CONFIG,
    unit_of_measurement=UNIT_CFM,
    icon=ICON_FAN,
).extend(
    {
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
        cv.Optional(CONF_MIN_VALUE, default=65): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=193): cv.float_,
    }
)


def validate_flow_range(config):
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid("min_value must be lower than max_value")
    return config

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
        cv.Optional(CONF_HUMIDITY_SETPOINT): number.number_schema(
            HumiditySetpointNumber,
            device_class=DEVICE_CLASS_HUMIDITY,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_WATER,
        ),
        cv.Optional(CONF_INT_PERIOD): number.number_schema(
            IntermittentPeriodNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_MINUTE,
            icon=ICON_TIMER,
        ),
        **{
            cv.Optional(key): cv.All(FLOW_SETPOINT_SCHEMA, validate_flow_range)
            for key, _, _ in FLOW_SETPOINTS
        },
    }
)


async def to_code(config):
    broan_component = await cg.get_variable(config[CONF_BROAN_ID])

    if humidity_setpoint_config := config.get(CONF_HUMIDITY_SETPOINT):
        h = await number.new_number(
            humidity_setpoint_config, min_value=30, max_value=55, step=5
        )
        await cg.register_parented(h, config[CONF_BROAN_ID])
        cg.add(broan_component.set_humidity_setpoint_number(h))

    # Minutes ON per hour. The ERV stores seconds (02:22).
    if intermittent_period_config := config.get(CONF_INT_PERIOD):
        h = await number.new_number(
            intermittent_period_config, min_value=10, max_value=55, step=5
        )
        await cg.register_parented(h, config[CONF_BROAN_ID])
        cg.add(broan_component.set_intermittent_period_number(h))

    for key, speed, side in FLOW_SETPOINTS:
        if flow_config := config.get(key):
            n = await number.new_number(
                flow_config,
                min_value=flow_config[CONF_MIN_VALUE],
                max_value=flow_config[CONF_MAX_VALUE],
                step=1,
            )
            await cg.register_parented(n, config[CONF_BROAN_ID])
            cg.add(n.set_flow(speed, side))
            cg.add(broan_component.set_flow_setpoint_number(speed, side, n))
