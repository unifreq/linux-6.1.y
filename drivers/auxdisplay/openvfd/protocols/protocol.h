#ifndef __PROTOCOLS__
#define __PROTOCOLS__

#include "../openvfd_drv.h"

#define MSB_FIRST		0
#define LSB_FIRST		1

enum protocol_types {
	PROTOCOL_TYPE_I2C,
	PROTOCOL_TYPE_SPI_3W,
	PROTOCOL_TYPE_SPI_4W,
};

struct protocol_interface {
	unsigned char (*read_cmd_data)(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length);
	unsigned char (*read_data)(unsigned char *data, unsigned short length);
	unsigned char (*read_byte)(unsigned char *bdata);
	unsigned char (*write_cmd_data)(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length);
	unsigned char (*write_data)(const unsigned char *data, unsigned short length);
	unsigned char (*write_byte)(unsigned char bdata);
	enum protocol_types protocol_type;
};

#endif
