/*
 *
 */

#include <stdio.h>

#include "../hardware.h"

hw_reg_t 6510_hwreg = {
    HW_MEMORY_CONTROLLER,
    {
        { 0x0000, 0x0001, 0, 0, 0, 1 }
        { 0x0000, 0x0000, 0, 0, 0, 0 }
    }
}

