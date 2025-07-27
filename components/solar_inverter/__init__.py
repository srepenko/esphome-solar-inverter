import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, binary_sensor, switch, select, number
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_MODE,
)


CONF_MODE = "mode"

NUMBER_MODES = {
    "auto": number.NumberMode.AUTO,
    "slider": number.NumberMode.SLIDER,
    "box": number.NumberMode.BOX,
}

DEPENDENCIES = ['uart']
AUTO_LOAD = ['sensor', 'text_sensor', 'binary_sensor', 'switch', 'select', 'number']

solar_inverter_ns = cg.esphome_ns.namespace('solar_inverter')
SolarInverter = solar_inverter_ns.class_('SolarInverter', cg.Component, uart.UARTDevice)
InverterSelect = solar_inverter_ns.class_("InverterSelect", select.Select)
InverterSwitch = solar_inverter_ns.class_("InverterSwitch", switch.Switch)
InverterNumber = solar_inverter_ns.class_("InverterNumber", number.Number)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SolarInverter),
    cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),

    # text_sensors
    cv.Optional('protocol_id'): text_sensor.text_sensor_schema(),
    cv.Optional('serial_number'): text_sensor.text_sensor_schema(),
    cv.Optional('eeprom_version_text'): text_sensor.text_sensor_schema(),
    cv.Optional('charging_mode_text'): text_sensor.text_sensor_schema(),

    # sensors
    cv.Optional('grid_voltage'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=1, device_class='voltage', state_class='measurement'),
    cv.Optional('grid_freq'): sensor.sensor_schema(
        unit_of_measurement='Hz', accuracy_decimals=2, device_class='frequency', state_class='measurement'),
    cv.Optional('ac_output_voltage'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=1, device_class='voltage', state_class='measurement'),
    cv.Optional('ac_output_freq'): sensor.sensor_schema(
        unit_of_measurement='Hz', accuracy_decimals=2, device_class='frequency', state_class='measurement'),
    cv.Optional('output_apparent_power'): sensor.sensor_schema(
        unit_of_measurement='VA', accuracy_decimals=0, device_class='power', state_class='measurement'),
    cv.Optional('output_active_power'): sensor.sensor_schema(
        unit_of_measurement='W', accuracy_decimals=0, device_class='power', state_class='measurement'),
    cv.Optional('output_load_percent'): sensor.sensor_schema(
        unit_of_measurement='%', accuracy_decimals=0, device_class='power', state_class='measurement'),
    cv.Optional('bus_voltage'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=1, device_class='voltage', state_class='measurement'),
    cv.Optional('battery_voltage'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=2, device_class='voltage', state_class='measurement'),
    cv.Optional('battery_charging_current'): sensor.sensor_schema(
        unit_of_measurement='A', accuracy_decimals=2, device_class='current', state_class='measurement'),
    cv.Optional('battery_capacity'): sensor.sensor_schema(
        unit_of_measurement='%', accuracy_decimals=0, device_class='battery', state_class='measurement'),
    cv.Optional('inverter_temp'): sensor.sensor_schema(
        unit_of_measurement='°C', accuracy_decimals=0, device_class='temperature', state_class='measurement'),
    cv.Optional('pv_input_current'): sensor.sensor_schema(
        unit_of_measurement='A', accuracy_decimals=2, device_class='current', state_class='measurement'),
    cv.Optional('pv_input_voltage'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=1, device_class='voltage', state_class='measurement'),
    cv.Optional('battery_voltage_from_scc'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=2, device_class='voltage', state_class='measurement'),
    cv.Optional('battery_discharge_current'): sensor.sensor_schema(
        unit_of_measurement='A', accuracy_decimals=2, device_class='current', state_class='measurement'),
    cv.Optional('pv_charging_power'): sensor.sensor_schema(
        unit_of_measurement='W', accuracy_decimals=0, device_class='power', state_class='measurement'),
    cv.Optional('fan_on_voltage_offset'): sensor.sensor_schema(
        unit_of_measurement='V', accuracy_decimals=2, device_class='voltage', state_class='measurement'),

    # binary sensors
    cv.Optional('pv_or_ac_powering_load'): binary_sensor.binary_sensor_schema(),
    cv.Optional('config_changed'): binary_sensor.binary_sensor_schema(),
    cv.Optional('scc_fw_updated'): binary_sensor.binary_sensor_schema(),
    cv.Optional('load_on'): binary_sensor.binary_sensor_schema(),
    cv.Optional('charging_on'): binary_sensor.binary_sensor_schema(),
    cv.Optional('scc_charging_on'): binary_sensor.binary_sensor_schema(),
    cv.Optional('ac_charging_on'): binary_sensor.binary_sensor_schema(),

    cv.Optional('charging_to_float'): binary_sensor.binary_sensor_schema(),
    cv.Optional('inverter_on'): binary_sensor.binary_sensor_schema(),
    cv.Optional('dustproof_installed'): binary_sensor.binary_sensor_schema(),

    # charging mode sensor
    cv.Optional('charging_mode_sensor'): sensor.sensor_schema(accuracy_decimals=0, icon='mdi:battery-charging'),

    # text sensors qmod
    cv.Optional("device_mode_sensor"): text_sensor.text_sensor_schema(icon="mdi:power-settings"),
    cv.Optional("device_mode_text"): text_sensor.text_sensor_schema(icon="mdi:power-settings"),

    # switches (qflag)
   cv.Optional("buzzer_control"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:volume-high"): cv.icon,
    }),
    cv.Optional("overload_bypass"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:flash-alert"): cv.icon,
    }),
    cv.Optional("display_escape_to_default_page"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:monitor"): cv.icon,
    }),
    cv.Optional("overload_restart"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:restart"): cv.icon,
    }),
    cv.Optional("over_temperature_restart"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:thermometer-alert"): cv.icon,
    }),
    cv.Optional("backlight_control"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:brightness-5"): cv.icon,
    }),
    cv.Optional("alarm_primary_source_interrupt"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:bell-alert"): cv.icon,
    }),
    cv.Optional("fault_code_record"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:file-document-alert"): cv.icon,
    }),
    cv.Optional("power_saving"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:power-plug-off"): cv.icon,  # Значок энергосбережения/выключения питания
    }),
    cv.Optional("data_log_popup"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:chart-box-outline"): cv.icon,  # Значок всплывающего окна журнала / графика
    }),
    cv.Optional("grid_charge_enable"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:server-network"): cv.icon,
    }),
    cv.Optional("solar_feed_to_grid"): switch.SWITCH_SCHEMA.extend({
        cv.Optional(CONF_ID): cv.declare_id(InverterSwitch),
        cv.Optional("icon", default="mdi:database-alert"): cv.icon,
    }),


    # energy sensors history
    cv.Optional('energy_solar_today'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:solar-power',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_solar_month'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:calendar-month',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_solar_year'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:calendar',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_solar_total'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:calendar',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_inverter_today'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:flash',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_inverter_month'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:calendar-month',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_inverter_year'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:calendar',
        state_class='total_increasing', device_class='energy'),
    cv.Optional('energy_inverter_total'): sensor.sensor_schema(
        unit_of_measurement='kWh', accuracy_decimals=2, icon='mdi:calendar',
        state_class='total_increasing', device_class='energy'),

    #QPIWS
    cv.Optional('warning_status_text'): text_sensor.text_sensor_schema(),

    cv.Optional("equalization_enable"): select.SELECT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(InverterSelect),}),
    cv.Optional("equalization_active"): select.SELECT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(InverterSelect),}),
    cv.Optional("equalization_voltage"):  number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("equalization_time"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("equalization_over_time"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("equalization_period"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("equalization_max_current"): sensor.sensor_schema(unit_of_measurement="A", accuracy_decimals=0),
    cv.Optional("equalization_elapsed_time"): sensor.sensor_schema(unit_of_measurement="min", accuracy_decimals=0),
    
    #QPIWS
    cv.Optional("battery_recharge_voltage"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("battery_redischarge_voltage"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("max_charging_current"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("max_ac_charging_current"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("ac_output_rating_frequency"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional("ac_output_rating_voltage"):   number.NUMBER_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(InverterNumber),
        cv.GenerateID("parent"): cv.use_id(SolarInverter),
        cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),

}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    uart_var = await cg.get_variable(config[CONF_UART_ID])
    cg.add(var.set_uart_parent(uart_var))

    # numeric sensors (energy history)
    numeric_sensors = {
        'energy_solar_today': 'set_energy_solar_today_sensor',
        'energy_solar_month': 'set_energy_solar_month_sensor',
        'energy_solar_year': 'set_energy_solar_year_sensor',
        'energy_solar_total': 'set_energy_solar_total_sensor',
        'energy_inverter_today': 'set_energy_inverter_today_sensor',
        'energy_inverter_month': 'set_energy_inverter_month_sensor',
        'energy_inverter_year': 'set_energy_inverter_year_sensor',
        'energy_inverter_total': 'set_energy_inverter_total_sensor',
    }
    for key, setter in numeric_sensors.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))

    # text sensors
    text_sensors = {
        'device_mode_sensor': 'set_device_mode_sensor',
        'device_mode_text': 'set_device_mode_text',
        'protocol_id': 'set_protocol_id_sensor',
        'serial_number': 'set_serial_number_sensor',
        'eeprom_version_text': 'set_eeprom_version_text_sensor',
        'charging_mode_text': 'set_charging_mode_text_sensor',
        'warning_status_text': 'set_warning_status_text_sensor',
    }
    for key, setter in text_sensors.items():
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, setter)(sens))

    # normal sensors
    normal_sensors = [
        'grid_voltage', 'grid_freq', 'ac_output_voltage', 'ac_output_freq',
        'output_apparent_power', 'output_active_power', 'output_load_percent',
        'bus_voltage', 'battery_voltage', 'battery_charging_current',
        'battery_capacity', 'inverter_temp', 'pv_input_current', 'pv_input_voltage',
        'battery_voltage_from_scc', 'battery_discharge_current', 'pv_charging_power',
        'fan_on_voltage_offset', 'charging_mode_sensor',
    ]
    for key in normal_sensors:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f'set_{key}_sensor')(sens))

    # binary sensors
    binary_sensors = [
        'pv_or_ac_powering_load', 'config_changed', 'scc_fw_updated', 'load_on',
        'charging_on', 'scc_charging_on', 'ac_charging_on', 'charging_to_float',
        'inverter_on', 'dustproof_installed',
    ]
    for key in binary_sensors:
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, f'set_{key}')(sens))


    # switches (auto-assign id if missing)
    switches = {
        "buzzer_control": "set_buzzer_control",
        "overload_bypass": "set_overload_bypass",
        "display_escape_to_default_page": "set_display_escape_to_default_page",
        "overload_restart": "set_overload_restart",
        "over_temperature_restart": "set_over_temperature_restart",
        "backlight_control": "set_backlight_control",
        "alarm_primary_source_interrupt": "set_alarm_primary_source_interrupt",
        "fault_code_record": "set_fault_code_record",
        "power_saving": "set_power_saving",
        "data_log_popup": "set_data_log_popup",
        "grid_charge_enable": "set_grid_charge_enable",
        "solar_feed_to_grid": "set_solar_feed_to_grid",
    }

    for key, setter in switches.items():
        if key in config:
            val = config[key]
            if isinstance(val, dict):
                if 'id' not in val:
                    val['id'] = cv.declare_id(InverterSwitch)(key)

            else:
                val = {'id': cv.declare_id(InverterSwitch)(key)}
            sw = await switch.new_switch(val)
            cg.add(getattr(var, setter)(sw))

    # select
    select_fields_options = {
        'equalization_enable': {
            'options': ["Disabled", "Enabled"],
            'parameters': ["0", "1"],
            'command_prefix': "PBEQE",
            'command_status': "QBEQI",
            'request_index': 0,
        },
        'equalization_active': {
            'options': ["Inactive", "Active"],
            'parameters': ["0", "1"],
            'command_prefix': "PBEQA",
            'command_status': "QBEQI",
            'request_index': 9,
        },
    }
    for field, opt_data in select_fields_options.items():
        if field in config:
            conf = config[field]
            sel = cg.new_Pvariable(conf[CONF_ID])
            #await cg.register_component(sel, conf)
            await select.register_select(sel, conf, options=opt_data['options'])
            cg.add(sel.set_options_list(opt_data['options']))
            cg.add(sel.set_command_prefix(opt_data['command_prefix']))
            cg.add(sel.set_parameters(opt_data['parameters'])) 
            cg.add(var.add_inverter_select(opt_data['request_index'], sel))
            # Автоматически вызвать set_<field>()
            setter_name = f"set_{field}"
            if hasattr(var, setter_name):
                cg.add(getattr(var, setter_name)(sel))

    # sensors
    sensors_qbeqi = {
        "equalization_elapsed_time": "set_equalization_time",
        "equalization_max_current": "set_equalization_max_current",
    }
    for key, setter in sensors_qbeqi.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))

    # Number fields
    number_fields = {
        'equalization_voltage': {       'fmt': "%2.2f", 'cmd': "PBEQV", 'min': 48.0, 'max': 61.0, 'step': 0.1, 'unit': "V"},
        'equalization_time': {          'fmt': "%3d", 'cmd': "PBEQT", 'min': 5, 'max': 900, 'step': 5, 'unit': "min"},
        'equalization_over_time': {     'fmt': "%3d", 'cmd': "PBEQOT", 'min': 5, 'max': 900, 'step': 5, 'unit': "min"},
        'equalization_period': {        'fmt': "%3d", 'cmd': "PBEQP", 'min': 0, 'max': 90, 'step': 1, 'unit': "d"},
        # QPIRI
        'battery_recharge_voltage': {   'fmt': "%2.1f", 'cmd': "PBCV", 'min': 42, 'max': 51, 'step': 1, 'unit': "V"},
        'battery_redischarge_voltage': {'fmt': "%2.1f", 'cmd': "PBDV", 'min': 48, 'max': 58, 'step': 1, 'unit': "V"},
        'max_charging_current': {       'fmt': "%3d", 'cmd': "MNCHGC", 'min': 10, 'max': 120, 'step': 10, 'unit': "A"},
        'max_ac_charging_current': {    'fmt': "%3d", 'cmd': "MUCHGC", 'min': 2, 'max': 100, 'step': 10, 'unit': "A"},
        'ac_output_rating_frequency': { 'fmt': "%2d", 'cmd': "F", 'min': 50, 'max': 60, 'step': 10, 'unit': "Hz"},
        'ac_output_rating_voltage': {   'fmt': "%3d", 'cmd': "V", 'min': 220, 'max': 240, 'step': 10, 'unit': "V"},
    }


    for field, props in number_fields.items():
        if field in config:
            nconf = config[field]
            num = cg.new_Pvariable(nconf[CONF_ID])

            # Регистрируем number-сущность с параметрами min/max/step, если они заданы
            await number.register_number(
                num,
                nconf,
                min_value=props['min'] if props['min'] is not None else nconf.get(CONF_MIN_VALUE),
                max_value=props['max'] if props['max'] is not None else nconf.get(CONF_MAX_VALUE),
                step=props['step'] if props['step'] is not None else nconf.get(CONF_STEP),
            )

            par = await cg.get_variable(config[CONF_ID])
            cg.add(num.set_parent(par))
            cg.add(num.set_command_prefix(props['cmd']))
            cg.add(num.set_format(props['fmt']))
            if props['unit']:
                num.traits.set_unit_of_measurement(props['unit'])
            num.traits.set_mode(nconf[CONF_MODE])

            setter_name = f"set_{field}"
            if hasattr(var, setter_name):
                cg.add(getattr(var, setter_name)(num))


