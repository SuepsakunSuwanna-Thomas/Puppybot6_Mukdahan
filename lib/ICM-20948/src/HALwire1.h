#ifndef _ICM_HAL_H
#define _ICM_HAL_H

#include <Wire.h>

static int idd_io_hal_init_i2c(void)
{
    Wire1.setClock(1000000);
    return 0;
}

static int idd_io_hal_read_reg(uint8_t reg, uint8_t *rbuffer, uint32_t rlen)
{
    Wire1.beginTransmission(I2C_ADDR);
    Wire1.write(reg);
    Wire1.endTransmission(false);
    Wire1.requestFrom(I2C_ADDR, rlen);
    for (unsigned char i = 0; i < rlen; i++)
    {
        rbuffer[i] = Wire1.read();
    }
    return 0;
}

static int idd_io_hal_write_reg(uint8_t reg, const uint8_t *wbuffer, uint32_t wlen)
{
    Wire1.beginTransmission(I2C_ADDR);
    Wire1.write(reg);
    for (unsigned char i = 0; i < wlen; i++)
    {
        Wire1.write(wbuffer[i]);
    }
    Wire1.endTransmission(true);
    return 0;
}

void inv_icm20948_sleep_us(int us)
{
    delayMicroseconds(us);
}

uint64_t inv_icm20948_get_time_us(void)
{
    return micros();
}

#endif