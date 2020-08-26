/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Luca Hsu <luca_hsu@ilitek.com>
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
#include "ilitek_ts.h"
#include "ilitek_common.h"
//#include <linux/slab.h>
//#include <linux/delay.h>
#include "ilitek_protocol.h"

struct ilitek_ts_data ilitek_ctx;
struct ilitek_ts_data * ilitek_data = &ilitek_ctx;
PROTOCOL_MAP *ptl_cb_func;

// ----------------------------------------------------------------------------

void ilitek_reset(int delay)
{
    ilitek_gpio_reset_set( 1 );
    ilitek_delay(10);
    ilitek_gpio_reset_set( 0 );
    ilitek_delay(10);
    ilitek_gpio_reset_set( 1 );
    ilitek_delay(delay);
}

int32_t ilitek_check_busy(int32_t count, int32_t delay, int32_t type)
{
    int32_t i;
    uint8_t buf[2];

    for (i = 0; i < count; i++) {
        buf[0] = ILITEK_TP_CMD_GET_SYSTEM_BUSY;
        if (ilitek_i2c_rw(buf, 1, 1, buf, 1) < 0) {
            return ILITEK_I2C_TRANSFER_ERR;
        }
        if ((buf[0] & (ILITEK_TP_SYSTEM_READY + type)) == ILITEK_TP_SYSTEM_READY) {
            //LOG_INFO("check_busy i = %d\n", i);
            return ILITEK_SUCCESS;
        }
        ilitek_sleep(delay);
    }
    LOG_INFO("check_busy is busy,0x%x\n", buf[0]);
    return ILITEK_FAIL;
}

