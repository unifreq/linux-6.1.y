#ifndef __SPI_SW_H__
#define __SPI_SW_H__

#include "protocol.h"

#define SPI_DELAY_500KHz	1
#define SPI_DELAY_250KHz	2
#define SPI_DELAY_100KHz	5
#define SPI_DELAY_20KHz	25

struct protocol_interface *init_sw_spi_3w(unsigned char lsb_first, struct vfd_pin clk, struct vfd_pin dat, struct vfd_pin stb, unsigned long _spi_delay);
struct protocol_interface *init_sw_spi_4w(unsigned char lsb_first, struct vfd_pin clk, struct vfd_pin dout, struct vfd_pin din, struct vfd_pin stb, unsigned long _spi_delay);

#endif
