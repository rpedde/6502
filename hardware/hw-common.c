/*
 * Copyright (C) 2012 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hardware.h"
#include "hw-common.h"

char *config_get(hw_config_t *config, char *key) {
    assert(config);

    if(!config)
        return NULL;

    for(int x=0; x < config->config_items; x++)
        if (strcasecmp(key, config->item[x].key)==0)
            return (char*)config->item[x].value;

    return NULL;
}

int config_get_uint16(hw_config_t *config, char *key, uint16_t *value) {
    char *str_value;
    char *fmt = "%d";

    str_value = config_get(config, key);
    if(!str_value)
        return 0;

    if(str_value[0] == 'x')
        fmt = "x%x";
    else if (str_value[0] == '0')
        fmt = "%o";

    if (sscanf(str_value, fmt, value) == 1)
        return 1;

    return 0;
}