int ilitek_read_data_and_report_3XX(void)
{
    int ret = 0;
    int packet = 0;
    int report_max_point = 6;
    int release_point = 0;
    int tp_status = 0;
    int i = 0;
    int x = 0;
    int y = 0;
    struct input_dev *input = ilitek_data->input_dev;
    uint8_t buf[64] = { 0 };
#ifndef ILITEK_CHECK_FUNCMODE
    buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
    ret = ilitek_i2c_rw(buf, 1, 0, buf, 31);
    if (ret < 0) {
        LOG_ERR("get touch information err\n");
        if (ilitek_data->is_touched) {
#if 0
            ilitek_touch_release_all_point();
            ilitek_check_key_release(x, y, 0);
#endif
        }
        return ret;
    }
#else
    uint8_t mode_status = 0;

    buf[0] = ILITEK_TP_CMD_GET_TOUCH_INFORMATION;
    ret = ilitek_i2c_rw(buf, 1, 0, buf, 32);
    if (ret < 0) {
        LOG_ERR("get touch information err\n");
        if (ilitek_data->is_touched) {
            ilitek_touch_release_all_point();
            ilitek_check_key_release(x, y, 0);
        }
        return ret;
    }
    mode_status = buf[31];
    LOG_DEBUG("mode_status = 0x%X\n", mode_status);
    if (mode_status & 0x80)
        LOG_DEBUG("Palm reject mode enable\n");
    if (mode_status & 0x40)
        LOG_DEBUG("Thumb mdoe enable\n");
    if (mode_status & 0x04)
        LOG_DEBUG("Water mode enable\n");
    if (mode_status & 0x02)
        LOG_DEBUG("Mist mode enable\n");
    if (mode_status & 0x01)
        LOG_DEBUG("Normal mode\n");
#endif
    packet = buf[0];
    if (packet == 2) {
        ret = ilitek_i2c_read(buf + 31, 20);
        if (ret < 0) {
            LOG_ERR("get touch information packet 2 err\n");
            if (ilitek_data->is_touched) {
#if 0
                ilitek_touch_release_all_point();
                ilitek_check_key_release(x, y, 0);
#endif
            }
            return ret;
        }
        report_max_point = 10;
    }

    if (buf[1] == 0x5F || buf[0] == 0xDB) {
        LOG_DEBUG("debug message return\n");
        return 0;
    }
    for (i = 0; i < report_max_point; i++) {
        tp_status = buf[i * 5 + 1] >> 7;
        LOG_DEBUG("ilitek tp_status = %d buf[i*5+1] = 0x%X\n", tp_status, buf[i * 5 + 1]);
        if (tp_status) {
            ilitek_data->touch_flag[i] = 1;
            ilitek_data->tp[i].x = x = ((buf[i * 5 + 1] & 0x3F) << 8) + buf[i * 5 + 2];
            ilitek_data->tp[i].y = y = (buf[i * 5 + 3] << 8) + buf[i * 5 + 4];
            LOG_DEBUG("ilitek x = %d y = %d\n", x, y);
            if (ilitek_data->system_suspend) {
                LOG_INFO("system is suspend not report point\n");
#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
                finger_state = ilitek_double_click_touch(x, y, finger_state, i);
#endif
#endif
            } else {
#if 0
                if (!(ilitek_data->is_touched))
                    ilitek_check_key_down(x, y);
#endif
                if (!(ilitek_data->touch_key_hold_press)) {
                    if (x > ilitek_data->screen_max_x || y > ilitek_data->screen_max_y ||
                        x < ilitek_data->screen_min_x || y < ilitek_data->screen_min_y) {
                        LOG_INFO("Point (x > screen_max_x || y > screen_max_y) , ID=%02X, X=%d, Y=%d\n", i, x, y);
                        LOG_INFO("Packet is 0x%X. This ID read buf: 0x%X,0x%X,0x%X,0x%X,0x%X\n",
                                buf[0], buf[i * 5 + 1], buf[i * 5 + 2], buf[i * 5 + 3], buf[i * 5 + 4], buf[i * 5 + 5]);
                    } else {
                        ilitek_data->is_touched = true;
                        if (ILITEK_REVERT_X)
                            ilitek_data->tp[i].x = x = ilitek_data->screen_max_x - x + ilitek_data->screen_min_x;
                        if (ILITEK_REVERT_Y)
                            ilitek_data->tp[i].y = y = ilitek_data->screen_max_y - y + ilitek_data->screen_min_y;
                        LOG_DEBUG("Point, ID=%02X, X=%04d, Y=%04d\n", i, x, y);
#if 0
                        ilitek_touch_down(i, x, y, 10, 128, 1);
#endif
                    }
                }
                //if ((ilitek_data->touch_key_hold_press)){
                //      ilitek_check_key_release(x, y, 1);
                //}
            }
        } else {
            ilitek_data->touch_flag[i] = 0;
            release_point++;
#ifdef ILITEK_TOUCH_PROTOCOL_B
#if 0
            ilitek_touch_release(i);
#endif
#endif
        }
    }
    LOG_DEBUG("release point counter =  %d packet = %d\n", release_point, packet);
    if (packet == 0 || release_point == report_max_point) {
#if 0
        if (ilitek_data->is_touched)
            ilitek_touch_release_all_point();
        ilitek_check_key_release(x, y, 0);
#endif
        ilitek_data->is_touched = false;
        if (ilitek_data->system_suspend) {
#ifdef ILITEK_GESTURE
#if ILITEK_GESTURE == ILITEK_CLICK_WAKEUP
#if 0
            wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
            input_report_key(input, KEY_POWER, 1);
            input_sync(input);
            input_report_key(input, KEY_POWER, 0);
            input_sync(input);
            //ilitek_data->system_suspend = false;
#endif
#elif ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
            finger_state = ilitek_double_click_release(finger_state);
            if (finger_state == 5) {
                LOG_INFO("double click wakeup\n");
                wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
                input_report_key(input, KEY_POWER, 1);
                input_sync(input);
                input_report_key(input, KEY_POWER, 0);
                input_sync(input);
                //ilitek_data->system_suspend = false;
            }
#endif
#endif
        }
    }
#if 0
    input_sync(input);
#endif
    return 0;
}
int ilitek_read_data_and_report_6XX(void) {
    int ret = 0;
    int count = 0;
    int report_max_point = 6;
    int release_point = 0;
    int i = 0;
    struct input_dev *input = ilitek_data->input_dev;
    unsigned char buf[512]={0};
    int packet_len = 0;
    int packet_max_point = 0;

//  if (ilitek_data->system_suspend) {
//      LOG_INFO("system is suspend not report point\n");
// #ifdef ILITEK_GESTURE
// #if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
//      finger_state = ilitek_double_click_touch(x, y, finger_state, i);
// #endif
// #endif
//  }
    LOG_DEBUG("ilitek_read_data_and_report_6XX: format:%d\n", ilitek_data->format);
    //initial touch format
    switch (ilitek_data->format)
    {
    case 0:
        packet_len = REPORT_FORMAT0_PACKET_LENGTH;
        packet_max_point = REPORT_FORMAT0_PACKET_MAX_POINT;
        break;
    case 1:
        packet_len = REPORT_FORMAT1_PACKET_LENGTH;
        packet_max_point = REPORT_FORMAT1_PACKET_MAX_POINT;
        break;
    case 2:
        packet_len = REPORT_FORMAT2_PACKET_LENGTH;
        packet_max_point = REPORT_FORMAT2_PACKET_MAX_POINT;
        break;
    case 3:
        packet_len = REPORT_FORMAT3_PACKET_LENGTH;
        packet_max_point = REPORT_FORMAT3_PACKET_MAX_POINT;
        break;
    default:
        packet_len = REPORT_FORMAT0_PACKET_LENGTH;
        packet_max_point = REPORT_FORMAT0_PACKET_MAX_POINT;
        break;
    }
    for(i = 0; i < ilitek_data->max_tp; i++)
        ilitek_data->tp[i].status = false;

    ret = ilitek_i2c_read(buf, 64);
    if (ret < 0) {
        LOG_ERR("get touch information err\n");
        if (ilitek_data->is_touched) {
#if 0
            ilitek_touch_release_all_point();
            ilitek_check_key_release(0, 0, 0);
#endif
        }
        return ret;
    }

    report_max_point = buf[REPORT_ADDRESS_COUNT];
    if(report_max_point > ilitek_data->max_tp) {
        LOG_ERR("FW report max point:%d > panel information max point:%d\n", report_max_point, ilitek_data->max_tp);
        return ILITEK_FAIL;
    }
    count = CEIL(report_max_point, packet_max_point); //(report_max_point / 10) + 1;
    for(i = 1; i < count; i++) {
        ret = ilitek_i2c_read(buf+i*64, 64);
        if (ret < 0) {
            LOG_ERR("get touch information err, count=%d\n", count);
            if (ilitek_data->is_touched) {
#if 0
                ilitek_touch_release_all_point();
                ilitek_check_key_release(0, 0, 0);
#endif
            }
            return ret;
        }
    }
    for (i = 0; i < report_max_point; i++) {
        ilitek_data->tp[i].status = buf[i*packet_len+1] >> 6;
        ilitek_data->tp[i].id = buf[i*packet_len+1] & 0x3F;
        LOG_DEBUG("ilitek tp_status = %d buf[i*5+1] = 0x%X\n", ilitek_data->tp[i].status, buf[i*packet_len+1]);
        if (ilitek_data->tp[i].status) {
            ilitek_data->touch_flag[ilitek_data->tp[i].id] = 1;
            ilitek_data->tp[i].x = ((buf[i*packet_len+3]) << 8) + buf[i*packet_len+2];
            ilitek_data->tp[i].y = (buf[i*packet_len+5] << 8) + buf[i*packet_len+4];
            if(ilitek_data->format == 0) {
                ilitek_data->tp[i].p = 10;
                ilitek_data->tp[i].w = 10;
                ilitek_data->tp[i].h = 10;
            }
            if(ilitek_data->format == 1 || ilitek_data->format == 3) {
                ilitek_data->tp[i].p = buf[i*packet_len+6];
                ilitek_data->tp[i].w = 10;
                ilitek_data->tp[i].h = 10;
            }
            if(ilitek_data->format == 2 || ilitek_data->format == 3) {
                ilitek_data->tp[i].p = 10;
                ilitek_data->tp[i].w = buf[i*packet_len+7];
                ilitek_data->tp[i].h = buf[i*packet_len+8];
            }

            LOG_DEBUG("ilitek id = %d, x = %d y = %d\n", ilitek_data->tp[i].id, ilitek_data->tp[i].x, ilitek_data->tp[i].y);
            if (ilitek_data->system_suspend) {
                LOG_INFO("system is suspend not report point\n");
        #ifdef ILITEK_GESTURE
        #if ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
                finger_state = ilitek_double_click_touch(ilitek_data->tp[i].x, ilitek_data->tp[i].y, finger_state, i);
        #endif
        #endif
            }
            else {
                if(!(ilitek_data->is_touched)) {
#if 0
                    ilitek_check_key_down(ilitek_data->tp[i].x, ilitek_data->tp[i].y);
#endif
                }
                if (!(ilitek_data->touch_key_hold_press)) {
                    if (ilitek_data->tp[i].x > ilitek_data->screen_max_x || ilitek_data->tp[i].y > ilitek_data->screen_max_y ||
                        ilitek_data->tp[i].x < ilitek_data->screen_min_x || ilitek_data->tp[i].y < ilitek_data->screen_min_y) {

                        LOG_INFO("Point (x > screen_max_x || y > screen_max_y) , ID=%02X, X=%d, Y=%d\n",
                            ilitek_data->tp[i].id, ilitek_data->tp[i].x, ilitek_data->tp[i].y);
                    }
                    else {
                        ilitek_data->is_touched = true;
                        if (ILITEK_REVERT_X) {
                            ilitek_data->tp[i].x = ilitek_data->screen_max_x - ilitek_data->tp[i].x + ilitek_data->screen_min_x;
                        }
                        if (ILITEK_REVERT_Y) {
                            ilitek_data->tp[i].y = ilitek_data->screen_max_y - ilitek_data->tp[i].y + ilitek_data->screen_min_y;
                        }

                        LOG_DEBUG("Point, ID=%02X, X=%04d, Y=%04d, P=%d, H=%d, W=%d\n",
                            ilitek_data->tp[i].id, ilitek_data->tp[i].x,ilitek_data->tp[i].y,
                            ilitek_data->tp[i].p, ilitek_data->tp[i].h, ilitek_data->tp[i].w);
#if 0
                        ilitek_touch_down(ilitek_data->tp[i].id, ilitek_data->tp[i].x, ilitek_data->tp[i].y,
                        ilitek_data->tp[i].p, ilitek_data->tp[i].h, ilitek_data->tp[i].w);
#endif
                    }
                }
                if ((ilitek_data->touch_key_hold_press)){
#if 0
                    ilitek_check_key_release(ilitek_data->tp[i].x, ilitek_data->tp[i].y, 1);
#endif
                }
            }
        }
        else {
            release_point++;
            #ifdef ILITEK_TOUCH_PROTOCOL_B
#if 0
            ilitek_touch_release(ilitek_data->tp[i].id);
#endif
            #endif
        }
    }
    LOG_DEBUG("release point counter =  %d , report max point = %d\n", release_point, report_max_point);
    if (release_point == report_max_point) {
        if (ilitek_data->is_touched) {
#if 0
            ilitek_touch_release_all_point();
#endif
        }
#if 0
        ilitek_check_key_release(0, 0, 0);
#endif
        ilitek_data->is_touched = false;
        if (ilitek_data->system_suspend) {
        #ifdef ILITEK_GESTURE
            #if ILITEK_GESTURE == ILITEK_CLICK_WAKEUP
#if 0
            wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
            input_report_key(input, KEY_POWER, 1);
            input_sync(input);
            input_report_key(input, KEY_POWER, 0);
            input_sync(input);
            //ilitek_data->system_suspend = false;
#endif
            #elif ILITEK_GESTURE == ILITEK_DOUBLE_CLICK_WAKEUP
            finger_state = ilitek_double_click_release(finger_state);
            if (finger_state == 5) {

                LOG_INFO("double click wakeup\n");
#if 0
                wake_lock_timeout(&ilitek_wake_lock, 5 * HZ);
                input_report_key(input, KEY_POWER, 1);
                input_sync(input);
                input_report_key(input, KEY_POWER, 0);
                input_sync(input);
                //ilitek_data->system_suspend = false;
#endif
            }
            #endif
        #endif
        }
    }
#if 0
    input_sync(input);
#endif
    return 0;
}


