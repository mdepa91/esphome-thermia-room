"""Thermia/Danfoss room sensor emulator: sends the room temperature and setpoint to the heat pump over its room
sensor terminals (303/304), so it regulates on real values. ESP32 only (uses the RMT peripheral). See README.md."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import esp32, number, select, switch
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NAME,
    CONF_PIN,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
)

CODEOWNERS = []
AUTO_LOAD = ["number", "select", "switch"]

thermia_room_ns = cg.esphome_ns.namespace("thermia_room")
ThermiaRoomComponent = thermia_room_ns.class_("ThermiaRoomComponent", cg.PollingComponent)
ThermiaRoomNumber = thermia_room_ns.class_("ThermiaRoomNumber", number.Number)
ThermiaRoomNumberKind = ThermiaRoomNumber.enum("Kind")
ThermiaRoomSwitch = thermia_room_ns.class_("ThermiaRoomSwitch", switch.Switch)
CardMapSelect = thermia_room_ns.class_("CardMapSelect", select.Select, cg.Component)

CONF_MARK_US = "mark_us"
CONF_SPACE_SHORT_US = "space_short_us"
CONF_SPACE_LONG_US = "space_long_us"
CONF_FRAME_GAP = "frame_gap"
CONF_POLARITY_INVERT = "polarity_invert"
CONF_DRY_RUN = "dry_run"
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_SETPOINT = "setpoint"
# Diagnostic switch that turns the frames on and off (off = no room sensor: alarm, the pump uses r32). In normal
# operation the room sensor is switched off with room factor 0 (esphome-thermia), which raises no alarm.
CONF_TRANSMIT = "transmit"
# Diagnostic select listing "<config key>=<entity name>" for thermia-heatpump-card's device auto-detection
# (see card_map_select.h). On by default; `card_map: {internal: true}` hides it from Home Assistant.
CONF_CARD_MAP = "card_map"
CARD_MAP_MARKER = "thermia-card-map:1"

_TEMPERATURE_NUMBER = dict(device_class=DEVICE_CLASS_TEMPERATURE, unit_of_measurement=UNIT_CELSIUS)

# The pump raises the room sensor alarm for setpoints below 16 °C (checked on a Thermia), so that is the default
# minimum; 31.5 °C is the default maximum. The frame itself can carry 5-35 °C; widen the range in YAML if your pump
# accepts more.
SETPOINT_FRAME_MIN, SETPOINT_FRAME_MAX = 5.0, 35.0
SETPOINT_DEFAULT_MIN, SETPOINT_DEFAULT_MAX = 16.0, 31.5


def _validate_setpoint_range(config):
    if config[CONF_MIN_VALUE] > config[CONF_MAX_VALUE]:
        raise cv.Invalid("min_value must not be greater than max_value")
    return config


_SETPOINT_SCHEMA = cv.All(
    number.number_schema(ThermiaRoomNumber, icon="mdi:home-thermometer-outline", **_TEMPERATURE_NUMBER).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=SETPOINT_DEFAULT_MIN): cv.float_range(
                min=SETPOINT_FRAME_MIN, max=SETPOINT_FRAME_MAX
            ),
            cv.Optional(CONF_MAX_VALUE, default=SETPOINT_DEFAULT_MAX): cv.float_range(
                min=SETPOINT_FRAME_MIN, max=SETPOINT_FRAME_MAX
            ),
        }
    ),
    _validate_setpoint_range,
)

# Timing defaults are the values running on a Thermia pump (README.md, "Timing").
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ThermiaRoomComponent),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_MARK_US, default=60000): cv.positive_int,
            cv.Optional(CONF_SPACE_SHORT_US, default=82000): cv.positive_int,
            cv.Optional(CONF_SPACE_LONG_US, default=388000): cv.positive_int,
            # The pump finds the start of a frame by the idle gap before it; 2-4 s works, 0 does not.
            cv.Optional(CONF_FRAME_GAP, default="4s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_POLARITY_INVERT, default=False): cv.boolean,
            cv.Optional(CONF_DRY_RUN, default=False): cv.boolean,
            cv.Optional(CONF_ROOM_TEMPERATURE): number.number_schema(
                ThermiaRoomNumber, icon="mdi:home-thermometer", **_TEMPERATURE_NUMBER
            ),
            cv.Optional(CONF_SETPOINT): _SETPOINT_SCHEMA,
            cv.Optional(CONF_TRANSMIT): switch.switch_schema(
                ThermiaRoomSwitch,
                icon="mdi:access-point",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                default_restore_mode="RESTORE_DEFAULT_ON",
            ),
            cv.Optional(CONF_CARD_MAP, default={CONF_NAME: "Thermia room card map"}): select.select_schema(
                CardMapSelect, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:map-legend"
            ),
        }
    ).extend(cv.polling_component_schema("10s")),  # how often the entities are republished
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    esp32.include_builtin_idf_component("esp_driver_rmt")  # ESPHome strips unused IDF drivers otherwise

    cg.add(var.set_pin(await cg.gpio_pin_expression(config[CONF_PIN])))
    cg.add(var.set_mark_us(config[CONF_MARK_US]))
    cg.add(var.set_space_short_us(config[CONF_SPACE_SHORT_US]))
    cg.add(var.set_space_long_us(config[CONF_SPACE_LONG_US]))
    cg.add(var.set_frame_gap_ms(config[CONF_FRAME_GAP]))
    cg.add(var.set_polarity_invert(config[CONF_POLARITY_INVERT]))
    cg.add(var.set_dry_run(config[CONF_DRY_RUN]))

    if CONF_ROOM_TEMPERATURE in config:
        num = await number.new_number(config[CONF_ROOM_TEMPERATURE], min_value=0.0, max_value=31.9, step=0.1)
        cg.add(num.set_parent(var))
        cg.add(num.set_kind(ThermiaRoomNumberKind.ROOM_TEMPERATURE))
        cg.add(var.set_room_temperature_number(num))

    if CONF_SETPOINT in config:
        conf = config[CONF_SETPOINT]
        num = await number.new_number(conf, min_value=conf[CONF_MIN_VALUE], max_value=conf[CONF_MAX_VALUE], step=0.5)
        cg.add(var.set_setpoint_range(conf[CONF_MIN_VALUE], conf[CONF_MAX_VALUE]))
        cg.add(num.set_parent(var))
        cg.add(num.set_kind(ThermiaRoomNumberKind.SETPOINT))
        cg.add(var.set_setpoint_number(num))

    if CONF_TRANSMIT in config:
        sw = await switch.new_switch(config[CONF_TRANSMIT])
        cg.add(sw.set_parent(var))
        cg.add(var.set_transmit_switch(sw))

    map_options = [CARD_MAP_MARKER] + [
        f"{key}={config[key][CONF_NAME]}"
        for key in (CONF_ROOM_TEMPERATURE, CONF_SETPOINT, CONF_TRANSMIT)
        if key in config and config[key].get(CONF_NAME)
    ]
    card_map = await select.new_select(config[CONF_CARD_MAP], options=map_options)
    await cg.register_component(card_map, config[CONF_CARD_MAP])
