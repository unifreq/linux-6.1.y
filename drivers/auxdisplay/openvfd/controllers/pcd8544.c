#include <linux/gpio.h>
#include "../protocols/i2c_hw.h"
#include "../protocols/i2c_sw.h"
#include "../protocols/spi_sw.h"
#include "pcd8544.h"
#include "gfx_mono_ctrl.h"

static unsigned char pcd8544_init(void);
static unsigned char pcd8544_set_display_type(struct vfd_display *display);
static void pcd8544_clear(void);
static void pcd8544_set_power(unsigned char state);
static void pcd8544_set_contrast(unsigned char value);
static unsigned char pcd8544_set_xy(unsigned short x, unsigned short y);
static void pcd8544_print_string(const unsigned char *buffer, const struct rect *rect);
static void pcd8544_write_ctrl_command_buf(const unsigned char *buf, unsigned int length);
static void pcd8544_write_ctrl_command(unsigned char cmd);
static void pcd8544_write_ctrl_data_buf(const unsigned char *buf, unsigned int length);
static void pcd8544_write_ctrl_data(unsigned char data);

static struct specific_gfx_mono_ctrl pcd8544_gfx_mono_ctrl = {
	.init = pcd8544_init,
	.set_display_type = pcd8544_set_display_type,
	.clear = pcd8544_clear,
	.set_power = pcd8544_set_power,
	.set_contrast = pcd8544_set_contrast,
	.set_xy = pcd8544_set_xy,
	.print_char = NULL,
	.print_string = pcd8544_print_string,
	.write_ctrl_command_buf = pcd8544_write_ctrl_command_buf,
	.write_ctrl_command = pcd8544_write_ctrl_command,
	.write_ctrl_data_buf = pcd8544_write_ctrl_data_buf,
	.write_ctrl_data = pcd8544_write_ctrl_data,
	.screen_view = NULL,
};

struct pcd8544_display {
	unsigned char columns				: 3;
	unsigned char banks				: 3;
	unsigned char offset				: 2;

	union {
		struct {
			unsigned char address		: 7;
			unsigned char not_i2c		: 1;
		} i2c;
		struct {
			unsigned char reserved1		: 7;
			unsigned char is_spi		: 1;
		} spi;
	};

	unsigned char flags_secs			: 1;
	unsigned char flags_invert			: 1;
	unsigned char flags_transpose			: 1;
	unsigned char flags_rotate			: 1;
	unsigned char flags_ext_vcc			: 1;
	unsigned char flags_alt_com_conf		: 1;
	unsigned char flags_low_freq			: 1;
	unsigned char reserved2				: 1;

	unsigned char controller;
};

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol;
static unsigned char columns = 80;
static unsigned char banks = 6;
static unsigned char col_offset = 2;
static const unsigned char ram_buffer_blank[504] = { 0 };
static int pin_rst = 0;
static int pin_dc = 0;
struct pcd8544_display pcd8544_display;

struct controller_interface *init_pcd8544(struct vfd_dev *_dev)
{
	dev = _dev;
	memcpy(&pcd8544_display, &dev->dtb_active.display, sizeof(pcd8544_display));
	columns = (pcd8544_display.columns + 1) * 16;
	banks = pcd8544_display.banks + 1;
	col_offset = pcd8544_display.offset << 1;
	return init_gfx_mono_ctrl(_dev, &pcd8544_gfx_mono_ctrl);
}

static void pcd8544_write_ctrl_buf(unsigned char dc, const unsigned char *buf, unsigned int length)
{
	if (pcd8544_display.spi.is_spi) {
		gpio_direction_output(pin_dc, dc ? 1 : 0);
		protocol->write_data(buf, length);
	} else {
		protocol->write_cmd_data(&dc, 1, buf, length);
	}
}

static void pcd8544_write_ctrl_command_buf(const unsigned char *buf, unsigned int length)
{
	pcd8544_write_ctrl_buf(0x00, buf, length);
}

static void pcd8544_write_ctrl_command(unsigned char cmd)
{
	pcd8544_write_ctrl_buf(0x00, &cmd, 1);
}