//----------------------protocol function----------------------//
int api_protocol_control_funcmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_SET_FUNC_MODE;
	ret = ilitek_i2c_rw(inbuf, 4, 10, outbuf, 0);
	return ret;
}
int api_protocol_system_busy(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_GET_SYSTEM_BUSY;
	ret = ilitek_i2c_rw(inbuf, 1, 10, outbuf, 1);
	return ret;
}

int api_protocol_control_tsmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_CONTROL_TESTMODE;
	ret = ilitek_i2c_rw(inbuf, 2, 0, outbuf, 0);
	return ret;
}

int api_protocol_write_enable(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_WRITE_ENABLE;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = ilitek_i2c_rw(buf, 3, 0, outbuf, 0);
	return ret;
}

int api_protocol_set_apmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_SET_APMODE;
	ret = ilitek_i2c_rw(buf, 1, 0, outbuf, 0);
	return ret;
}

int api_protocol_set_blmode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_SET_BLMODE;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = ilitek_i2c_rw(buf, 1, 0, outbuf, 0);
	return ret;
}

int api_protocol_get_mcuver(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t buf[64] = {0};
	uint32_t ic = 0;

	buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
	if(ilitek_i2c_rw(buf, 1, 5, outbuf, 5) < 0){
		return ILITEK_FAIL;
	}
	for (i = 0; i < 5; i++)
		ilitek_data->mcu_ver[i] = outbuf[i];
	ic = ilitek_data->mcu_ver[0]+(ilitek_data->mcu_ver[1] << 8);
	LOG_INFO("MCU KERNEL version:0x%x\n", ic);
	if(ic == 0x2326 || ic == 0x2316 || ic == 0x2520) {
		ilitek_data->reset_time = 1000;
	}
	else {
		ilitek_data->reset_time = 200;
	}
    return ret;
}

