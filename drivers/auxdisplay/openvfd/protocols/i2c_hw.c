
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/i2c.h>

#include "i2c_hw.h"

#define pr_dbg2(args...) printk(KERN_DEBUG "OpenVFD: " args)
#define LOW	0
#define HIGH	1

static unsigned char i2c_hw_read_cmd_data(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length);
static unsigned char i2c_hw_read_data(unsigned char *data, unsigned short length);
static unsigned char i2c_hw_read_byte(unsigned char *bdata);
static unsigned char i2c_hw_write_cmd_data(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length);
static unsigned char i2c_hw_write_data(const unsigned char *data, unsigned short length);
static unsigned char i2c_hw_write_byte(unsigned char bdata);

static struct protocol_interface i2c_hw_interface = {
	.read_cmd_data = i2c_hw_read_cmd_data,
	.read_data = i2c_hw_read_data,
	.read_byte = i2c_hw_read_byte,
	.write_cmd_data = i2c_hw_write_cmd_data,
	.write_data = i2c_hw_write_data,
	.write_byte = i2c_hw_write_byte,
	.protocol_type = PROTOCOL_TYPE_I2C
};

union address {
	struct {
		unsigned char low;
		unsigned char high;
	} nibbles;
	unsigned short value;
};

static union address i2c_address = { 0 };
static struct i2c_adapter *i2c;
static unsigned char use_address = 0;
static unsigned short long_address_flag = 0;

static unsigned char i2c_hw_test_connection(void)
{
	return i2c_hw_write_cmd_data(NULL, 0, NULL, 0);
}

struct protocol_interface *init_hw_i2c(unsigned short _address, unsigned char _device_id)
{
	struct protocol_interface *i2c_hw_ptr = NULL;
	i2c = i2c_get_adapter(_device_id);
	if (i2c) {
		pr_dbg2("Found I2C-%d adapter: %s\n", _device_id , i2c->name);
		if (_address) {
			use_address = 1;
			long_address_flag = _address > 0xFF ? I2C_M_TEN : 0;				// A valid 10-bit address always starts with b11110.
			i2c_address.value = _address == 0xFF ? 0x0000 : _address;	// General call.
		} else {
			use_address = 0;
		}
		if (!i2c_hw_test_connection()) {
			i2c_hw_ptr = &i2c_hw_interface;
			pr_dbg2("HW I2C interface intialized (address = 0x%04X%s)\n", i2c_address.value, !use_address ? " (N/A)" : "");
		} else {
			pr_dbg2("HW I2C interface failed to intialize. Could not establish communication with I2C slave\n");
		}
	} else {
		pr_dbg2("HW I2C interface failed to intialize. Could not get I2C-%d adapter\n", _device_id);
	}
	return i2c_hw_ptr;
}

static int i2c_hw_writereg(unsigned char *data, unsigned short size)
{
	int ret;
	struct i2c_msg msg[1] = {
			{
				.addr = i2c_address.value,
				.flags = long_address_flag,
				.buf = data,
				.len = size,
			}
	};

	ret = i2c_transfer(i2c, msg, 1);
	if (ret == 1) {
		ret = 0;
	} else {
		dev_warn(&i2c->dev, "i2c wr failed=%d", ret);
		ret = -EREMOTEIO;
	}

	return ret;
}

static int i2c_hw_readreg(unsigned char *data, unsigned short size)
{
	int ret;
	struct i2c_msg msg[1] = {
			{
					.addr = i2c_address.value,
					.flags = I2C_M_RD | long_address_flag,
					.buf = data,
					.len = size,
			}
	};

	ret = i2c_transfer(i2c, msg, 1);
	if (ret == 1) {
		ret = 0;
	} else {
		dev_warn(&i2c->dev, "i2c rd failed=%d", ret);
		ret = -EREMOTEIO;
	}

	return ret;
}

static unsigned char i2c_hw_read_cmd_data(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length)
{
	unsigned char status = 0;

	status = i2c_hw_readreg(data, data_length);
	return status;
}

static unsigned char i2c_hw_read_data(unsigned char *data, unsigned short length)
{
	return i2c_hw_read_cmd_data(NULL, 0, data, length);
}

static unsigned char i2c_hw_read_byte(unsigned char *bdata)
{
	return i2c_hw_read_cmd_data(NULL, 0, bdata, 1);
}

static unsigned char i2c_hw_write_cmd_data(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length)
{
	unsigned char status = 0;
	unsigned short total_length = max(cmd_length + data_length, 4);
	char *buf = kmalloc(total_length, GFP_KERNEL);

	if (cmd)
		memcpy(buf, cmd, cmd_length);

	if (data)
		memcpy(buf + cmd_length, data, data_length);

	status = i2c_hw_writereg(buf, cmd_length + data_length);
	kfree(buf);
	return status;
}

static unsigned char i2c_hw_write_data(const unsigned char *data, unsigned short length)
{
	return i2c_hw_write_cmd_data(NULL, 0, data, length);
}

static unsigned char i2c_hw_write_byte(unsigned char bdata)
{
	return i2c_hw_write_cmd_data(NULL, 0, &bdata, 1);
}
