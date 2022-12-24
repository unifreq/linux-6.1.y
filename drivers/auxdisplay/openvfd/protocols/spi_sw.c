#include <linux/gpio.h>
#include "spi_sw.h"

#define pr_dbg2(args...) printk(KERN_DEBUG "OpenVFD: " args)
#define LOW	0
#define HIGH	1

static unsigned char spi_sw_read_cmd_data(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length);
static unsigned char spi_sw_read_data(unsigned char *data, unsigned short length);
static unsigned char spi_sw_read_byte(unsigned char *bdata);
static unsigned char spi_sw_write_cmd_data(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length);
static unsigned char spi_sw_write_data(const unsigned char *data, unsigned short length);
static unsigned char spi_sw_write_byte(unsigned char bdata);

static struct protocol_interface spi_sw_interface = {
	.read_cmd_data = spi_sw_read_cmd_data,
	.read_data = spi_sw_read_data,
	.read_byte = spi_sw_read_byte,
	.write_cmd_data = spi_sw_write_cmd_data,
	.write_data = spi_sw_write_data,
	.write_byte = spi_sw_write_byte,
	.protocol_type = PROTOCOL_TYPE_SPI_3W
};

static void spi_sw_stop_condition(void);

static unsigned long spi_sw_delay = SPI_DELAY_100KHz;
static unsigned char lsb_first = 0;
static int pin_clk = 0;
static int pin_do  = 0;
static int pin_stb = 0;
static int pin_di  = 0;

static struct protocol_interface *init_sw_spi(unsigned char _lsb_first, struct vfd_pin clk, struct vfd_pin dout, struct vfd_pin stb, const struct vfd_pin *din, unsigned long _spi_sw_delay)
{
	struct protocol_interface *spi_sw_ptr = NULL;
	if (clk.pin >= 0 && dout.pin >= 0 && stb.pin >= 0) {
		pin_clk = clk.pin;
		pin_do = dout.pin;
		pin_stb = stb.pin;
		lsb_first = _lsb_first;
		spi_sw_delay = _spi_sw_delay;
		if (!din) {
			pin_di = pin_do;
			spi_sw_interface.protocol_type = PROTOCOL_TYPE_SPI_3W;
			spi_sw_ptr = &spi_sw_interface;
		} else if (din->pin >= 0) {
			pin_di = din->pin;
			spi_sw_interface.protocol_type = PROTOCOL_TYPE_SPI_4W;
			spi_sw_ptr = &spi_sw_interface;
		}
		if (spi_sw_ptr)
			spi_sw_stop_condition();
	}
	return spi_sw_ptr;
}

struct protocol_interface *init_sw_spi_3w(unsigned char _lsb_first, struct vfd_pin clk, struct vfd_pin dat, struct vfd_pin stb, unsigned long _spi_sw_delay)
{
	struct protocol_interface *spi_sw_3w_ptr = init_sw_spi(_lsb_first, clk, dat, stb, NULL, _spi_sw_delay);
	if (spi_sw_3w_ptr)
		pr_dbg2("SW SPI 3-wire interface intialized (%s mode)\n", lsb_first ? "LSB" : "MSB");
	else
		pr_dbg2("SW SPI 3-wire interface failed to intialize. Invalid CLK (%d), DAT (%d) or STB (%d) pins\n", clk.pin, dat.pin, stb.pin);
	return spi_sw_3w_ptr;
}

struct protocol_interface *init_sw_spi_4w(unsigned char _lsb_first, struct vfd_pin clk, struct vfd_pin dout, struct vfd_pin din, struct vfd_pin stb, unsigned long _spi_sw_delay)
{
	struct protocol_interface *spi_sw_4w_ptr = init_sw_spi(_lsb_first, clk, dout, stb, &din, _spi_sw_delay);
	if (spi_sw_4w_ptr)
		pr_dbg2("SW SPI 4-wire interface intialized (%s mode)\n", lsb_first ? "LSB" : "MSB");
	else
		pr_dbg2("SW SPI 4-wire interface failed to intialize. Invalid CLK (%d), DOUT (%d), DIN (%d) or STB (%d) pins\n", clk.pin, dout.pin, din.pin, stb.pin);
	return spi_sw_4w_ptr;
}

static void spi_sw_start_condition(void)
{
	gpio_direction_output(pin_stb, LOW);
	udelay(spi_sw_delay);
}

static void spi_sw_stop_condition(void)
{
	gpio_direction_output(pin_clk, HIGH);
	udelay(spi_sw_delay);
	gpio_direction_output(pin_stb, HIGH);
	gpio_direction_output(pin_do, HIGH);
	gpio_direction_input(pin_do);
	udelay(spi_sw_delay);
}

static unsigned char spi_sw_write_raw_byte(unsigned char data)
{
	unsigned char i = 8;
	unsigned char mask = lsb_first ? 0x01 : 0x80;
	while (i--) {
		gpio_direction_output(pin_clk, LOW);
		udelay(spi_sw_delay);
		if (data & mask)
			gpio_direction_output(pin_do, HIGH);
		else
			gpio_direction_output(pin_do, LOW);
		gpio_direction_output(pin_clk, HIGH);
		udelay(spi_sw_delay);
		if (lsb_first)
			data >>= 1;
		else
			data <<= 1;
	}
	return 0;
}

static unsigned char spi_sw_read_raw_byte(unsigned char *data)
{
	unsigned char i = 8;
	unsigned char mask = lsb_first ? 0x80 : 0x01;
	*data = 0;
	gpio_direction_input(pin_di);
	while (i--) {
		if (lsb_first)
			*data >>= 1;
		else
			*data <<= 1;
		gpio_direction_output(pin_clk, LOW);
		udelay(spi_sw_delay);
		gpio_direction_output(pin_clk, HIGH);
		udelay(spi_sw_delay);
		if (gpio_get_value(pin_di))
			*data |= mask;
	}
	return 0;
}

static unsigned char spi_sw_read_cmd_data(const unsigned char *cmd, unsigned short cmd_length, unsigned char *data, unsigned short data_length)
{
	unsigned char status = 0;
	spi_sw_start_condition();
	if (!status && cmd) {
		while (cmd_length--) {
			status |= spi_sw_write_raw_byte(*cmd);
			cmd++;
		}
	}
	if (!status) {
		while (data_length--) {
			status |= spi_sw_read_raw_byte(data);
			data++;
		}
	}
	spi_sw_stop_condition();
	return status;
}

static unsigned char spi_sw_read_data(unsigned char *data, unsigned short length)
{
	return spi_sw_read_cmd_data(NULL, 0, data, length);
}

static unsigned char spi_sw_read_byte(unsigned char *bdata)
{
	return spi_sw_read_cmd_data(NULL, 0, bdata, 1);
}

static unsigned char spi_sw_write_cmd_data(const unsigned char *cmd, unsigned short cmd_length, const unsigned char *data, unsigned short data_length)
{
	unsigned char status = 0;
	spi_sw_start_condition();
	if (!status && cmd) {
		while (cmd_length--) {
			status |= spi_sw_write_raw_byte(*cmd);
			cmd++;
		}
	}
	if (!status) {
		while (data_length--) {
			status |= spi_sw_write_raw_byte(*data);
			data++;
		}
	}
	spi_sw_stop_condition();
	return status;
}

static unsigned char spi_sw_write_data(const unsigned char *data, unsigned short length)
{
	return spi_sw_write_cmd_data(NULL, 0, data, length);
}

static unsigned char spi_sw_write_byte(unsigned char bdata)
{
	return spi_sw_write_cmd_data(NULL, 0, &bdata, 1);
}