int api_protocol_check_mode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_READ_MODE;
	ret = ilitek_i2c_rw(buf, 1, 5, outbuf, 2);
	LOG_INFO("ilitek ic. mode =%d , it's %s\n", outbuf[0], ((outbuf[0] == 0x5A) ? "AP MODE" : "BL MODE"));
#ifdef ILITEK_UPDATE_FW
	if (outbuf[0] == 0x55)
		ilitek_data->force_update = true;
#endif
    return ret;
}

int api_protocol_get_fwver(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_GET_FIRMWARE_VERSION;
	ret = ilitek_i2c_rw(buf, 1, 5, outbuf, 8);
	for (i = 0; i < 8; i++)
		ilitek_data->firmware_ver[i] = outbuf[i];
	LOG_INFO("firmware version:0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x.0x%x\n",
	outbuf[0], outbuf[1], outbuf[2], outbuf[3], outbuf[4], outbuf[5], outbuf[6], outbuf[7]);
    return ret;
}

int api_protocol_get_ptl(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_GET_PROTOCOL_VERSION;
	ret = ilitek_i2c_rw(buf, 1, 5, outbuf, 3);
	ilitek_data->ptl.ver = (((int)outbuf[0]) << 16) + (((int)outbuf[1]) << 8);
	ilitek_data->ptl.ver_major = outbuf[0];
	ilitek_data->ptl.ver_mid = outbuf[1];
	ilitek_data->ptl.ver_minor = outbuf[2];
	LOG_INFO("protocol version: %d.%d.%d  ilitek_data->ptl.ver = 0x%x\n",
	ilitek_data->ptl.ver_major, ilitek_data->ptl.ver_mid, ilitek_data->ptl.ver_minor, ilitek_data->ptl.ver);
    return ret;
}

int api_protocol_get_sc_resolution(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
    buf[0] = ILITEK_TP_CMD_GET_SCREEN_RESOLUTION;
	ret = ilitek_i2c_rw(buf, 1, 5, outbuf, 8);
	ilitek_data->screen_min_x = outbuf[0];
	ilitek_data->screen_min_x += ((int)outbuf[1]) * 256;
	ilitek_data->screen_min_y = outbuf[2];
	ilitek_data->screen_min_y += ((int)outbuf[3]) * 256;
	ilitek_data->screen_max_x = outbuf[4];
	ilitek_data->screen_max_x += ((int)outbuf[5]) * 256;
	ilitek_data->screen_max_y = outbuf[6];
	ilitek_data->screen_max_y += ((int)outbuf[7]) * 256;
	LOG_INFO("screen_min_x: %d, screen_min_y: %d screen_max_x: %d, screen_max_y: %d\n",
			ilitek_data->screen_min_x, ilitek_data->screen_min_y, ilitek_data->screen_max_x, ilitek_data->screen_max_y);
    return ret;
}

int api_protocol_get_v3_key_info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t buf[1024];                              //  Check minimum size...
	int key_num = ilitek_data->keycount;
    buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
    ret = ilitek_i2c_rw(buf, 1, 10, outbuf, ILITEK_KEYINFO_FIRST_PACKET);
    if (ret < 0)
        return ret;
    if (key_num > 5) {
        for (i = 0; i < CEIL(key_num, ILITEK_KEYINFO_FORMAT_LENGTH); i++) {
            LOG_INFO("read keyinfo times i = %d\n", i);
            ret = ilitek_i2c_rw(buf, 0, 10,
             outbuf + ILITEK_KEYINFO_FIRST_PACKET + ILITEK_KEYINFO_OTHER_PACKET * i, ILITEK_KEYINFO_OTHER_PACKET);
            if (ret < 0)
                return ret;
        }
    }

    ilitek_data->key_xlen = (outbuf[0] << 8) + outbuf[1];
    ilitek_data->key_ylen = (outbuf[2] << 8) + outbuf[3];
    LOG_INFO("key_xlen: %d, key_ylen: %d\n", ilitek_data->key_xlen, ilitek_data->key_ylen);

    //print key information
    for (i = 0; i < key_num; i++) {
        ilitek_data->keyinfo[i].id = outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_ID];
        ilitek_data->keyinfo[i].x = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_X_MSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_X_LSB];
        ilitek_data->keyinfo[i].y = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_Y_MSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V3_HEADER + ILITEK_KEYINFO_FORMAT_Y_LSB];
        ilitek_data->keyinfo[i].status = 0;
        LOG_INFO("key_id: %d, key_x: %d, key_y: %d, key_status: %d\n",
                ilitek_data->keyinfo[i].id, ilitek_data->keyinfo[i].x, ilitek_data->keyinfo[i].y, ilitek_data->keyinfo[i].status);
    }
    return ret;
}

