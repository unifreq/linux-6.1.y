#include <linux/gpio.h>
#include "../protocols/i2c_hw.h"
#include "../protocols/i2c_sw.h"
#include "../protocols/spi_sw.h"
#include "ssd1306.h"
#include "gfx_mono_ctrl.h"

static unsigned char sh1106_init(void);
static unsigned char ssd1306_init(void);
static unsigned char ssd1306_set_display_type(struct vfd_display *display);
static void sh1106_clear(void);
static void ssd1306_clear(void);
static void ssd1306_set_power(unsigned char state);
static void ssd1306_set_contrast(unsigned char value);
static unsigned char ssd1306_set_xy(unsigned short x, unsigned short y);
static void ssd1306_print_string(const unsigned char *buffer, const struct rect *rect);
static void ssd1306_write_ctrl_command_buf(const unsigned char *buf, unsigned int length);
static void ssd1306_write_ctrl_command(unsigned char cmd);
static void ssd1306_write_ctrl_data_buf(const unsigned char *buf, unsigned int length);
static void ssd1306_write_ctrl_data(unsigned char data);

static struct specific_gfx_mono_ctrl ssd1306_gfx_mono_ctrl = {
	.init = ssd1306_init,
	.set_display_type = ssd1306_set_display_type,
	.clear = ssd1306_clear,
	.set_power = ssd1306_set_power,
	.set_contrast = ssd1306_set_contrast,
	.set_xy = ssd1306_set_xy,
	.print_char = NULL,
	.print_string = ssd1306_print_string,
	.write_ctrl_command_buf = ssd1306_write_ctrl_command_buf,
	.write_ctrl_command = ssd1306_write_ctrl_command,
	.write_ctrl_data_buf = ssd1306_write_ctrl_data_buf,
	.write_ctrl_data = ssd1306_write_ctrl_data,
	.screen_view = NULL,
};

struct ssd1306_display {
	unsigned char columns				: 3;
	unsigned char banks				: 3;
	unsigned char offset				: 2;

