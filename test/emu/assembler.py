#!/usr/bin/env python

from py65.devices.mpu6502 import MPU as NMOS6502
from py65.disassembler import Disassembler
from py65.assembler import Assembler
from py65.utils.addressing import AddressParser
from py65.memory import ObservableMemory

mpu = NMOS6502()
address_parser = AddressParser()
assembler = Assembler(mpu, address_parser)

m = ObservableMemory(addrWidth=mpu.ADDR_WIDTH)
mpu.memory = m

def assemble(statements, start):
    result = None

    for statement in statements:
        bytes = assembler.assemble(statement, start)
        result += bytes
        start += len(bytes)

    return bytes
