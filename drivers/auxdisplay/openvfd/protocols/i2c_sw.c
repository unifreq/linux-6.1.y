#include <linux/gpio.h>
#include <linux/version.h>
#include "i2c_sw.h"

#define pr_dbg2(args...) printk(KERN_DEBUG "OpenVFD: " args)
#define LOW	0
#define HIGH	1

static unsigned char i2c_sw_read_cmd_data(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length);
static unsigned char i2c_sw_read_data(unsigned char *data, unsigned short length);
static unsigned char i2c_sw_read_byte(unsigned char *bdata);
static unsigned char i2c_sw_write_cmd_data(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length);
static unsigned char i2c_sw_write_data(const unsigned char *data, unsigned short length);
static unsigned char i2c_sw_write_byte(unsigned char bdata);

static struct protocol_interface i2c_sw_interface = {
	.read_cmd_data = i2c_sw_read_cmd_data,
	.read_data = i2c_sw_read_data,
	.read_byte = i2c_sw_read_byte,
	.write_cmd_data = i2c_sw_write_cmd_data,
	.write_data = i2c_sw_write_data,
	.write_byte = i2c_sw_write_byte,
	.protocol_type = PROTOCOL_TYPE_I2C
};

union address {
	struct {
		unsigned char low;
		unsigned char high;
	} nibbles;
	unsigned short value;
};

static void i2c_sw_stop_condition(void);

static union address i2c_sw_address = { 0 };
static unsigned char use_address = 0;
static unsigned char long_address = 0;
static unsigned long i2c_sw_delay = I2C_DELAY_100KHz;
static unsigned char lsb_first = 0;
static unsigned short clk_stretch_timeout = 0;
static struct vfd_pin pin_scl = { 0 };
static struct vfd_pin pin_sda = { 0 };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
inline void gpio_set_pullup(unsigned gpio, int value)
{
	gpio_direction_input(gpio);
	gpio_set_value(gpio, value);
}
#endif

static unsigned char i2c_sw_test_connection(const struct protocol_interface *protocol)
{
	return i2c_sw_write_cmd_data(NULL, 0, NULL, 0);
}

struct protocol_interface *init_sw_i2c(unsigned short _address, unsigned char _lsb_first, unsigned char _clock_stretch_support, struct vfd_pin _pin_scl, struct vfd_pin _pin_sda, unsigned long _i2c_sw_delay, unsigned char(*test_connection)(const struct protocol_interface *protocol))
{
	struct protocol_interface *i2c_sw_ptr = NULL;
	if (_pin_scl.pin >= 0 && _pin_sda.pin >= 0) {
		if (_address) {
			use_address = 1;
			long_address = _address > 0xFF;					// A valid 10-bit address always starts with b11110.
			i2c_sw_address.value = _address == 0xFF ? 0x0000 : _address;	// General call.
		} else {
			use_address = 0;
		}
		lsb_first = _lsb_first;
		pin_scl = _pin_scl;
		pin_sda = _pin_sda;
		i2c_sw_delay = _i2c_sw_delay;
		clk_stretch_timeout = _clock_stretch_support ? (10 * _i2c_sw_delay) : 0;
		if (_pin_scl.flags.bits.pullup_on)
			gpio_set_pullup(_pin_scl.pin, 1);
		if (_pin_sda.flags.bits.pullup_on)
			gpio_set_pullup(_pin_sda.pin, 1);
		i2c_sw_stop_condition();
		if (!test_connection)
			test_connection = i2c_sw_test_connection;
		if (!test_connection(&i2c_sw_interface)) {
			i2c_sw_ptr = &i2c_sw_interface;
			pr_dbg2("SW I2C interface intialized (address = 0x%04X%s, %s mode, pull-ups %s)\n", i2c_sw_address.value, !use_address ? " (N/A)" : "",
				lsb_first ? "LSB" : "MSB", _pin_scl.flags.bits.pullup_on ? "on" : "off" );
		} else {
			pr_dbg2("SW I2C interface failed to intialize. Could not establish communication with I2C slave\n");
		}
	} else {
		pr_dbg2("SW I2C interface failed to intialize. Invalid SCL (%d) and/or SDA (%d) pins\n", _pin_scl.pin, _pin_sda.pin);
	}
	return i2c_sw_ptr;
}

static inline void gpio_set_pin_low(const struct vfd_pin *pin)
{
	gpio_direction_output(pin->pin, LOW);
}

static inline void gpio_set_pin_high(const struct vfd_pin *pin)
{
	if (pin->flags.bits.kick_high)
		gpio_direction_output(pin->pin, HIGH);
	gpio_direction_input(pin->pin);
}

static void i2c_sw_start_condition(void)
{
	gpio_set_pin_low(&pin_sda);
	udelay(i2c_sw_delay);
	gpio_set_pin_low(&pin_scl);
	udelay(i2c_sw_delay);
}

