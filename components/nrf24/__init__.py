from esphome import (
    codegen as cg,
    config_validation as cv,
    automation,
    pins,
)
from esphome.const import CONF_ID, CONF_CHANNEL, CONF_PAYLOAD

CODEOWNERS = ["@carlos-jenkins"]

CONF_CE_PIN = "ce_pin"
CONF_CSN_PIN = "csn_pin"
CONF_IRQ_PIN = "irq_pin"
CONF_PA_LEVEL = "pa_level"
CONF_DATA_RATE = "data_rate"
CONF_TX_ADDRESS = "tx_address"
CONF_RX_ADDRESS = "rx_address"
CONF_RX_PIPE = "rx_pipe"
CONF_ON_RECEIVE = "on_receive"

# RF24 PA level values (see RF24.h rf24_pa_dbm_e)
PA_LEVELS = {
    "MIN": 0,
    "LOW": 1,
    "HIGH": 2,
    "MAX": 3,
}

# RF24 data rate values (see RF24.h rf24_datarate_e)
DATA_RATES = {
    "1MBPS": 0,
    "2MBPS": 1,
    "250KBPS": 2,
}

DEFAULT_ADDRESS = [0xDE, 0xAD, 0xC0, 0xDE, 0x01]

nrf24_ns = cg.esphome_ns.namespace("nrf24")
NRF24Component = nrf24_ns.class_("NRF24Component", cg.Component)
NRF24OnReceiveTrigger = nrf24_ns.class_(
    "NRF24OnReceiveTrigger",
    automation.Trigger.template(cg.std_string),
)
NRF24SendAction = nrf24_ns.class_("NRF24SendAction", automation.Action)


def _validate_address(value):
    value = cv.ensure_list(cv.hex_uint8_t)(value)
    if len(value) != 5:
        raise cv.Invalid(
            f"NRF24 addresses must be exactly 5 bytes, got {len(value)}"
        )
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NRF24Component),
        cv.Required(CONF_CE_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CSN_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_CHANNEL, default=76): cv.int_range(min=0, max=125),
        cv.Optional(CONF_PA_LEVEL, default="LOW"): cv.enum(PA_LEVELS, upper=True),
        cv.Optional(CONF_DATA_RATE, default="1MBPS"): cv.enum(
            DATA_RATES, upper=True
        ),
        cv.Optional(CONF_TX_ADDRESS, default=DEFAULT_ADDRESS): _validate_address,
        cv.Optional(CONF_RX_ADDRESS, default=DEFAULT_ADDRESS): _validate_address,
        cv.Optional(CONF_RX_PIPE, default=1): cv.int_range(min=0, max=5),
        cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
            {
                cv.GenerateID(): cv.declare_id(NRF24OnReceiveTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_library("nRF24/RF24", "1.4.11")

    component = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(component, config)

    ce_pin = await cg.gpio_pin_expression(config[CONF_CE_PIN])
    cg.add(component.set_ce_pin(ce_pin))

    csn_pin = await cg.gpio_pin_expression(config[CONF_CSN_PIN])
    cg.add(component.set_csn_pin(csn_pin))

    if CONF_IRQ_PIN in config:
        irq_pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(component.set_irq_pin(irq_pin))

    cg.add(component.set_channel(config[CONF_CHANNEL]))
    cg.add(component.set_pa_level(config[CONF_PA_LEVEL]))
    cg.add(component.set_data_rate(config[CONF_DATA_RATE]))
    cg.add(component.set_tx_address(config[CONF_TX_ADDRESS]))
    cg.add(component.set_rx_address(config[CONF_RX_ADDRESS]))
    cg.add(component.set_rx_pipe(config[CONF_RX_PIPE]))

    if CONF_ON_RECEIVE in config:
        cg.add(component.set_rx_enabled(True))
        for conf in config[CONF_ON_RECEIVE]:
            trigger = cg.new_Pvariable(conf[CONF_ID], component)
            await automation.build_automation(
                trigger,
                [(cg.std_string, "message")],
                conf,
            )


NRF24_SEND_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(NRF24Component),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "nrf24.send", NRF24SendAction, NRF24_SEND_ACTION_SCHEMA, synchronous=True
)
async def nrf24_send_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(template_))
    return var