int api_protocol_get_tp_resolution(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
	ret = ilitek_i2c_rw(buf, 1, 5, outbuf, 10);
	if (ret < 0)
		return ret;
	ilitek_data->max_tp = outbuf[6];
	ilitek_data->max_btn = outbuf[7];
	ilitek_data->keycount = outbuf[8];
	if (ilitek_data->keycount > 20) {
		LOG_ERR("exception keycount > 20 is %d set keycount = 0\n", ilitek_data->keycount);
		ilitek_data->keycount = 0;
	}
	ilitek_data->tp_max_x = outbuf[0];
	ilitek_data->tp_max_x += ((int)outbuf[1]) << 8;
	ilitek_data->tp_max_y = outbuf[2];
	ilitek_data->tp_max_y += ((int)outbuf[3]) << 8;
	ilitek_data->x_ch = outbuf[4];
	ilitek_data->y_ch = outbuf[5];
	if (ilitek_data->keycount > 0) {
		//get key infotmation
        ret = api_protocol_get_v3_key_info(NULL, outbuf);
        if (ret < 0)
		    return ret;
	}
	if (ilitek_data->max_tp > 40) {
		LOG_ERR("exception max_tp > 40 is %d set max_tp = 10\n", ilitek_data->max_tp);
		ilitek_data->max_tp = 10;
	}
	LOG_INFO("tp_min_x: %d, tp_max_x: %d, tp_min_y: %d, tp_max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, key_count: %d\n",
		    ilitek_data->tp_min_x, ilitek_data->tp_max_x, ilitek_data->tp_min_y, ilitek_data->tp_max_y, ilitek_data->x_ch,
		    ilitek_data->y_ch, ilitek_data->max_tp, ilitek_data->keycount);
    return ret;
}

int api_protocol_get_module_name(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_GET_KERNEL_VERSION;
	if(ilitek_i2c_rw(buf, 1, 5, buf, 32) < 0){
		return ILITEK_FAIL;
	}
	memcpy(outbuf, buf+6, 26);
	LOG_INFO("module name:%s\n", outbuf);
    return ret;
}

//--------------------------protocol v6-------------------------//
int api_protocol_get_v6_key_info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS, i = 0;
    uint8_t buf[1024];
	int key_num = ilitek_data->keycount;
	int w_len = key_num * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER;
    //buf = kmalloc(w_len, GFP_ATOMIC);
    buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
    ret = ilitek_i2c_rw(buf, 1, 10, outbuf, ILITEK_KEYINFO_FIRST_PACKET);
    if (ret < 0)
        return ret;

    ilitek_data->key_xlen = (outbuf[2] << 8) + outbuf[1];
    ilitek_data->key_ylen = (outbuf[4] << 8) + outbuf[3];
    LOG_INFO("v6 key_xlen: %d, key_ylen: %d\n", ilitek_data->key_xlen, ilitek_data->key_ylen);

    //print key information
    for (i = 0; i < key_num; i++) {
        ilitek_data->keyinfo[i].id = outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_ID];
        ilitek_data->keyinfo[i].x = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_X_LSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_X_MSB];
        ilitek_data->keyinfo[i].y = (outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_Y_LSB] << 8)
         + outbuf[i * ILITEK_KEYINFO_FORMAT_LENGTH + ILITEK_KEYINFO_V6_HEADER + ILITEK_KEYINFO_FORMAT_Y_MSB];
        ilitek_data->keyinfo[i].status = 0;
        LOG_INFO("key_id: %d, key_x: %d, key_y: %d, key_status: %d\n",
                ilitek_data->keyinfo[i].id, ilitek_data->keyinfo[i].x, ilitek_data->keyinfo[i].y, ilitek_data->keyinfo[i].status);
    }
    return ret;
}

int api_protocol_get_v6_tp_resolution(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_GET_TP_RESOLUTION;
	ret = ilitek_i2c_rw(buf, 1, 5, outbuf, 15);
	if (ret < 0)
		return ret;
	ilitek_data->tp_max_x = outbuf[0];
	ilitek_data->tp_max_x += ((int)outbuf[1]) << 8;
	ilitek_data->tp_max_y = outbuf[2];
	ilitek_data->tp_max_y += ((int)outbuf[3]) << 8;
	ilitek_data->x_ch = outbuf[4] + ((int)outbuf[5] << 8);
	ilitek_data->y_ch = outbuf[6] + ((int)outbuf[7] << 8);
	ilitek_data->max_tp = outbuf[8];
	ilitek_data->keycount = outbuf[9];
	ilitek_data->slave_count = outbuf[10];
	ilitek_data->format = outbuf[12];
	if (ilitek_data->keycount > 20) {
		LOG_ERR("exception keycount > 20 is %d set keycount = 0\n", ilitek_data->keycount);
		ilitek_data->keycount = 0;
	}
	if (ilitek_data->keycount > 0) {
		//get key infotmation
        ret = api_protocol_get_v6_key_info(NULL, outbuf);
        if (ret < 0)
		    return ret;
	}
	if (ilitek_data->max_tp > 40) {
		LOG_ERR("exception max_tp > 40 is %d set max_tp = 10\n", ilitek_data->max_tp);
		ilitek_data->max_tp = 40;
	}
	LOG_INFO("tp_min_x: %d, tp_max_x: %d, tp_min_y: %d, tp_max_y: %d, ch_x: %d, ch_y: %d, max_tp: %d, key_count: %d, slave_count: %d report format: %d\n",
		    ilitek_data->tp_min_x, ilitek_data->tp_max_x, ilitek_data->tp_min_y, ilitek_data->tp_max_y, ilitek_data->x_ch,
		    ilitek_data->y_ch, ilitek_data->max_tp, ilitek_data->keycount, ilitek_data->slave_count, ilitek_data->format);
    return ret;
}

