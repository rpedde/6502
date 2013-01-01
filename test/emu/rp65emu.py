#!/usr/bin/env python

import struct

class RP65Emu:
    CMD_NOP = 0
    CMD_VER = 1
    CMD_REGS = 2       # cpu_state struct
    CMD_READMEM = 3    # param1: start, param2: len
    CMD_WRITEMEM = 4   # param1: start, param2: len, extra: data
    CMD_LOAD = 5       # param1: start, param2: zt filename
    CMD_SET = 6        # param1: register, param2: value
    CMD_NEXT = 7       # step
    CMD_STOP = 255     # terminate emulator

    RESPONSE_OK = 0
    RESPONSE_ERROR = 1

    PARAM_A = 1
    PARAM_X = 2
    PARAM_Y = 3
    PARAM_P = 4
    PARAM_SP = 5
    PARAM_IP = 6

    def __init__(self, fifo_path='/tmp/debug'):
        self.cmd_fd = open('%s-cmd' % fifo_path, 'rb+', 0)
        self.rsp_fd = open('%s-rsp' % fifo_path, 'rb+', 0)
        self._update_registers()

    def _send_command(self, cmd, param1, param2, extra_len, extra_data):
        # typedef struct dbg_command_t_struct {
        #     uint8_t cmd;
        #     uint16_t param1;
        #     uint16_t param2;
        #     uint16_t extra_len;
        # } dbg_command_t;

        # typedef struct dbg_response_t_struct {
        #     uint8_t response_status;
        #     uint16_t extra_len;
        # } dbg_response_t;

        req = None
        rsp = None

        if extra_len > 0:
            fmt = 'BHHH%ds' % extra_len
            req = struct.pack(fmt, cmd, param1, param2, extra_len, extra_data)
        else:
            req = struct.pack('BHHH', cmd, param1, param2, 0)

        self.cmd_fd.write(req);

        rsp = self.rsp_fd.read(struct.calcsize('BH'))

        rsp_extra_data = None
        rsp_status, rsp_extra_len = struct.unpack('BH', rsp)

        if rsp_status != self.RESPONSE_OK:
            raise 'error sending command'

        if rsp_extra_len != 0:
            rsp_extra_data = self.rsp_fd.read(rsp_extra_len)

        return rsp_extra_data

    def _update_registers(self):
        data = self._send_command(self.CMD_REGS, 0, 0, 0, None)

        (self._p, self._a, self._x, self._y,
         self._ip, self._sp, self._irq) = struct.unpack('BBBBHBB', data)

    @property
    def a(self):
        return self._a

    @a.setter
    def a(self, value):
        self._a = value
        self._send_command(self.CMD_SET, self.PARAM_A, value, 0, 0)

    @property
    def x(self):
        return self._x

    @x.setter
    def x(self, value):
        self._x = value
        self._send_command(self.CMD_SET, self.PARAM_X, value, 0, 0)

    @property
    def y(self):
        return self._y

    @y.setter
    def y(self, value):
        self._y = value
        self._send_command(self.CMD_SET, self.PARAM_Y, value, 0, 0)

    @property
    def pc(self):
        return self._ip

    @pc.setter
    def pc(self, value):
        self._ip = value
        self._send_command(self.CMD_SET, self.PARAM_IP, value, 0, 0)

    @property
    def sp(self):
        return self._sp

    @sp.setter
    def sp(self, value):
        self._sp = value
        self._send_command(self.CMD_SET, self.PARAM_SP, value, 0, 0)

    @property
    def p(self):
        return self._p

    @p.setter
    def p(self, value):
        self._p = value
        self._send_command(self.CMD_SET, self.PARAM_P, value, 0, 0)

    def get_memory(self, start, length):
        data = self._send_command(self.CMD_READMEM, start,
                                  length, 0, None)
        return data

    def set_memory(self, start, data):
        data_length = len(data)

        self._send_command(self.CMD_WRITEMEM, start, data_length,
                           data_length, data)
