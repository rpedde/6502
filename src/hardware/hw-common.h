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

#ifndef _HW_COMMON_H_
#define _HW_COMMON_H_

extern char *config_get(hw_config_t *config, char *key);
extern int config_get_uint16(hw_config_t *config, char *key, uint16_t *value);

#define DBG_FATAL 0
#define DBG_ERROR 1
#define DBG_WARN  2
#define DBG_INFO  3
#define DBG_DEBUG 4

#define NOTIFY(format, args...) hardware_callbacks->hw_notify(format, ##args)

#if defined(NDEBUG)
#define DEBUG(format, args...)
#define INFO(format, args...)
#define WARN(format, args...)
#define ERROR(format, args...) hardware_callbacks->hw_logger(DBG_ERROR, "Error: " format "\n", ##args)
#define FATAL(format, args...) hardware_callbacks->hw_logger(DBG_FATAL, "Fatal: " format "\n", ##args)
# define DPRINTF(level, format, args...);
#else
#define DEBUG(format, args...) hardware_callbacks->hw_logger(DBG_DEBUG, "[DEBUG] %s:%d (%s): " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#define INFO(format, args...) hardware_callbacks->hw_logger(DBG_INFO, "[INFO] %s:%d (%s): " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#define WARN(format, args...) hardware_callbacks->hw_logger(DBG_WARN, "[WARN] %s:%d (%s): " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#define ERROR(format, args...) hardware_callbacks->hw_logger(DBG_ERROR, "[ERROR] %s:%d (%s): " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#define FATAL(format, args...) hardware_callbacks->hw_logger(DBG_FATAL, "[FATAL] %s:%d (%s): " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args)
#define DPRINTF(level, format, args...)  hardware_callbacks->hw_logger(level, "[%s] %s:%d (%s): " format "\n", #level, __FILE__, __LINE__, __FUNCTION__, ##args)
#endif /* NDEBUG */


#endif /* _HW_COMMON_H_ */
