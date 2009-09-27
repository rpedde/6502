#include <stdio.h>

#include "../hardware.h"

hw_reg_t hwreg = {
    HW_OTHER,
    {
        { 0xD000, 0xD02E, 0, 0, 1, 1 },  /* 6566 video control */
        { 0x0400, 0x07FF, 0, 0, 1, 1 },  /* Video memory */
    }
}