	union {
		struct {
			unsigned char address		: 7;
			unsigned char not_i2c		: 1;
		} i2c;
		struct {
			unsigned char is_4w		: 1;
			unsigned char reserved1		: 6;
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
static unsigned char columns = 128;
static unsigned char rows = 32;
static unsigned char banks = 32 / 8;
static unsigned char col_offset = 0;
static const unsigned char ram_buffer_blank[1024] = { 0 };
static int pin_rst = 0;
static int pin_dc = 0;
static struct ssd1306_display ssd1306_display;

static void (*clear)(void);

struct controller_interface *init_ssd1306(struct vfd_dev *_dev)
{
	dev = _dev;
	memcpy(&ssd1306_display, &dev->dtb_active.display, sizeof(ssd1306_display));
	columns = (ssd1306_display.columns + 1) * 16;
	banks = ssd1306_display.banks + 1;
	rows = banks * 8;
	col_offset = ssd1306_display.offset << 1;
	switch (ssd1306_display.controller) {
	case CONTROLLER_SH1106:
		clear = sh1106_clear;
		ssd1306_gfx_mono_ctrl.init = sh1106_init;
		break;
	case CONTROLLER_SSD1306:
	default:
		clear = ssd1306_clear;
		ssd1306_gfx_mono_ctrl.init = ssd1306_init;
		break;
	}
	ssd1306_gfx_mono_ctrl.clear = clear;
	return init_gfx_mono_ctrl(_dev, &ssd1306_gfx_mono_ctrl);
}

static void ssd1306_write_ctrl_buf(unsigned char dc, const unsigned char *buf, unsigned int length)
{
	if (ssd1306_display.spi.is_spi && ssd1306_display.spi.is_4w) {
		gpio_direction_output(pin_dc, dc ? 1 : 0);
		protocol->write_data(buf, length);
	} else {
		protocol->write_cmd_data(&dc, 1, buf, length);
	}
}

static void ssd1306_write_ctrl_command_buf(const unsigned char *buf, unsigned int length)
{
	ssd1306_write_ctrl_buf(0x00, buf, length);
}

static void ssd1306_write_ctrl_command(unsigned char cmd)
{
	ssd1306_write_ctrl_buf(0x00, &cmd, 1);
}

static void ssd1306_write_ctrl_data_buf(const unsigned char *buf, unsigned int length)
{
	ssd1306_write_ctrl_buf(0x40, buf, length);
}

static void ssd1306_write_ctrl_data(unsigned char data)
{
	ssd1306_write_ctrl_buf(0x40, &data, 1);
}

static void ssd1306_clear(void)
{
	unsigned char cmd_buf[] = { 0x21, col_offset, col_offset + columns - 1, 0x22, 0x00, banks - 1, 0xAE };
	ssd1306_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
	ssd1306_write_ctrl_data_buf(ram_buffer_blank, min((size_t)(columns * banks), sizeof(ram_buffer_blank)));
	ssd1306_write_ctrl_command(0xAF);
}

static void sh1106_clear(void)
{
	unsigned char i;
	ssd1306_write_ctrl_command(0xAE);
	for (i = 0; i < banks; i++) {
		ssd1306_set_xy(0, i);
		ssd1306_write_ctrl_data_buf(ram_buffer_blank, columns);
	}
	ssd1306_write_ctrl_command(0xAF);
}

static void ssd1306_set_power(unsigned char state)
{
	if (state)
		ssd1306_write_ctrl_command(0xAF); // Set display On
	else
		ssd1306_write_ctrl_command(0xAE); // Set display OFF
}

static void ssd1306_set_contrast(unsigned char value)
{
	unsigned char cmd_buf[] = { 0x81, ++value };
	ssd1306_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
}

static unsigned char ssd1306_set_xy(unsigned short x, unsigned short y)
{
	unsigned char ret = 0;
	x += col_offset;
	if (x < columns + col_offset || y < banks) {
		unsigned char cmd_buf[] = { 0xB0 | (y & 0xF), x & 0xF, 0x10 | (x >> 4) };
		ssd1306_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
		ret = 1;
	}

	return ret;
}

static void ssd1306_print_string(const unsigned char *buffer, const struct rect *rect)
{
	unsigned char i;
	if (ssd1306_display.controller == CONTROLLER_SH1106) {
		for (i = 0; i < rect->height; i++) {
			ssd1306_set_xy(rect->x1, rect->y1 + i);
			ssd1306_write_ctrl_data_buf(buffer + (i * rect->width), rect->width);
		}
	} else {
		unsigned char cmd_set_addr_range[] = { 0x21, rect->x1 + col_offset, rect->x2 + col_offset, 0x22, rect->y1, rect->y2 };
		unsigned char cmd_reset_addr_range[] = { 0x21, col_offset, col_offset + columns - 1, 0x22, 0x00, banks - 1 };
		ssd1306_write_ctrl_command_buf(cmd_set_addr_range, sizeof(cmd_set_addr_range));
		ssd1306_write_ctrl_data_buf(buffer, rect->length * rect->font->font_char_size);
		ssd1306_write_ctrl_command_buf(cmd_reset_addr_range, sizeof(cmd_reset_addr_range));
	}
}

static void init_protocol(void)
{
	if (ssd1306_display.spi.is_spi) {
		if (dev->gpio0_pin.pin >= 0 && (!ssd1306_display.spi.is_4w || dev->gpio1_pin.pin >= 0)) {
			protocol = init_sw_spi_3w(MSB_FIRST, dev->clk_pin, dev->dat_pin, dev->stb_pin, ssd1306_display.flags_low_freq ? SPI_DELAY_100KHz : SPI_DELAY_500KHz);
			if (protocol) {
				pin_rst = dev->gpio0_pin.pin;
				pin_dc = dev->gpio1_pin.pin;
				gpio_direction_output(pin_rst, 0);
				udelay(5);
				gpio_direction_output(pin_rst, 1);
			}
		} else {
			pr_dbg2("SSD1306 controller failed to intialize. Invalid RESET (%d) and/or DC (%d) pins\n", dev->gpio0_pin.pin, dev->gpio1_pin.pin);
		}
	} else {
		if (dev->hw_protocol.protocol == PROTOCOL_I2C)
			protocol = init_hw_i2c(ssd1306_display.i2c.address, dev->hw_protocol.device_id);
		else
			protocol = init_sw_i2c(ssd1306_display.i2c.address, MSB_FIRST, 1, dev->clk_pin, dev->dat_pin, ssd1306_display.flags_low_freq ? I2C_DELAY_100KHz : I2C_DELAY_500KHz, NULL);
	}
}

static unsigned char sh1106_init(void)
{
	unsigned char cmd_buf[] = {
		0xAE, // [00] Set display OFF

		0xA0, // [01] Set Segment Re-Map

		0xDA, // [02] Set COM Hardware Configuration
		0x02, // [03] COM Hardware Configuration

		0xC0, // [04] Set Com Output Scan Direction

		0xA8, // [05] Set Multiplex Ratio
		0x3F, // [06] Multiplex Ratio for 128x64 (64-1)

		0xD5, // [07] Set Display Clock Divide Ratio / OSC Frequency
		0x80, // [08] Display Clock Divide Ratio / OSC Frequency

		0xDB, // [09] Set VCOMH Deselect Level
		0x35, // [10] VCOMH Deselect Level

		0x81, // [11] Set Contrast
		0x8F, // [12] Contrast

		0x30, // [13] Set Vpp

		0xAD, // [14] Set DC-DC
		0x8A, // [15] DC-DC ON/OFF

		0x40, // [16] Set Display Start Line

		0xA4, // [17] Set all pixels OFF
		0xA6, // [18] Set display not inverted
		0xAF, // [19] Set display On
	};

	init_protocol();
	if (!protocol)
		return 0;

	cmd_buf[1] |= ssd1306_display.flags_rotate ? 0x01 : 0x00;		// [01] Set Segment Re-Map
	cmd_buf[3] |= ssd1306_display.flags_alt_com_conf ? 0x10 : 0x00;		// [03] COM Hardware Configuration
	cmd_buf[4] |= ssd1306_display.flags_rotate ? 0x08 : 0x00;		// [04] Set Com Output Scan Direction
	cmd_buf[6] = max(min(rows-1, 63), 15);					// [06] Multiplex Ratio for 128 x rows (rows-1)
	cmd_buf[12] = (dev->brightness * 36) + 1;				// [12] Contrast
	cmd_buf[15] |= ssd1306_display.flags_ext_vcc ? 0x00 : 0x01;		// [15] DC-DC ON/OFF
	cmd_buf[18] |= ssd1306_display.flags_invert ? 0x01 : 0x00;		// [18] Set display not inverted
	ssd1306_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
	clear();

	return 1;
}

static unsigned char ssd1306_init(void)
{
	unsigned char cmd_buf[] = {
		0xAE, // [00] Set display OFF

		0xD5, // [01] Set Display Clock Divide Ratio / OSC Frequency
		0x80, // [02] Display Clock Divide Ratio / OSC Frequency

		0xA8, // [03] Set Multiplex Ratio
		0x3F, // [04] Multiplex Ratio for 128x64 (64-1)

		0xD3, // [05] Set Display Offset
		0x00, // [06] Display Offset

		0x8D, // [07] Set Charge Pump
		0x14, // [08] Charge Pump (0x10 External, 0x14 Internal DC/DC)

		0xA0, // [09] Set Segment Re-Map
		0xC0, // [10] Set Com Output Scan Direction

		0xDA, // [11] Set COM Hardware Configuration
		0x02, // [12] COM Hardware Configuration

		0x81, // [13] Set Contrast
		0x8F, // [14] Contrast

		0xD9, // [15] Set Pre-Charge Period
		0xF1, // [16] Set Pre-Charge Period (0x22 External, 0xF1 Internal)

		0xDB, // [17] Set VCOMH Deselect Level
		0x30, // [18] VCOMH Deselect Level

		0x40, // [19] Set Display Start Line

		0x20, // [20] Set Memory Addressing Mode
		0x00, // [21] Set Horizontal Mode

		0x21, // [22] Set Column Address
		0x00, // [23] First column
		0x7F, // [24] Last column

		0x22, // [25] Set Page Address
		0x00, // [26] First page
		0x07, // [27] Last page

		0xA4, // [28] Set all pixels OFF
		0xA6, // [29] Set display not inverted
		0xAF, // [30] Set display On
	};

	init_protocol();
	if (!protocol)
		return 0;

	cmd_buf[4] = max(min(rows-1, 63), 15);					// [04] Multiplex Ratio for 128 x rows (rows-1)
	cmd_buf[8] = ssd1306_display.flags_ext_vcc ? 0x10 : 0x14;		// [08] Charge Pump (0x10 External, 0x14 Internal DC/DC)
	cmd_buf[9] |= ssd1306_display.flags_rotate ? 0x01 : 0x00;		// [09] Set Segment Re-Map
	cmd_buf[10] |= ssd1306_display.flags_rotate ? 0x08 : 0x00;		// [10] Set Com Output Scan Direction
	cmd_buf[12] |= ssd1306_display.flags_alt_com_conf ? 0x10 : 0x00;	// [12] COM Hardware Configuration
	cmd_buf[14] = (dev->brightness * 36) + 1;				// [14] Contrast
	cmd_buf[16] = ssd1306_display.flags_ext_vcc ? 0x22 : 0xF1;		// [16] Set Pre-Charge Period (0x22 External, 0xF1 Internal)
	cmd_buf[23] = col_offset;						// [23] First column
	cmd_buf[24] = col_offset + columns - 1;					// [24] Last column
	cmd_buf[27] = banks - 1;						// [27] Last page
	cmd_buf[29] |= ssd1306_display.flags_invert ? 0x01 : 0x00;		// [29] Set display not inverted
	ssd1306_write_ctrl_command_buf(cmd_buf, sizeof(cmd_buf));
	clear();

	return 1;
}

static unsigned char ssd1306_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->controller == CONTROLLER_SSD1306 || display->controller == CONTROLLER_SH1106) {
		dev->dtb_active.display = *display;
		ssd1306_init();
		ret = 1;
	}

	return ret;
}
