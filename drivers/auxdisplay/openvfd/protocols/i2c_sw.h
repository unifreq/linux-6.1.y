#ifndef __I2C_SW_H__
#define __I2C_SW_H__

#include "protocol.h"

#define I2C_DELAY_500KHz	1
#define I2C_DELAY_250KHz	2
#define I2C_DELAY_100KHz	5
#define I2C_DELAY_20KHz	25

struct protocol_interface *init_sw_i2c(unsigned short address, unsigned char lsb_first, unsigned char clock_stretch_support, struct vfd_pin pin_scl, struct vfd_pin pin_sda, unsigned long i2c_delay, unsigned char(*test_connection)(const struct protocol_interface *protocol));

#endif