int api_protocol_write_flash_enable(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};
	buf[0] = ILITEK_TP_CMD_WRITE_FLASH_ENABLE;
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	ret = ilitek_i2c_rw(buf, 3, 0, outbuf, 0);
	return ret;
}

int api_set_data_length(uint32_t data_len)
{
    int ret = ILITEK_SUCCESS;
    uint8_t buf[64] = {0};

	buf[0] = ILITEK_TP_CMD_SET_DATA_LENGTH;
	buf[1] = (uint8_t)(data_len & 0xFF);
	buf[2] = (uint8_t)(data_len >> 8);
	ret = ilitek_i2c_rw(buf, 3, 1, NULL, 0);
    return ret;
}

int api_write_flash_enable_BL1_8(uint32_t start,uint32_t end)
{
	uint8_t buf[64] = {0};

    buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_FLASH_ENABLE;//0xCC
    buf[1] = 0x5A;
    buf[2] = 0xA5;
    buf[3] = start & 0xFF;
    buf[4] = (start >> 8) & 0xFF;
    buf[5] = start >> 16;
    buf[6] = end & 0xFF;
    buf[7] = (end >> 8) & 0xFF;
    buf[8] = end >> 16;

	return ilitek_i2c_rw(buf, 9, 1, NULL, 0);
}

int api_protocol_get_block_crc(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
    uint16_t crc=0;

    inbuf[0] = ILITEK_TP_CMD_GET_BLOCK_CRC;
    if(inbuf[1] == 0) {
    	ret = ilitek_i2c_rw(inbuf, 8, 10, outbuf, 1);
		ret = ilitek_check_busy(50, 50, ILITEK_TP_SYSTEM_BUSY);
    }
    inbuf[1] = 1;
    ret = ilitek_i2c_rw(inbuf, 2, 1, outbuf, 2);
    crc = outbuf[0]+(outbuf[1] << 8);
    return crc;
}

int api_protocol_set_mode(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	inbuf[0] = ILITEK_TP_CMD_SET_MODE_CONTORL;
	inbuf[2] = 0;
	ret = ilitek_i2c_rw(inbuf, 3, 100, outbuf, 0);
	return ret;
}

int api_protocol_set_fs_Info(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;
	ret = ilitek_i2c_rw(inbuf, 3, 10, outbuf, 0);
	return ret;
}

int api_set_fs_Info(int32_t Dump1, int32_t Dump2)
{
    uint8_t inbuf[3] = {0};
    inbuf[0] = ILITEK_TP_CMD_SET_FS_INFO;
    inbuf[1] = Dump1;
    inbuf[2] = Dump2;

    if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_FS_INFO, inbuf, NULL) < 0) {
        return ILITEK_FAIL;
    }
    return ILITEK_SUCCESS;
}

uint16_t api_get_block_crc(uint32_t start, uint32_t end, uint32_t type) {
	uint8_t inbuf[8] = {0}, outbuf[2] = {0};

	inbuf[1] = type;
	inbuf[2] = start;
	inbuf[3] = (start >> 8) & 0xFF;
	inbuf[4] = (start >> 16) & 0xFF;
	inbuf[5] = end & 0xFF;
	inbuf[6] = (end >> 8) & 0xFF;
	inbuf[7] = (end >> 16) & 0xFF;
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_BLOCK_CRC, inbuf, outbuf) < 0) {
		return ILITEK_FAIL;
	}
	return 	outbuf[0]+(outbuf[1] << 8);
}

int api_get_slave_mode(int number)
{
    int i = 0;
    uint8_t inbuf[64] = {0}, outbuf[64] = {0};

    inbuf[0] = (uint8_t)ILITEK_TP_CMD_READ_MODE;
	if(ilitek_i2c_rw(inbuf, 1, 5, outbuf, number*2) < 0)
		return ILITEK_FAIL;
    for(i = 0; i < number; i++){
        ilitek_data->ic[i].mode = outbuf[i*2];
        LOG_INFO("IC[%d] mode: 0x%x\n", i, ilitek_data->ic[i].mode);
    }
	return ILITEK_SUCCESS;
}

uint32_t api_get_ap_crc(int number)
{
    int i = 0;
    uint8_t inbuf[64] = {0}, outbuf[64] = {0};

    inbuf[0] = ILITEK_TP_GET_AP_CRC;
	if(ilitek_i2c_rw(inbuf, 1, 1, outbuf, number*2) < 0)
		return ILITEK_FAIL;
    for(i = 0; i < number; i++){
        ilitek_data->ic[i].crc = outbuf[i*2] + (outbuf[i*2+1] << 8);
        LOG_INFO("IC[%d] CRC: 0x%x\n", i, ilitek_data->ic[i].crc);
    }
    return ILITEK_SUCCESS;
}

int api_set_access_slave(int number)
{
    int ret=0;
    uint8_t inbuf[64] = {0};

    inbuf[0] = (uint8_t)ILITEK_TP_CMD_ACCESS_SLAVE;
    inbuf[1] = 0x3;
    inbuf[2] = 0xC3;
	ret = ilitek_i2c_rw(inbuf, 3, 1, NULL, 0);
	if (ilitek_check_busy(10, 100, ILITEK_TP_SYSTEM_BUSY) < 0)
	{
		LOG_INFO("%s, Last: CheckBusy Failed\n", __func__);
		return ILITEK_FAIL;
	}
    return ILITEK_SUCCESS;
}

