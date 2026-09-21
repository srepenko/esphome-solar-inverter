import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, binary_sensor, switch, select, number, button
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_MODE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
)


CONF_MODE = "mode"

NUMBER_MODES = {
    "auto": number.NumberMode.AUTO,
    "slider": number.NumberMode.SLIDER,
    "box": number.NumberMode.BOX,
}

DEPENDENCIES = ['uart']
AUTO_LOAD = ['sensor', 'text_sensor', 'binary_sensor', 'switch', 'select', 'number', 'button']

solar_inverter_ns = cg.esphome_ns.namespace('solar_inverter')
SolarInverter = solar_inverter_ns.class_('SolarInverter', cg.Component, uart.UARTDevice)
InverterSelect = solar_inverter_ns.class_("InverterSelect", select.Select)
InverterSwitch = solar_inverter_ns.class_("InverterSwitch", switch.Switch)
InverterNumber = solar_inverter_ns.class_("InverterNumber", number.Number)
InverterInquiryButton = solar_inverter_ns.class_("InverterInquiryButton", button.Button)

INVERTER_SWITCH_SCHEMA = lambda icon: switch.switch_schema(
    InverterSwitch, icon=icon, entity_category=ENTITY_CATEGORY_CONFIG
)
# NumberTraits has no set_unit_of_measurement in ESPHome 2026.9; pass unit here.
INVERTER_NUMBER_SCHEMA = lambda unit: number.number_schema(
    InverterNumber, entity_category=ENTITY_CATEGORY_CONFIG, unit_of_measurement=unit
).extend({
    cv.GenerateID("parent"): cv.use_id(SolarInverter),
    cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
})
INVERTER_DEBUG_BUTTON_SCHEMA = button.button_schema(
    InverterInquiryButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon="mdi:serial-port"
)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SolarInverter),
    cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),

    # text_sensors
    cv.Optional('protocol_id'): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional('serial_number'): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional('eeprom_version_text'): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
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
    cv.Optional("buzzer_control"): INVERTER_SWITCH_SCHEMA("mdi:volume-high"),
    cv.Optional("overload_bypass"): INVERTER_SWITCH_SCHEMA("mdi:flash-alert"),
    cv.Optional("display_escape_to_default_page"): INVERTER_SWITCH_SCHEMA("mdi:monitor"),
    cv.Optional("overload_restart"): INVERTER_SWITCH_SCHEMA("mdi:restart"),
    cv.Optional("over_temperature_restart"): INVERTER_SWITCH_SCHEMA("mdi:thermometer-alert"),
    cv.Optional("backlight_control"): INVERTER_SWITCH_SCHEMA("mdi:brightness-5"),
    cv.Optional("alarm_primary_source_interrupt"): INVERTER_SWITCH_SCHEMA("mdi:bell-alert"),
    cv.Optional("fault_code_record"): INVERTER_SWITCH_SCHEMA("mdi:file-document-alert"),
    cv.Optional("power_saving"): INVERTER_SWITCH_SCHEMA("mdi:power-plug-off"),  # Значок энергосбережения/выключения питания
    cv.Optional("data_log_popup"): INVERTER_SWITCH_SCHEMA("mdi:chart-box-outline"),  # Значок всплывающего окна журнала / графика
    cv.Optional("grid_charge_enable"): INVERTER_SWITCH_SCHEMA("mdi:server-network"),
    cv.Optional("solar_feed_to_grid"): INVERTER_SWITCH_SCHEMA("mdi:database-alert"),


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

    cv.Optional("equalization_enable"): select.select_schema(
        InverterSelect, entity_category=ENTITY_CATEGORY_CONFIG),
    cv.Optional("equalization_active"): select.select_schema(
        InverterSelect, entity_category=ENTITY_CATEGORY_CONFIG),
    # Program 01: QPIRI field 16. SET only POP00/01/02 (MAX). QPIRI 3 = LCD UtS, no POP03.
    cv.Optional("output_source_priority"): select.select_schema(
        InverterSelect, icon="mdi:transmission-tower", entity_category=ENTITY_CATEGORY_CONFIG
    ),
    cv.Optional("output_source_priority_text"): text_sensor.text_sensor_schema(
        icon="mdi:text-box-outline", entity_category=ENTITY_CATEGORY_CONFIG),
    cv.Optional("output_source_priority_code"): text_sensor.text_sensor_schema(
        icon="mdi:numeric", entity_category=ENTITY_CATEGORY_DIAGNOSTIC),

    cv.Optional("debug_last_command"): text_sensor.text_sensor_schema(
        icon="mdi:console", entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional("debug_last_response"): text_sensor.text_sensor_schema(
        icon="mdi:console", entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional("debug_last_result"): text_sensor.text_sensor_schema(
        icon="mdi:check-decagram", entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    cv.Optional("debug_query_qpi"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qid"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qvfw"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qvfw2"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qmn"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qgmn"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qmod"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qflag"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qpiri"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qpigs"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qpiws"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qbeqi"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qdi"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qoppt"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qmchgcr"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_query_qmuchgcr"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("debug_dump_inquiries"): INVERTER_DEBUG_BUTTON_SCHEMA,
    cv.Optional("equalization_voltage"): INVERTER_NUMBER_SCHEMA("V"),
    cv.Optional("equalization_time"): INVERTER_NUMBER_SCHEMA("min"),
    cv.Optional("equalization_over_time"): INVERTER_NUMBER_SCHEMA("min"),
    cv.Optional("equalization_period"): INVERTER_NUMBER_SCHEMA("d"),
    cv.Optional("equalization_max_current"): sensor.sensor_schema(unit_of_measurement="A", accuracy_decimals=0),
    cv.Optional("equalization_elapsed_time"): sensor.sensor_schema(unit_of_measurement="min", accuracy_decimals=0),
    
    #QPIWS
    cv.Optional("battery_recharge_voltage"): INVERTER_NUMBER_SCHEMA("V"),
    cv.Optional("battery_redischarge_voltage"): INVERTER_NUMBER_SCHEMA("V"),
    cv.Optional("max_charging_current"): INVERTER_NUMBER_SCHEMA("A"),
    cv.Optional("max_ac_charging_current"): INVERTER_NUMBER_SCHEMA("A"),
    cv.Optional("ac_output_rating_frequency"): INVERTER_NUMBER_SCHEMA("Hz"),
    cv.Optional("ac_output_rating_voltage"): INVERTER_NUMBER_SCHEMA("V"),

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
        'eeprom_version_text': 'set_eeprom_version_text',
        'charging_mode_text': 'set_charging_mode_text_sensor',
        'warning_status_text': 'set_warning_status_text_sensor',
        'output_source_priority_text': 'set_output_source_priority_text',
        'output_source_priority_code': 'set_output_source_priority_code',
        'debug_last_command': 'set_debug_last_command',
        'debug_last_response': 'set_debug_last_response',
        'debug_last_result': 'set_debug_last_result',
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
        'fan_on_voltage_offset',
    ]
    for key in normal_sensors:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f'set_{key}_sensor')(sens))

    # Setter is set_charging_mode_sensor, not set_charging_mode_sensor_sensor.
    if 'charging_mode_sensor' in config:
        sens = await sensor.new_sensor(config['charging_mode_sensor'])
        cg.add(var.set_charging_mode_sensor(sens))

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
            'set_commands': ["PBEQE0", "PBEQE1"],
            'command_prefix': "PBEQE",
            'command_status': "QBEQI",
            'request_index': 0,
        },
        'equalization_active': {
            'options': ["Inactive", "Active"],
            'parameters': ["0", "1"],
            'set_commands': ["PBEQA0", "PBEQA1"],
            'command_prefix': "PBEQA",
            'command_status': "QBEQI",
            'request_index': 9,
        },
        # QPIRI field 16 (program 01). MAX POP is only 00/01/02.
        # Value 3 on main is SolarBatUtility* (LCD UtS). Do not send POP03.
        'output_source_priority': {
            'options': [
                "USB — сеть сначала",
                "SUB — сначала солнце",
                "SBU — солнце, затем батарея",
                "UtS — солнце, сеть когда нет PV",
            ],
            'parameters': ["0", "1", "2", "3"],
            'set_commands': ["POP00", "POP01", "POP02", ""],
            'command_prefix': "POP",
            'command_status': "QPIRI",
            'request_index': 16,
        },
    }
    for field, opt_data in select_fields_options.items():
        if field in config:
            conf = config[field]
            sel = cg.new_Pvariable(conf[CONF_ID])
            await select.register_select(sel, conf, options=opt_data['options'])
            cg.add(sel.set_options_list(opt_data['options']))
            cg.add(sel.set_command_prefix(opt_data['command_prefix']))
            cg.add(sel.set_parameters(opt_data['parameters']))
            cg.add(sel.set_set_commands(opt_data.get('set_commands', [])))
            cg.add(sel.set_field_name(field))
            if opt_data.get('command_status'):
                cg.add(sel.set_status_command(opt_data['command_status']))
            cg.add(var.add_inverter_select(opt_data['request_index'], sel))
            setter_name = f"set_{field}"
            if hasattr(var, setter_name):
                cg.add(getattr(var, setter_name)(sel))

    # sensors
    sensors_qbeqi = {
        "equalization_elapsed_time": "set_equalization_elapsed_time",
        "equalization_max_current": "set_equalization_max_current",
    }
    for key, setter in sensors_qbeqi.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))

    # Number fields: min/max/step from Voltronic/MAX + this component's P* commands.
    # Formats use float specifiers (control() passes float). pipsolar-style ranges.
    number_fields = {
        'equalization_voltage': {       'fmt': "%.2f", 'cmd': "PBEQV", 'min': 48.0, 'max': 61.0, 'step': 0.1},
        'equalization_time': {          'fmt': "%03.0f", 'cmd': "PBEQT", 'min': 5, 'max': 900, 'step': 5},
        'equalization_over_time': {     'fmt': "%03.0f", 'cmd': "PBEQOT", 'min': 5, 'max': 900, 'step': 5},
        'equalization_period': {        'fmt': "%03.0f", 'cmd': "PBEQP", 'min': 0, 'max': 90, 'step': 1},
        # QPIRI writable setpoints (48V MAX / pipsolar)
        'battery_recharge_voltage': {   'fmt': "%02.1f", 'cmd': "PBCV", 'min': 44, 'max': 51, 'step': 1},
        'battery_redischarge_voltage': {'fmt': "%02.1f", 'cmd': "PBDV", 'min': 48, 'max': 58, 'step': 1},
        'max_charging_current': {       'fmt': "%03.0f", 'cmd': "MNCHGC", 'min': 10, 'max': 120, 'step': 10},
        'max_ac_charging_current': {    'fmt': "%03.0f", 'cmd': "MUCHGC", 'min': 10, 'max': 100, 'step': 10},
        'ac_output_rating_frequency': { 'fmt': "%02.0f", 'cmd': "F", 'min': 50, 'max': 60, 'step': 10},
        'ac_output_rating_voltage': {   'fmt': "%03.0f", 'cmd': "V", 'min': 220, 'max': 240, 'step': 10},
    }


    for field, props in number_fields.items():
        if field in config:
            nconf = config[field]
            num = cg.new_Pvariable(nconf[CONF_ID])

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

            setter_name = f"set_{field}"
            if hasattr(var, setter_name):
                cg.add(getattr(var, setter_name)(num))

    debug_buttons = {
        'debug_query_qpi': ('QPI', False),
        'debug_query_qid': ('QID', False),
        'debug_query_qvfw': ('QVFW', False),
        'debug_query_qvfw2': ('QVFW2', False),
        'debug_query_qmn': ('QMN', False),
        'debug_query_qgmn': ('QGMN', False),
        'debug_query_qmod': ('QMOD', False),
        'debug_query_qflag': ('QFLAG', False),
        'debug_query_qpiri': ('QPIRI', False),
        'debug_query_qpigs': ('QPIGS', False),
        'debug_query_qpiws': ('QPIWS', False),
        'debug_query_qbeqi': ('QBEQI', False),
        'debug_query_qdi': ('QDI', False),
        'debug_query_qoppt': ('QOPPT', False),
        'debug_query_qmchgcr': ('QMCHGCR', False),
        'debug_query_qmuchgcr': ('QMUCHGCR', False),
        'debug_dump_inquiries': ('', True),
    }
    for key, (cmd, dump_all) in debug_buttons.items():
        if key in config:
            btn = await button.new_button(config[key])
            cg.add(btn.set_parent(var))
            cg.add(btn.set_inquiry_command(cmd))
            cg.add(btn.set_dump_all(dump_all))



