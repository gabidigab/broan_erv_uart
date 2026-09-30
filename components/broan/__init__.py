from __future__ import annotations

from typing import Literal

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_FLOW_CONTROL_PIN
from esphome.cpp_helpers import gpio_pin_expression

AUTO_LOAD = []
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@nspitko"]

broan_ns = cg.esphome_ns.namespace("broan")
BroanComponent = broan_ns.class_("BroanComponent", cg.Component, uart.UARTDevice)

CONF_BROAN_ID = "broan_id"
CONF_LISTEN_ONLY = "listen_only"
CONF_AIR_EXCHANGE_MEDIUM_CFM = "air_exchange_medium_cfm"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BroanComponent),
            cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
            # Passive sniffer: never transmit, log what the wall controller writes.
            cv.Optional(CONF_LISTEN_ONLY, default=False): cv.boolean,
            # Flow (CFM) forced for Air Exchange Medium, supply and exhaust (06:22 / 08:22).
            cv.Optional(CONF_AIR_EXCHANGE_MEDIUM_CFM): cv.float_range(min=1, max=1000),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

BroanBaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_BROAN_ID): cv.use_id(BroanComponent),
    },
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "broan",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)

async def to_code(config):
    cg.add_global(broan_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    if config[CONF_LISTEN_ONLY]:
        cg.add_build_flag("-DLISTEN_ONLY")

    if CONF_AIR_EXCHANGE_MEDIUM_CFM in config:
        cg.add(var.set_air_exchange_medium_cfm(config[CONF_AIR_EXCHANGE_MEDIUM_CFM]))

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))
