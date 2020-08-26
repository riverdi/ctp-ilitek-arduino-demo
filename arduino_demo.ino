#include <Wire.h>
#include "ilitek_common.h"
#include "ilitek_protocol.h"

// ----------------------------------------- PORT

void ilitek_interface_init ( void )
{
    //  Initialize I2C bus.
    Wire.begin();

    //  Initialize RST.
    pinMode(2, OUTPUT);

    //  Initialize INT.
    pinMode(3, INPUT);

    //  (Optional) Initialize serial port for printout.
    Serial.begin(9600);
}

void ilitek_delay ( uint32_t msec )
{
    delay( msec );
}

void ilitek_sleep ( uint32_t msec )
{
    delay( msec );
}

int ilitek_i2c_read( uint8_t * data, int rdlen )
{
    int r;

    Wire.requestFrom(0x41,rdlen);
    for (r = 0; ((r < rdlen) && Wire.available()); ++r)
    {
        data[r] = Wire.read();
    }

    return 0;
}

int ilitek_i2c_rw( uint8_t * cmd, int wrlen, int del, uint8_t * data, int rdlen )
{
    int r,w;

    Wire.beginTransmission(0x41);
    for (w = 0; w < wrlen; ++w)
    {
        Wire.write(cmd[w]);
    }
    Wire.endTransmission();

    if (del > 0)
    {
        delay( del );
    }

    Wire.requestFrom(0x41,rdlen);
    for (r = 0; ((r < rdlen) && Wire.available()); ++r)
    {
        data[r] = Wire.read();
    }

    return 0;
}

void ilitek_gpio_reset_set( uint8_t value )
{
    digitalWrite(2, value);
}

uint8_t ilitek_gpio_int_get( void )
{
    return digitalRead(3);
}

void ilitek_print ( const char * fmt, ... )
{
    char str[1024];
    va_list arg;

    va_start( arg, fmt );
    vsprintf( str, fmt, arg );
    va_end( arg );

    Serial.print( str );
}

// ----------------------------------------- PROGRAM

void setup()
{
    ilitek_interface_init( );
    ilitek_reset( 1000 );

    api_protocol_init_func( );
}

void loop()
{
    if ( !ilitek_gpio_int_get() )
    {
        ilitek_read_data_and_report_3XX( );

#if 0
        int i;
        int x_coord;
        int y_coord;

        for (i = 0; i < ILITEK_SUPPORT_MAX_POINT; ++i)
        {
            if (ilitek_data->touch_flag[i] == 1)
            {
                x_coord = ilitek_data->tp[i].x;
                y_coord = ilitek_data->tp[i].y;

                Serial.print( x_coord );
                Serial.print( y_coord );
            }
        }
#endif

    }
}