int api_protocol_set_cdc_initial(uint8_t *inbuf, uint8_t *outbuf)
{
    int ret = ILITEK_SUCCESS;

	inbuf[0] = ILITEK_TP_CMD_SET_CDC_INITOAL_V6;
	ret = ilitek_i2c_rw(inbuf, 2, 10, outbuf, 0);
	return ret;
}
/* Private define ------------------------------------------------------------*/
PROTOCOL_MAP ptl_func_map_v3[] =
{
    {ILITEK_TP_CMD_GET_TP_RESOLUTION, api_protocol_get_tp_resolution,"ILITEK_TP_CMD_GET_TP_RESOLUTION"},
    {ILITEK_TP_CMD_GET_SCREEN_RESOLUTION, api_protocol_get_sc_resolution, "ILITEK_TP_CMD_GET_SCREEN_RESOLUTION"},

    {ILITEK_TP_CMD_GET_KEY_INFORMATION, api_protocol_get_v3_key_info, "ILITEK_TP_CMD_GET_KEY_INFORMATION"},

    {ILITEK_TP_CMD_GET_FIRMWARE_VERSION, api_protocol_get_fwver, "ILITEK_TP_CMD_GET_FIRMWARE_VERSION"},
    {ILITEK_TP_CMD_READ_MODE, api_protocol_check_mode, "ILITEK_TP_CMD_READ_MODE"},
    {ILITEK_TP_CMD_GET_PROTOCOL_VERSION, api_protocol_get_ptl, "ILITEK_TP_CMD_GET_PROTOCOL_VERSION"},
    {ILITEK_TP_CMD_GET_KERNEL_VERSION, api_protocol_get_mcuver, "ILITEK_TP_CMD_GET_KERNEL_VERSION"},
	{ILITEK_TP_CMD_SET_FUNC_MODE, api_protocol_control_funcmode, "ILITEK_TP_CMD_SET_FUNC_MODE"},
	{ILITEK_TP_CMD_GET_SYSTEM_BUSY, api_protocol_system_busy, "ILITEK_TP_CMD_GET_SYSTEM_BUSY"},
	{ILITEK_TP_CMD_SET_APMODE, api_protocol_set_apmode, "ILITEK_TP_CMD_SET_APMODE"},
	{ILITEK_TP_CMD_SET_BLMODE, api_protocol_set_blmode, "ILITEK_TP_CMD_SET_BLMODE"},

    {ILITEK_TP_CMD_WRITE_ENABLE, api_protocol_write_enable, "ILITEK_TP_CMD_WRITE_ENABLE"},
	{ILITEK_TP_CMD_CONTROL_TESTMODE, api_protocol_control_tsmode, "ILITEK_TP_CMD_CONTROL_TESTMODE"},

    {ILITEK_TP_CMD_GET_MODULE_NAME, api_protocol_get_module_name, "ILITEK_TP_CMD_GET_MODULE_NAME"},
};

PROTOCOL_MAP ptl_func_map_v6[] =
{
    {ILITEK_TP_CMD_GET_TP_RESOLUTION, api_protocol_get_v6_tp_resolution,"ILITEK_TP_CMD_GET_TP_RESOLUTION"},
    {ILITEK_TP_CMD_GET_SCREEN_RESOLUTION, api_protocol_get_sc_resolution, "ILITEK_TP_CMD_GET_SCREEN_RESOLUTION"},

    {ILITEK_TP_CMD_GET_KEY_INFORMATION, api_protocol_get_v6_key_info, "ILITEK_TP_CMD_GET_KEY_INFORMATION"},

    {ILITEK_TP_CMD_GET_FIRMWARE_VERSION, api_protocol_get_fwver, "ILITEK_TP_CMD_GET_FIRMWARE_VERSION"},
    {ILITEK_TP_CMD_READ_MODE, api_protocol_check_mode, "ILITEK_TP_CMD_READ_MODE"},
    {ILITEK_TP_CMD_GET_PROTOCOL_VERSION, api_protocol_get_ptl, "ILITEK_TP_CMD_GET_PROTOCOL_VERSION"},
    {ILITEK_TP_CMD_GET_KERNEL_VERSION, api_protocol_get_mcuver, "ILITEK_TP_CMD_GET_KERNEL_VERSION"},
	{ILITEK_TP_CMD_SET_FUNC_MODE, api_protocol_control_funcmode, "ILITEK_TP_CMD_SET_FUNC_MODE"},
	{ILITEK_TP_CMD_GET_SYSTEM_BUSY, api_protocol_system_busy, "ILITEK_TP_CMD_GET_SYSTEM_BUSY"},
	{ILITEK_TP_CMD_SET_APMODE, api_protocol_set_apmode, "ILITEK_TP_CMD_SET_APMODE"},
	{ILITEK_TP_CMD_SET_BLMODE, api_protocol_set_blmode, "ILITEK_TP_CMD_SET_BLMODE"},

    {ILITEK_TP_CMD_SET_MODE_CONTORL, api_protocol_set_mode, "ILITEK_TP_CMD_SET_MODE_CONTORL"},
    {ILITEK_TP_CMD_WRITE_FLASH_ENABLE, api_protocol_write_flash_enable, "ILITEK_TP_CMD_WRITE_FLASH_ENABLE"},
	{ILITEK_TP_CMD_GET_BLOCK_CRC, api_protocol_get_block_crc, "ILITEK_TP_CMD_GET_BLOCK_CRC"},
	{ILITEK_TP_CMD_SET_CDC_INITOAL_V6, api_protocol_set_cdc_initial, "ILITEK_TP_CMD_SET_CDC_INITOAL_V6"},
	{ILITEK_TP_CMD_SET_FS_INFO, api_protocol_set_fs_Info, "ILITEK_TP_CMD_SET_FS_INFO"},

    {ILITEK_TP_CMD_GET_MODULE_NAME, api_protocol_get_module_name, "ILITEK_TP_CMD_GET_MODULE_NAME"},
};
//-----------------------extern api function------------------------//
int api_protocol_set_cmd(uint16_t cmd, uint8_t *inbuf, uint8_t *outbuf)
{
    uint16_t cmd_id = 0;

    for(cmd_id = 0; cmd_id < ilitek_data->ptl.cmd_count; cmd_id++)
    {
        if(ptl_cb_func[cmd_id].cmd == cmd)
        {
			ptl_cb_func[cmd_id].func(inbuf, outbuf);
			LOG_INFO("set command:%s\n", ptl_cb_func[cmd_id].name);
            return cmd_id;
        }
    }
	LOG_INFO("command list not find, cmd:0x%x\n", cmd);
    return ILITEK_FAIL;
}

