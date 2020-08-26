/**
 *  \file ilitek_protocol_port.c
 *  \author MV - mvidojevic@gmail.com
 */

#ifndef _ILITEK_PORT_
#define _ILITEK_PORT_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 *  \brief Initialize interface to Ilitek controller.
 *
 *  \todo
 *  Implement this function for the particular platform/architecture and inside it :
 *  - Initialize I2C bus
 *  - Initialize RST pin as output
 *  - Initialize INT pin as input
 *  - (Optional) Initialize serial port for printout.
 */
extern void ilitek_interface_init ( void );

/**
 *  \brief Delay function
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Issue delay for provided `msec` number of milliseconds.
 */
extern void ilitek_delay ( uint32_t msec );

/**
 *  \brief Sleep function
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Issue system sleep for provided `msec` number of milliseconds.
 */
extern void ilitek_sleep ( uint32_t msec );

/**
 *  \brief I2C Read function
 *
 *  \param[out] data - pointer to buffer which will be populated with readout
 *  \param[in] read_len - size of the readout
 *  \return - 0 no error / 1 error occurs
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Send slave address of ILITEK controller with 'R' bit
 *      - Read array of `read_len` number bytes and put it into `data` buffer
 *      - Issue stop condition on the I2C bus
 */
extern int ilitek_i2c_read( uint8_t * data, int read_len );

/**
 *  \brief I2C RW function
 *
 *  \param[in] cmd - pointer to buffer which carries command to be send via I2C bus to the touch panel
 *  \param[in] write_len - length of the command
 *  \param[in] delay - time between command and readout in milliseconds
 *  \param[out] data - pointer to buffer which will be populated with readout
 *  \param[in] read_len - size of the readout
 *  \return - 0 no error / 1 error occurs
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Issue start condition on the I2C bus
 *      - Send slave address of ILITEK controller with 'W' bit
 *      - Send array of `write_len` number bytes from array pointer with `cmd`
 *      - Issue stop condition on the I2C bus
 *      - Make delay in amount of `delay` milliseconds
 *      - Send slave address of ILITEK controller with 'R' bit
 *      - Read array of `read_len` number bytes and put it into `data` buffer
 *      - Issue stop condition on the I2C bus
 */
extern int ilitek_i2c_rw( uint8_t * cmd, int write_len, int delay, uint8_t * data, int read_len );

/**
 *  \brief Set RST pin state
 *
 *  \param[in] value - state of the RST pin (0 - 1)
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Set appropriate state on the RST pin depending on `value` argument
 */
extern void ilitek_gpio_reset_set( uint8_t value );

/**
 *  \brief Get INT pin state
 *
 *  \return State of the INT pin
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Read the state of the INT pin and return it.
 */
extern uint8_t ilitek_gpio_int_get( void );

/**
 *  \brief Print function
 *
 *  \todo
 *  Implement this function for the particular platform/architecture to :
 *      - Print the string provided in same manner as `printf` doing.
 */
extern void ilitek_print ( const char * fmt, ... );

#ifdef __cplusplus
} // extern "C"
#endif
#endif
