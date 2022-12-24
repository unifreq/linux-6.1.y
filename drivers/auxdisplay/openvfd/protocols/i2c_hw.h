#ifndef __I2C_HW_H__
#define __I2C_HW_H__

#include "protocol.h"

struct protocol_interface *init_hw_i2c(unsigned short _address, unsigned char _device_id);

#endif
