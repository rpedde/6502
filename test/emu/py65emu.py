#!/usr/bin/env python

from py65.devices.mpu6502 import MPU as NMOS6502
from py65.utils.addressing import AddressParser
from py65.memory import ObservableMemory
from py65.disassembler import Disassembler

class PY65Emu(object):
    def __init__(self):
        self.mpu = NMOS6502()
        self.address_parser = AddressParser()
        # self.assembler = Assembler(self.mpu, self.address_parser)

        m = ObservableMemory(addrWidth=self.mpu.ADDR_WIDTH)
        self.mpu.memory = m
        self.disassembler = Disassembler(self.mpu, self.address_parser)

    @property
    def a(self):
        return self.mpu.a

    @a.setter
    def a(self, value):
        self.mpu.a = value

    @property
    def x(self):
        return self.mpu.x

    @x.setter
    def x(self, value):
        self.mpu.x = value

    @property
    def y(self):
        return self.mpu.y

    @y.setter
    def y(self, value):
        self.mpu.y = value

    @property
    def pc(self):
        return self.mpu.pc

    @pc.setter
    def pc(self, value):
        self.mpu.pc = value

    @property
    def sp(self):
        return self.mpu.sp

    @sp.setter
    def sp(self, value):
        self.mpu.sp = value

    @property
    def p(self):
        return self.mpu.p

    @p.setter
    def p(self, value):
        self.mpu.p = value

    def get_memory(self, start, length):
        return self.mpu.memory[start:start+length]

    def set_memory(self, start, data):
        self.mpu.memory[start:start+len(data)] = data

    def step(self):
        self.mpu.step()

    def disassemble(self, pc):
        return self.disassembler.instruction_at(pc)
