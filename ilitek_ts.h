/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Jijie Wang <jijie_wang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 */

#ifndef __ILITEK_TS_H__
#define __ILITEK_TS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

//#define ILITEK_TOOL

//#define ILITEK_ESD_PROTECTION
//#define ILITEK_CHECK_FUNCMODE
#define ILITEK_TOUCH_PROTOCOL_B
//#define ILITEK_REPORT_PRESSURE
//#define ILITEK_USE_LCM_RESOLUTION

#define ILITEK_ROTATE_FLAG											0
#define ILITEK_REVERT_X												0
#define ILITEK_REVERT_Y												0
#define TOUCH_SCREEN_X_MAX   										(1080)	//LCD_WIDTH
#define TOUCH_SCREEN_Y_MAX   										(1920)	//LCD_HEIGHT
#define ILITEK_RESOLUTION_MAX										16384
#define ILITEK_ENABLE_REGULATOR_POWER_ON
#define ILITEK_GET_GPIO_NUM

#define ILITEK_CLICK_WAKEUP                                         0
#define ILITEK_DOUBLE_CLICK_WAKEUP                                  1
#define ILITEK_GESTURE_WAKEUP                                       2
#define ILITEK_GESTURE                                              ILITEK_CLICK_WAKEUP

#ifdef ILITEK_GESTURE
#define ILITEK_GET_TIME_FUNC_WITH_TIME								0
#define ILITEK_GET_TIME_FUNC_WITH_JIFFIES							1
#define ILITEK_GET_TIME_FUNC										ILITEK_GET_TIME_FUNC_WITH_JIFFIES
#endif

#define DOUBLE_CLICK_DISTANCE										1000
#define DOUBLE_CLICK_ONE_CLICK_USED_TIME							800
#define DOUBLE_CLICK_NO_TOUCH_TIME									1000
#define DOUBLE_CLICK_TOTAL_USED_TIME								(DOUBLE_CLICK_NO_TOUCH_TIME + (DOUBLE_CLICK_ONE_CLICK_USED_TIME * 2))

//#define ILITEK_UPDATE_FW
#define RAW_DATA_TRANSGER_LENGTH                                    1024
#define UPGRADE_LENGTH_BLV1_8               						2048

#define ILI_UPDATE_BY_CHECK_INT

#define ILITEK_TS_NAME												"ilitek_ts"
#define ILITEK_TUNING_MESSAGE										0xDB
#define ILITEK_TUNING_NODE											0xDB

#ifdef __cplusplus
} // extern "C"
#endif
#endif
