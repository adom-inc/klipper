from __future__ import division
import logging
import mcu

class HX711(object):  
    def __init__(self, config):
        self.printer = config.get_printer()
        self.name = config.get_name().split()[-1]
        self.reactor = self.printer.get_reactor()
        self.mcu = mcu.get_printer_mcu(self.printer,
                                       config.get('hx711_mcu', 'mcu'))
        self.oid = self.mcu.create_oid()

        ppins = config.get_printer().lookup_object("pins")
        sck_params = ppins.lookup_pin(config.get('hx711_sck_pin'))
        dout_params = ppins.lookup_pin(config.get('hx711_dout_pin'))

        self.mcu.add_config_cmd(
            "config_hx711 oid=%d  sck_pin=%s dout_pin=%s" % (self.oid, sck_params['pin'], dout_params['pin']))

        self.report_time = config.getfloat('hx711_report_time', 2, minval=0.02)
        self.scale = config.getfloat('hx711_scale', 0.001)
        self.id = config.get('hx711_id')

        self.weight = 0.0
        self.sample_timer = self.reactor.register_timer(self._sample_hx711)
        self.printer.add_object("hx711" + self.name, self)
        self.printer.register_event_handler("klippy:connect", self.handle_connect)

        self.cmd_queue = self.mcu.alloc_command_queue()
        self.mcu.register_config_callback(self.build_config)

    def handle_connect(self):
        self.reactor.update_timer(self.sample_timer, self.reactor.NOW)
        self._register_gcode_commands()
        return

    def _register_gcode_commands(self):
        gcode = self.printer.lookup_object('gcode')
        command_name = f'QUERY_WEIGHT_{self.id}'
        gcode.register_command(command_name, self.cmd_query_weight,
                               desc=f"Query the current weight from hx711_id_{self.id}")

    def cmd_query_weight(self, params):
        gcode = self.printer.lookup_object('gcode')
        weight = self.get_status(self.reactor.monotonic())['weight']
        gcode.respond_info("hx711 Weight: {:.2f}".format(weight))

    def build_config(self):
        self.read_hx711_cmd = self.mcu.lookup_query_command(
            "read_hx711 oid=%c read_len=%u",
            "read_hx711_response oid=%c response=%*s", oid=self.oid,
            cq=self.cmd_queue)

    def read_hx711(self, read_len):
        return self.read_hx711_cmd.send([self.oid, read_len])

    def setup_callback(self, cb):
        self._callback = cb

    def get_report_time_delta(self):
        return self.report_time

    def _sample_hx711(self, eventtime):
        params = self.read_hx711(4)

        response = bytearray(params['response'])
        w = sum((b << (i * 8)) for i, b in enumerate(response))
        self.weight = w * self.scale  # weight scale

        logging.info(" read hx711 @ %.3f , weight:%.2f", eventtime, self.weight)
        return eventtime + self.report_time

    def get_status(self, eventtime):
        return {
            'weight': round(self.weight, 2)
        }

def load_config(config):
    return HX711(config)

def load_config_prefix(config):
    return HX711(config)