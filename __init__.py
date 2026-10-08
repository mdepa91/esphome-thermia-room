"""Thermia/Danfoss room sensor emulator: sends the room temperature and setpoint to the heat pump over its room
sensor terminals (303/304), so it regulates on real values. ESP32 only (uses the RMT peripheral). See README.md."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import esp32, number
from esphome.const import CONF_ID, CONF_PIN, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS

CODEOWNERS = []
AUTO_LOAD = ["number"]

thermia_room_ns = cg.esphome_ns.namespace("thermia_room")
ThermiaRoomComponent = thermia_room_ns.class_("ThermiaRoomComponent", cg.PollingComponent)
ThermiaRoomNumber = thermia_room_ns.class_("ThermiaRoomNumber", number.Number)
ThermiaRoomNumberKind = ThermiaRoomNumber.enum("Kind")

CONF_MARK_US = "mark_us"
CONF_SPACE_SHORT_US = "space_short_us"
CONF_SPACE_LONG_US = "space_long_us"
CONF_FRAME_GAP = "frame_gap"
CONF_POLARITY_INVERT = "polarity_invert"
CONF_DRY_RUN = "dry_run"
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_SETPOINT = "setpoint"

_TEMPERATURE_NUMBER = dict(device_class=DEVICE_CLASS_TEMPERATURE, unit_of_measurement=UNIT_CELSIUS)

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
            cv.Optional(CONF_SETPOINT): number.number_schema(
                ThermiaRoomNumber, icon="mdi:home-thermometer-outline", **_TEMPERATURE_NUMBER
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
        num = await number.new_number(config[CONF_SETPOINT], min_value=5.0, max_value=35.0, step=0.5)
        cg.add(num.set_parent(var))
        cg.add(num.set_kind(ThermiaRoomNumberKind.SETPOINT))
        cg.add(var.set_setpoint_number(num))
