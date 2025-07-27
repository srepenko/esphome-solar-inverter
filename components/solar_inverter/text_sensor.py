# text_sensor.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from .. import solar_inverter_ns

SolarInverter = solar_inverter_ns.class_('SolarInverter')

TextSensor = text_sensor.text_sensor_ns.class_('TextSensor', text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.text_sensor_schema()