static void i2c_sw_stop_condition(void)
{
	gpio_set_pin_high(&pin_scl);
	udelay(i2c_sw_delay);
	gpio_set_pin_high(&pin_sda);
	udelay(i2c_sw_delay);
	udelay(i2c_sw_delay);
}

static inline unsigned char i2c_sw_ack(void)
{
	unsigned char ret = 1, scl = 1;
	unsigned short timeout = clk_stretch_timeout;
	gpio_set_pin_low(&pin_scl);
	gpio_set_pin_high(&pin_sda);
	udelay(i2c_sw_delay);
	gpio_set_pin_high(&pin_scl);
	udelay(i2c_sw_delay);
	if (timeout) {
		do {
			scl = gpio_get_value(pin_scl.pin) ? 1 : 0;
			udelay(1);
		} while (!scl && timeout--);
		ret = gpio_get_value(pin_sda.pin) ? 1 : 0;
	} else {
		ret = 0;
	}
	gpio_set_pin_low(&pin_scl);
	gpio_set_pin_low(&pin_sda);
	udelay(i2c_sw_delay);
	return ret;
}

static unsigned char i2c_sw_write_raw_byte(unsigned char data)
{
	unsigned char i = 8;
	unsigned char mask = lsb_first ? 0x01 : 0x80;
	gpio_set_pin_low(&pin_scl);
	while (i--) {
		if (data & mask)
			gpio_set_pin_high(&pin_sda);
		else
			gpio_set_pin_low(&pin_sda);
		udelay(i2c_sw_delay);
		gpio_set_pin_high(&pin_scl);
		udelay(i2c_sw_delay);
		gpio_set_pin_low(&pin_scl);
		if (lsb_first)
			data >>= 1;
		else
			data <<= 1;
	}
	return i2c_sw_ack();
}

static unsigned char i2c_sw_read_raw_byte(unsigned char *data)
{
	unsigned char i = 8;
	unsigned char mask = lsb_first ? 0x80 : 0x01;
	*data = 0;
	gpio_set_pin_high(&pin_sda);
	while (i--) {
		if (lsb_first)
			*data >>= 1;
		else
			*data <<= 1;
		gpio_set_pin_high(&pin_scl);
		udelay(i2c_sw_delay);
		if (gpio_get_value(pin_sda.pin))
			*data |= mask;
		gpio_set_pin_low(&pin_scl);
		udelay(i2c_sw_delay);
	}
	return i2c_sw_ack();
}

static unsigned char i2c_sw_write_address(union address _address, unsigned char rw)
{
	unsigned char ret = 0;
	if (long_address) {
		_address.nibbles.high <<= 1;
		_address.nibbles.high |= rw ? 0x01 : 0x00;
		ret = i2c_sw_write_raw_byte(_address.nibbles.high);
		if (!ret)
			ret = i2c_sw_write_raw_byte(_address.nibbles.low);
	} else {
		_address.nibbles.low <<= 1;
		_address.nibbles.low |= rw ? 0x01 : 0x00;
		ret = i2c_sw_write_raw_byte(_address.nibbles.low);
	}
	return ret;
}

static unsigned char i2c_sw_read_cmd_data(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length)
{
	unsigned char status = 0;
	i2c_sw_start_condition();
	if (use_address)
		status = i2c_sw_write_address(i2c_sw_address, 1);
	if (cmd) {
		while (!status && cmd_length--)
			status |= i2c_sw_write_raw_byte(*cmd++);
	}
	while (!status && data_length--)
		status |= i2c_sw_read_raw_byte(data++);
	i2c_sw_stop_condition();
	return status;
}

static unsigned char i2c_sw_read_data(unsigned char *data, unsigned short length)
{
	return i2c_sw_read_cmd_data(NULL, 0, data, length);
}

static unsigned char i2c_sw_read_byte(unsigned char *bdata)
{
	return i2c_sw_read_cmd_data(NULL, 0, bdata, 1);
}

static unsigned char i2c_sw_write_cmd_data(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length)
{
	unsigned char status = 0;
	i2c_sw_start_condition();
	if (use_address)
		status = i2c_sw_write_address(i2c_sw_address, 0);
	if (cmd) {
		while (!status && cmd_length--)
			status |= i2c_sw_write_raw_byte(*cmd++);
	}
	while (!status && data_length--)
			status |= i2c_sw_write_raw_byte(*data++);
	i2c_sw_stop_condition();
	return status;
}

static unsigned char i2c_sw_write_data(const unsigned char *data, unsigned short length)
{
	return i2c_sw_write_cmd_data(NULL, 0, data, length);
}

static unsigned char i2c_sw_write_byte(unsigned char bdata)
{
	return i2c_sw_write_cmd_data(NULL, 0, &bdata, 1);
}