int api_protocol_set_testmode(bool testmode)
{
	uint8_t inbuf[3];
	if(ilitek_data->ptl.ver_major == 0x3) {
		if (testmode) {
			inbuf[1] = 0x01;	//inable test mode
		} else {
			inbuf[1] = 0x00;	//disable test mode
		}
		if (api_protocol_set_cmd(ILITEK_TP_CMD_CONTROL_TESTMODE, inbuf, NULL) < 0) {
			return ILITEK_FAIL;
		}
		ilitek_delay(10);
	} else if(ilitek_data->ptl.ver_major == 0x6) {
		if (testmode) {
			inbuf[1] = ENTER_SUSPEND_MODE;	//inable test mode
		} else {
			inbuf[1] = ENTER_NORMAL_MODE;	//disable test mode
		}
		inbuf[2] = 0;
		if (api_protocol_set_cmd(ILITEK_TP_CMD_SET_MODE_CONTORL, inbuf, NULL) < 0) {
			return ILITEK_FAIL;
		}
		ilitek_delay(100);
	}
	return ILITEK_SUCCESS;
}

int
api_protocol_init_func(void)
{
	int ret = ILITEK_SUCCESS;
	uint8_t outbuf[64] = {0};

	api_protocol_get_ptl(NULL, outbuf);

	if (ilitek_data->ptl.ver_major == 0x3 || ilitek_data->ptl.ver == BL_V1_6 || ilitek_data->ptl.ver == BL_V1_7)
    {
		ptl_cb_func = ptl_func_map_v3;
		ilitek_data->ptl.cmd_count = PROTOCOL_CMD_NUM_V3;
		LOG_INFO("command protocol: PROTOCOL_CMD_NUM_V3\n");
		ilitek_data->process_and_report = ilitek_read_data_and_report_3XX;
		LOG_INFO("report function: ilitek_read_data_and_report_3XX\n");
	}
    else
    if (ilitek_data->ptl.ver_major == 0x6 || ilitek_data->ptl.ver == BL_V1_8)
    {
		ptl_cb_func = ptl_func_map_v6;
		ilitek_data->ptl.cmd_count = PROTOCOL_CMD_NUM_V6;
		LOG_INFO("command protocol: PROTOCOL_CMD_NUM_V6\n");
		ilitek_data->process_and_report = ilitek_read_data_and_report_6XX;
		LOG_INFO("report function: ilitek_read_data_and_report_6XX\n");
	}
	else
    {
		LOG_INFO("Ilitek_Ver = %d\n", ilitek_data->ptl.ver);
		ptl_cb_func = ptl_func_map_v3;
		ilitek_data->ptl.cmd_count = PROTOCOL_CMD_NUM_V3;
		LOG_INFO("command protocol: PROTOCOL_CMD_NUM_V3\n");
		ilitek_data->process_and_report = ilitek_read_data_and_report_3XX;
		LOG_INFO("report function: ilitek_read_data_and_report_3XX\n");
	}

	return ret;
}

int api_protocol_get_funcmode(int mode)
{
	int ret = ILITEK_SUCCESS;
	uint8_t inbuf[2], outbuf[64] = {0};

	inbuf[0] = ILITEK_TP_CMD_SET_FUNC_MODE;
	ret = ilitek_i2c_rw(inbuf, 1, 10, outbuf, 3);
	LOG_INFO("mode:%d\n", outbuf[2]);
	if(mode != outbuf[2]) {
		LOG_INFO("Set mode fail, set:%d, read:%d\n", mode, outbuf[2]);
	}
	return ret;
}

int api_protocol_set_funcmode(int mode)
{
	int ret = ILITEK_SUCCESS, i = 0;
	uint8_t inbuf[4] = {ILITEK_TP_CMD_SET_FUNC_MODE, 0x55, 0xAA, mode}, outbuf[64] = {0};;
	LOG_INFO("ilitek_data->ptl.ver:0x%x\n", ilitek_data->ptl.ver);
	if(ilitek_data->ptl.ver >= 0x30400){
		ret = api_protocol_control_funcmode(inbuf, NULL);
		for(i = 0; i < 20; i++)
		{
			ilitek_delay(100);
			ret = api_protocol_system_busy(inbuf, outbuf);
			if(ret < ILITEK_SUCCESS)
				return ret;
			if(outbuf[0] == ILITEK_TP_SYSTEM_READY){
				LOG_INFO("system is ready\n");
				ret = api_protocol_get_funcmode(mode);
				return ret;
			}
		}
		LOG_INFO("system is busy\n");
	}
	else {
		LOG_INFO("It is protocol not support\n");
	}
	return ret;
}
