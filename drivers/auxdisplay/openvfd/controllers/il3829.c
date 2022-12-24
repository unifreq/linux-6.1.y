#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include "../protocols/i2c_hw.h"
#include "../protocols/i2c_sw.h"
#include "../protocols/spi_sw.h"
#include "il3829.h"
#include "gfx_mono_ctrl.h"

static unsigned char il3829_init(void);
static unsigned char il3829_set_display_type(struct vfd_display *display);
static void il3829_clear(void);
static void il3829_set_power(unsigned char state);
static void il3829_set_contrast(unsigned char value);
static unsigned char il3829_set_xy(unsigned short x, unsigned short y);
static void il3829_set_area(const struct rect *rect);
static void il3829_print_char(char ch, const struct font *font_struct, unsigned char x, unsigned char y);
static void il3829_print_string(const unsigned char *buffer, const struct rect *rect);
static void il3829_write_ctrl_command_data_buf(const unsigned char *buf, unsigned int length);
static void il3829_write_ctrl_command(unsigned char cmd);
static void il3829_write_ctrl_data_buf(const unsigned char *buf, unsigned int length);
static void il3829_write_ctrl_data(unsigned char data);

static struct screen_view screen_view = { 0 };

static struct specific_gfx_mono_ctrl il3829_gfx_mono_ctrl = {
	.init = il3829_init,
	.set_display_type = il3829_set_display_type,
	.clear = il3829_clear,
	.set_power = il3829_set_power,
	.set_contrast = il3829_set_contrast,
	.set_xy = il3829_set_xy,
	.print_char = il3829_print_char,
	.print_string = il3829_print_string,
	.write_ctrl_command_buf = il3829_write_ctrl_command_data_buf,
	.write_ctrl_command = il3829_write_ctrl_command,
	.write_ctrl_data_buf = il3829_write_ctrl_data_buf,
	.write_ctrl_data = il3829_write_ctrl_data,
	.screen_view = &screen_view,
};

enum {
	TYPE_IL3829,
	TYPE_IL3820,
};

struct il3829_display {
	unsigned char columns				: 3;
	unsigned char banks				: 3;
	unsigned char offset				: 2;

