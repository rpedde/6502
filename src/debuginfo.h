/*
 * Copyright (C) 2013 Ron Pedde <ron@pedde.com>
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

#ifndef _DEBUGINFO_H_
#define _DEBUGINFO_H_

extern int debuginfo_init(void);
extern int debuginfo_deinit(void);
extern int debuginfo_getline(uint16_t addr, char *buffer, int len);
extern int debuginfo_load(char *file);
extern int debuginfo_lookup_symbol(char *symbol, uint16_t *value);
extern int debuginfo_lookup_addr(uint16_t addr, char **symbol);

#endif /* _DEBUGINFO_H_ */