static void pcd8544_write_ctrl_data_buf(const unsigned char *buf, unsigned int length)
{
	pcd8544_write_ctrl_buf(0x40, buf, length);
}

static void pcd8544_write_ctrl_data(unsigned char data)
{
	pcd8544_write_ctrl_buf(0x40, &data, 1);
}

static void pcd8544_clear(void)
{
	unsigned char cmd_buf[] = { 0x40, 0x80, 0x08 };
	pcd8544_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
	pcd8544_write_ctrl_data_buf(ram_buffer_blank, sizeof(ram_buffer_blank));
	pcd8544_write_ctrl_command(0x0C | pcd8544_display.flags_invert);
}

static void pcd8544_set_power(unsigned char state)
{
	if (state)
		pcd8544_write_ctrl_command(0x0C | pcd8544_display.flags_invert); // Set display On
	else
		pcd8544_write_ctrl_command(0x08); // Set display OFF
}

static void pcd8544_set_contrast(unsigned char value)
{
	unsigned char cmd_buf[] = { 0x21, 0x80 | (value >> 1), 0x20 };
	pcd8544_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
}

static unsigned char pcd8544_set_xy(unsigned short x, unsigned short y)
{
	unsigned char ret = 0;
	x += col_offset;
	if (x < columns + col_offset || y < banks) {
		unsigned char cmd_buf[] = { 0x40 | (y & 0x7), 0x80 | (x & 0x7F) };
		pcd8544_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
		ret = 1;
	}

	return ret;
}

static void pcd8544_print_string(const unsigned char *buffer, const struct rect *rect)
{
	unsigned char i;
	for (i = 0; i < rect->height; i++) {
		pcd8544_set_xy(rect->x1, rect->y1 + i);
		pcd8544_write_ctrl_data_buf(buffer + (i * rect->width), rect->width);
	}
}

static unsigned char pcd8544_init(void)
{
	unsigned char cmd_buf[] = {
		0x21, // [00] Enable extended instruction set
		0xB1, // [01] Set LCD Vop (Contrast)
		0x06, // [02] Set Temp coefficent
		0x12, // [03] LCD bias mode

		0x20, // [04] Disable extended instruction set
		0x0C, // [05] Set display On
	};

	if (pcd8544_display.spi.is_spi) {
		if (dev->gpio0_pin.pin >= 0 && dev->gpio1_pin.pin >= 0) {
			protocol = init_sw_spi_3w(MSB_FIRST, dev->clk_pin, dev->dat_pin, dev->stb_pin, pcd8544_display.flags_low_freq ? SPI_DELAY_100KHz : SPI_DELAY_500KHz);
			if (protocol) {
				pin_rst = dev->gpio0_pin.pin;
				pin_dc = dev->gpio1_pin.pin;
				gpio_direction_output(pin_rst, 0);
				udelay(5);
				gpio_direction_output(pin_rst, 1);
			}
		} else {
			pr_dbg2("PCD8544 controller failed to intialize. Invalid RESET (%d) and/or DC (%d) pins\n", dev->gpio0_pin.pin, dev->gpio1_pin.pin);
		}
	} else {
		if (dev->hw_protocol.protocol == PROTOCOL_I2C)
			protocol = init_hw_i2c(pcd8544_display.i2c.address, dev->hw_protocol.device_id);
		else
			protocol = init_sw_i2c(pcd8544_display.i2c.address, MSB_FIRST, 1, dev->clk_pin, dev->dat_pin, pcd8544_display.flags_low_freq ? I2C_DELAY_100KHz : I2C_DELAY_500KHz, NULL);
	}
	if (!protocol)
		return 0;

	cmd_buf[01] = (dev->brightness * 36) + 1;				// [01] Contrast
	cmd_buf[05] |= pcd8544_display.flags_invert ? 0x01 : 0x00;		// [05] Set display inverted state
	pcd8544_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
	pcd8544_clear();

	return 1;
}

static unsigned char pcd8544_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->controller == CONTROLLER_PCD8544) {
		dev->dtb_active.display = *display;
		pcd8544_init();
		ret = 1;
	}

	return ret;
}