	union {
		struct {
			unsigned char address		: 7;
			unsigned char not_i2c		: 1;
		} i2c;
		struct {
			unsigned char disp_type		: 4;
			unsigned char reserved1		: 3;
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

struct write_list {
	struct rect rect;
	unsigned short buffer_length;
	unsigned char *buffer;
	struct list_head list;
};

static void il3829_init_display(unsigned char is_full_mode);

#define GxGDEP015OC1_POWER_DELAY 150
#define GxGDEP015OC1_PU_DELAY 325
#define GxGDEP015OC1_FU_DELAY 1300

const unsigned char LUTDefault_full[] =
{
  0x32,  // command
  0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69, 0x69, 0x59, 0x58, 0x99, 0x99,
  0x88, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00
};

const unsigned char LUTDefault_part[] =
{
  0x32,	// command
  0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol;
static unsigned short rows = 200;
static unsigned short banks = 200 / 8;
static unsigned char row_offset = 0;
static const unsigned char ram_buffer_blank[] = { [0 ... ((200 * 200 / 8) - 1)] = 0xFF };
static const unsigned char is_full_mode = 1;
static int pin_rst = -1;
static int pin_dc = -1;
static int pin_busy = -1;
static struct il3829_display il3829_display;
static struct write_list write_list;
static unsigned char power_state = 0;

static unsigned char is_needs_transpose(void)
{
	return il3829_display.spi.disp_type == TYPE_IL3820;
}

struct controller_interface *init_il3829(struct vfd_dev *_dev)
{
	dev = _dev;
	INIT_LIST_HEAD(&write_list.list);
	memcpy(&il3829_display, &dev->dtb_active.display, sizeof(il3829_display));
	switch (il3829_display.spi.disp_type) {
	case TYPE_IL3820:
		banks = 128 / 8;
		rows = 296;
		break;
	case TYPE_IL3829:
		banks = 200 / 8;
		rows = 200;
		break;
	}
	if (is_needs_transpose()) {
		screen_view.columns = rows / 8;
		screen_view.rows = banks * 8;
	} else {
		screen_view.columns = banks;
		screen_view.rows = rows;
	}
	screen_view.colomn_offset = 0;
	screen_view.swap_banks_orientation = 1;
	return init_gfx_mono_ctrl(_dev, &il3829_gfx_mono_ctrl);
}

static void il3829_write_ctrl_buf(unsigned char dc, const unsigned char *buf, unsigned int length)
{
	if (il3829_display.spi.is_spi) {
		gpio_direction_output(pin_dc, dc ? 1 : 0);
		protocol->write_data(buf, length);
	} else {
		protocol->write_cmd_data(&dc, 1, buf, length);
	}
}

static void il3829_write_ctrl_command(unsigned char cmd)
{
	il3829_write_ctrl_buf(0x00, &cmd, 1);
}

static void il3829_write_ctrl_command_data_buf(const unsigned char *buf, unsigned int length)
{
	length--;
	il3829_write_ctrl_command(*buf++);
	il3829_write_ctrl_buf(0x40, buf, length);
}

static void il3829_write_ctrl_data_buf(const unsigned char *buf, unsigned int length)
{
	il3829_write_ctrl_buf(0x40, buf, length);
}

static void il3829_write_ctrl_data(unsigned char data)
{
	il3829_write_ctrl_buf(0x40, &data, 1);
}

static void il3829_wait_busy(unsigned short max_delay)
{
	if (pin_busy >= 0) {
		unsigned short delay_count = 0;
		while (delay_count < max_delay && gpio_get_value(pin_busy)) {
			msleep(10);
			delay_count += 10;
		}
	} else {
		msleep(max_delay);
	}
}

static void il3829_full_update(void)
{
	il3829_write_ctrl_command(0x22);
	il3829_write_ctrl_data(0xC4);
	il3829_write_ctrl_command(0x20);
	il3829_write_ctrl_command(0xFF);
	il3829_wait_busy(GxGDEP015OC1_FU_DELAY);
}

static void il3829_part_update(void)
{
	il3829_write_ctrl_command(0x22);
	il3829_write_ctrl_data(0x04);
	il3829_write_ctrl_command(0x20);
	il3829_write_ctrl_command(0xFF);
	il3829_wait_busy(GxGDEP015OC1_PU_DELAY);
}

inline static void il3829_update(unsigned char is_full_mode)
{
	if (is_full_mode)
		il3829_full_update();
	else
		il3829_part_update();
}

struct task_struct *refresh_thread = NULL;

static int refresh_thread_loop(void *data)
{
	unsigned int prev_msecs, delay;
	while (!kthread_should_stop())
	{
		if (!mutex_trylock(dev->mutex)) {
			msleep(1);
			continue;
		}
		prev_msecs = jiffies_to_msecs(jiffies);
		if (power_state) {
			if (!list_empty(&write_list.list)) {
				struct write_list *item, *tmp;
				il3829_update(!is_full_mode);
				list_for_each_entry_safe(item, tmp, &write_list.list, list) {
					il3829_set_area(&item->rect);
					il3829_set_xy(item->rect.x1, item->rect.y1);
					il3829_write_ctrl_command(0x24);
					il3829_write_ctrl_data_buf(item->buffer, item->buffer_length);
					list_del(&item->list);
					kfree(item->buffer);
					kfree(item);
				}
			}
		}
		delay = min((unsigned int)500, (jiffies_to_msecs(jiffies) - prev_msecs));
		mutex_unlock(dev->mutex);
		if (!kthread_should_stop())
			msleep(500 - delay);
	}

	return 0;
}

static void stop_refresh_thread(void)
{
	if (refresh_thread)
	{
		kthread_stop(refresh_thread);
		refresh_thread = NULL;
	}
}

static void start_refresh_thread(void)
{
	if (!refresh_thread)
	{
		refresh_thread = kthread_create(refresh_thread_loop, NULL, "%s_e-ink_refresh_thread_loop", DEV_NAME);
		wake_up_process(refresh_thread);
	}
}

static void clear_write_list(void)
{
	struct write_list *item, *tmp;
	list_for_each_entry_safe(item, tmp, &write_list.list, list) {
		list_del(&item->list);
		kfree(item->buffer);
		kfree(item);
	}
}

static void clear(unsigned char is_full_mode)
{
	struct rect full_rect = {
		.x1 = 0x00, .x2 = banks - 1, .y1 = 0x00, .y2 = rows - 1
	};
	il3829_set_area(&full_rect);
	il3829_set_xy(0, 0);
	il3829_write_ctrl_command(0x24);
	il3829_write_ctrl_data_buf(ram_buffer_blank, sizeof(ram_buffer_blank));
	il3829_update(is_full_mode);
}

static void il3829_clear(void)
{
	clear_write_list();
	il3829_init_display(is_full_mode);
	clear(is_full_mode);
	clear(is_full_mode);
	il3829_init_display(!is_full_mode);
}

static void il3829_set_power(unsigned char state)
{
	power_state = state;
	if (state) {
		start_refresh_thread();
		il3829_init_display(!is_full_mode);
	} else {
		stop_refresh_thread();
		il3829_init_display(is_full_mode);
		il3829_update(is_full_mode);
		clear_write_list();
	}

	il3829_write_ctrl_command(0x22);
	il3829_write_ctrl_data(state ? 0xC0 : 0xC3);
	il3829_write_ctrl_command(0x20);
	il3829_wait_busy(GxGDEP015OC1_POWER_DELAY);
}

static void il3829_set_contrast(unsigned char value)
{
}

static unsigned char il3829_set_xy(unsigned short x, unsigned short y)
{
	unsigned char ret = 0;
	y += row_offset;
	if (x < banks || y < rows + row_offset) {
		unsigned char x_buf[] = { 0x4E, x };
		unsigned char y_buf[] = { 0x4F, (y & 0xFF), y >> 8 };
		il3829_write_ctrl_command_data_buf(x_buf, sizeof(x_buf));
		il3829_write_ctrl_command_data_buf(y_buf, sizeof(y_buf));
		ret = 1;
	}

	return ret;
}

static void il3829_set_area(const struct rect *rect)
{
	unsigned char x_buf[] = { 0x44, rect->x1, rect->x2 };
	unsigned char y_buf[] = { 0x45, rect->y1 & 0xFF, rect->y1 >> 8, rect->y2 & 0xFF, rect->y2 >> 8 };
	il3829_write_ctrl_command_data_buf(x_buf, sizeof(x_buf));
	il3829_write_ctrl_command_data_buf(y_buf, sizeof(y_buf));
}

static void il3829_print_char(char ch, const struct font *font_struct, unsigned char x, unsigned char y)
{
	unsigned short offset = 0;
	struct rect rect = {
		.x1 = x, .x2 = x + font_struct->font_width - 1, .y1 = y, .y2 = y + font_struct->font_height - 1,
		.font = font_struct, .length = 1, .width = font_struct->font_width, .height = font_struct->font_height,
	};
	if (is_needs_transpose())
		swap(x, y);
	if (x >= banks || y >= rows || ch < font_struct->font_offset || ch >= font_struct->font_offset + font_struct->font_char_count)
		return;

	ch -= font_struct->font_offset;
	offset = ch * font_struct->font_char_size;
	offset += 4;
	il3829_print_string(&font_struct->font_bitmaps[offset], &rect);
}

static inline void il3829_adjust_buffer(const struct write_list *item)
{
	unsigned short i;
	for (i = 0; i < item->buffer_length; i++)
		item->buffer[i] = ~item->buffer[i];
}

static void transpose_rect(struct rect *rect)
{
	struct rect tmp = *rect;
	rect->x1 = (tmp.y1 / 8);
	rect->x2 = (tmp.y2 / 8);
	rect->y1 = rows - 8 - (tmp.x2 * 8);
	rect->y2 = rows - (tmp.x1 * 8);
	rect->width = tmp.height / 8;
	rect->height = tmp.width * 8;
}

extern void transpose_buffer(unsigned char *buffer, const struct rect *rect);
static void il3829_print_string(const unsigned char *buffer, const struct rect *_rect)
{
	struct write_list *new_write;

	if (!power_state)
		return;

	new_write = kmalloc(sizeof(*new_write), GFP_KERNEL);
	if (new_write) {
		new_write->rect = *_rect;
		new_write->buffer_length = _rect->length * _rect->font->font_char_size;
		new_write->buffer = kmalloc(new_write->buffer_length, GFP_KERNEL);
		if (new_write->buffer) {
			list_add_tail(&new_write->list, &write_list.list);
			memcpy(new_write->buffer, buffer, new_write->buffer_length);
			il3829_adjust_buffer(new_write);

			if (is_needs_transpose()) {
				transpose_buffer(new_write->buffer, &new_write->rect);
				transpose_rect(&new_write->rect);
			}
			il3829_set_area(&new_write->rect);
			il3829_set_xy(new_write->rect.x1, new_write->rect.y1);
			il3829_write_ctrl_command(0x24);
			il3829_write_ctrl_data_buf(new_write->buffer, new_write->buffer_length);
		} else {
			kfree(new_write);
		}
	}
}

static void il3829_init_display(unsigned char is_full_mode)
{
	unsigned char gate_buf[] = {
		0x01, // [00] Gate setting
		0xC7, // [01] MUX [7:0] *x200 (200-1)
		0x00, // [02] MUX [8]
		0x00, // [03] Gate scan direction, order, output
	};
	unsigned char bssc_buf[] = {
		0x0C, // [04] Booster Soft start Control
		0xD7, // [05] Phase1
		0xD6, // [06] Phase2
		0x9D, // [07] Phase3
	};
	unsigned char vcom_buf[] = {
		0x2C, // [08] VCOM
		0x9B, // [09]
	};
	unsigned char dummy_buf[] = {
		0x3A, // [10] Set dummy line period
		0x1A, // [11] 4 dummy line per gate
	};
	unsigned char gate_line_buf[] = {
		0x3B, // [12] Set Gate line width
		0x08, // [13] 2us per line
	};
	unsigned char data_entry_buf[] = {
		0x11, // [1] Data Entry mode setting
		0x03, // [1] Y increment, X increment
	};

	if (il3829_display.spi.disp_type == TYPE_IL3820) {
		gate_buf[1] = 0x27;
		gate_buf[2] = 0x01;
	}
	il3829_write_ctrl_command_data_buf(gate_buf, sizeof(gate_buf));
	il3829_write_ctrl_command_data_buf(bssc_buf, sizeof(bssc_buf));
	il3829_write_ctrl_command_data_buf(vcom_buf, sizeof(vcom_buf));
	il3829_write_ctrl_command_data_buf(dummy_buf, sizeof(dummy_buf));
	il3829_write_ctrl_command_data_buf(gate_line_buf, sizeof(gate_line_buf));
	il3829_write_ctrl_command_data_buf(data_entry_buf, sizeof(data_entry_buf));
	if (is_full_mode)
		il3829_write_ctrl_command_data_buf(LUTDefault_full, sizeof(LUTDefault_full));
	else
		il3829_write_ctrl_command_data_buf(LUTDefault_part, sizeof(LUTDefault_part));
}

static unsigned char il3829_init(void)
{
	if (il3829_display.spi.is_spi) {
		if (dev->gpio1_pin.pin >= 0) {
			protocol = init_sw_spi_3w(MSB_FIRST, dev->clk_pin, dev->dat_pin, dev->stb_pin, il3829_display.flags_low_freq ? SPI_DELAY_100KHz : SPI_DELAY_500KHz);
			if (protocol) {
				pin_rst = dev->gpio0_pin.pin;
				pin_dc = dev->gpio1_pin.pin;
				pin_busy = dev->gpio2_pin.pin;
				if (pin_rst >= 0) {
					gpio_direction_output(pin_rst, 0);
					udelay(5);
					gpio_direction_output(pin_rst, 1);
				}
				if (pin_busy >= 0)
					gpio_direction_input(pin_busy);
			}
		} else {
			pr_dbg2("IL3829 controller failed to intialize. Invalid DC (%d) pin\n", dev->gpio1_pin.pin);
		}
	} else {
		if (dev->hw_protocol.protocol == PROTOCOL_I2C)
			protocol = init_hw_i2c(il3829_display.i2c.address, dev->hw_protocol.device_id);
		else
			protocol = init_sw_i2c(il3829_display.i2c.address, MSB_FIRST, 1, dev->clk_pin, dev->dat_pin, il3829_display.flags_low_freq ? I2C_DELAY_100KHz : I2C_DELAY_500KHz, NULL);
	}
	if (!protocol)
		return 0;

	il3829_write_ctrl_command(0x12);	// SW Reset.
	il3829_clear();
	il3829_wait_busy(GxGDEP015OC1_PU_DELAY);

	return 1;
}

static unsigned char il3829_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->controller == CONTROLLER_IL3829) {
		dev->dtb_active.display = *display;
		il3829_init();
		ret = 1;
	}

	return ret;
}
