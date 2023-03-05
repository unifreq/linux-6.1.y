#include "gfx_mono_ctrl.h"
#include "fonts/Grotesk16x32_h.h"
#include "fonts/Grotesk16x32_v.h"
#include "fonts/Grotesk24x48_v.h"
#include "fonts/Grotesk32x64_h.h"
#include "fonts/Retro8x16_v.h"
#include "fonts/icons16x16_v.h"
#include "fonts/icons32x32_h.h"
#include "fonts/icons32x32_v.h"

static unsigned char gfx_mono_ctrl_init(void);
static unsigned short gfx_mono_ctrl_get_brightness_levels_count(void);
static unsigned short gfx_mono_ctrl_get_brightness_level(void);
static unsigned char gfx_mono_ctrl_set_brightness_level(unsigned short level);
static unsigned char gfx_mono_ctrl_get_power(void);
static void gfx_mono_ctrl_set_power(unsigned char state);
static void gfx_mono_ctrl_power_suspend(void) { gfx_mono_ctrl_set_power(0); }
static void gfx_mono_ctrl_power_resume(void) { gfx_mono_ctrl_init(); }
static struct vfd_display *gfx_mono_ctrl_get_display_type(void);
static unsigned char gfx_mono_ctrl_set_display_type(struct vfd_display *display);
static void gfx_mono_ctrl_set_icon(const char *name, unsigned char state);
static size_t gfx_mono_ctrl_read_data(unsigned char *data, size_t length);
static size_t gfx_mono_ctrl_write_data(const unsigned char *data, size_t length);
static size_t gfx_mono_ctrl_write_display_data(const struct vfd_display_data *data);

static struct controller_interface gfx_mono_ctrl_interface = {
	.init = gfx_mono_ctrl_init,
	.get_brightness_levels_count = gfx_mono_ctrl_get_brightness_levels_count,
	.get_brightness_level = gfx_mono_ctrl_get_brightness_level,
	.set_brightness_level = gfx_mono_ctrl_set_brightness_level,
	.get_power = gfx_mono_ctrl_get_power,
	.set_power = gfx_mono_ctrl_set_power,
	.power_suspend = gfx_mono_ctrl_power_suspend,
	.power_resume = gfx_mono_ctrl_power_resume,
	.get_display_type = gfx_mono_ctrl_get_display_type,
	.set_display_type = gfx_mono_ctrl_set_display_type,
	.set_icon = gfx_mono_ctrl_set_icon,
	.read_data = gfx_mono_ctrl_read_data,
	.write_data = gfx_mono_ctrl_write_data,
	.write_display_data = gfx_mono_ctrl_write_display_data,
};

#define MAX_INDICATORS	4

enum display_modes {
	DISPLAY_MODE_296x128,
	DISPLAY_MODE_200x200,
	DISPLAY_MODE_128x32,
	DISPLAY_MODE_96x32,
	DISPLAY_MODE_80x32,
	DISPLAY_MODE_80x48,
	DISPLAY_MODE_64x48,
	DISPLAY_MODE_128x64,
	DISPLAY_MODE_96x64,
	DISPLAY_MODE_80x64,
	DISPLAY_MODE_64x64,
};

enum indicator_icons {
	INDICATOR_ICON_NONE = 1,
	INDICATOR_ICON_CHANNEL,
	INDICATOR_ICON_TEMP,
	INDICATOR_ICON_CALENDAR,
	INDICATOR_ICON_MEDIA,
	INDICATOR_ICON_TV,
	INDICATOR_ICON_ETH,
	INDICATOR_ICON_WIFI,
	INDICATOR_ICON_PLAY,
	INDICATOR_ICON_PAUSE,
	INDICATOR_ICON_USB,
	INDICATOR_ICON_SD,
	INDICATOR_ICON_BT,
	INDICATOR_ICON_APPS,
	INDICATOR_ICON_SETUP,
};

struct gfx_mono_ctrl_display {
	unsigned char columns				: 3;
	unsigned char rows				: 3;
	unsigned char offset				: 2;

	unsigned char reserved1;

	unsigned char flags_secs			: 1;
	unsigned char reserved2				: 1;
	unsigned char flags_transpose			: 1;
	unsigned char reserved3				: 5;

	unsigned char controller;
};

struct indicators {
	unsigned int usb	: 1;
	unsigned int sd		: 1;
	unsigned int play	: 1;
	unsigned int pause	: 1;
	unsigned int eth	: 1;
	unsigned int wifi	: 1;
	unsigned int bt		: 1;
	unsigned int apps	: 1;
	unsigned int setup	: 1;
	unsigned int reserved	: 23;
};

static void setup_fonts(void);
static void init_font(struct font *font_struct, const unsigned char *font_bitmaps);
static void init_rect(struct rect *rect, const struct font *font, const char *str, unsigned char x, unsigned char y, unsigned char transposed);
static unsigned char print_icon(unsigned char ch);
static void print_indicator(unsigned char ch, unsigned char state, unsigned char index);
static void print_clock(const struct vfd_display_data *data, unsigned char print_seconds);
static void print_channel(const struct vfd_display_data *data);
static void print_playback_time(const struct vfd_display_data *data);
static void print_title(const struct vfd_display_data *data);
static void print_date(const struct vfd_display_data *data);
static void print_temperature(const struct vfd_display_data *data);
static void print_char(char ch, const struct font *font_struct, unsigned char x, unsigned char y);

static struct vfd_dev *dev = NULL;
static unsigned char columns = 128;
static unsigned char rows = 32 / 8;
static unsigned char col_offset = 0;
static unsigned char show_colon = 1;
static unsigned char show_icons = 1;
static unsigned char swap_banks_orientation = 0;
static enum display_modes display_mode = DISPLAY_MODE_128x32;
static unsigned char icon_x_offset = 0;
static unsigned char indicators_on_screen[MAX_INDICATORS] = { 0 };
static unsigned char ram_buffer[5000] = { 0 };
static struct vfd_display_data old_data;
static struct font font_text = { 0 };
static struct font font_icons = { 0 };
static struct font font_indicators = { 0 };
static struct font font_small_text = { 0 };
static struct indicators indicators = { 0 };
static struct gfx_mono_ctrl_display gfx_mono_ctrl_display;

static struct specific_gfx_mono_ctrl specific_gfx_mono_ctrl;

struct controller_interface *init_gfx_mono_ctrl(struct vfd_dev *_dev, const struct specific_gfx_mono_ctrl *_specific_gfx_mono_ctrl)
{
	dev = _dev;
	specific_gfx_mono_ctrl = *_specific_gfx_mono_ctrl;
	memcpy(&gfx_mono_ctrl_display, &dev->dtb_active.display, sizeof(gfx_mono_ctrl_display));
	if (specific_gfx_mono_ctrl.screen_view) {
		columns = specific_gfx_mono_ctrl.screen_view->columns;
		rows = specific_gfx_mono_ctrl.screen_view->rows;
		col_offset = specific_gfx_mono_ctrl.screen_view->colomn_offset;
		swap_banks_orientation = specific_gfx_mono_ctrl.screen_view->swap_banks_orientation;
	} else {
		columns = (gfx_mono_ctrl_display.columns + 1) * 16;
		rows = gfx_mono_ctrl_display.rows + 1;
		col_offset = gfx_mono_ctrl_display.offset << 1;
	}
	memset(&old_data, 0, sizeof(old_data));

	setup_fonts();
	if (specific_gfx_mono_ctrl.init)
		gfx_mono_ctrl_interface.init = specific_gfx_mono_ctrl.init;
	if (specific_gfx_mono_ctrl.set_display_type)
		gfx_mono_ctrl_interface.set_display_type = specific_gfx_mono_ctrl.set_display_type;
	if (!specific_gfx_mono_ctrl.print_char)
		specific_gfx_mono_ctrl.print_char = print_char;
	return &gfx_mono_ctrl_interface;
}

static void print_char(char ch, const struct font *font_struct, unsigned char x, unsigned char y)
{
	unsigned short offset = 0, i;
	if (x >= columns || y >= rows || ch < font_struct->font_offset || ch >= font_struct->font_offset + font_struct->font_char_count)
		return;

	ch -= font_struct->font_offset;
	offset = ch * font_struct->font_char_size;
	offset += 4;
	for (i = 0; i < font_struct->font_height; i++) {
		specific_gfx_mono_ctrl.set_xy(x, y + i);
		specific_gfx_mono_ctrl.write_ctrl_data_buf(&font_struct->font_bitmaps[offset], font_struct->font_width);
		offset += font_struct->font_width;
	}
}

extern void transpose8rS64(unsigned char* A, unsigned char* B);

#define GFX_MONO_CTRL_PRINT_DEBUG 0

#if GFX_MONO_CTRL_PRINT_DEBUG
unsigned char print_buffer_cnt = 30;
char txt_buf[204800] = { 0 };
static void print_buffer(unsigned char *buf, const struct rect *rect, unsigned char rotate)
{
	char *t = txt_buf;
	if (!print_buffer_cnt)
		return;
	print_buffer_cnt--;
	if (rotate)
		pr_dbg2("Transposed Buffer:\n");
	else
		pr_dbg2("Non-Transposed Buffer:\n");
	if (swap_banks_orientation) {
		unsigned char height = rotate ? rect->width : rect->height;
		unsigned char width = rotate ? rect->height : rect->width;
		for (int j = 0; j < height; j++) {
			t = txt_buf;
			for (int i = 0; i < width; i++) {
				for (int k = 0; k < 8; k++) {
					*t++ = (buf[j * width + i] & (0x80 >> k)) ? '#' : ' ';
				}
				*t++ = ' ';
			}
			*t++ = '\0';
			printk(KERN_DEBUG "%s\n", txt_buf);
		}
	} else {
		unsigned char height = rotate ? rect->height : rect->width / 8;
		unsigned char width = rotate ? rect->width : rect->height * 8;
		for (int j = 0; j < height; j++) {
			for (int k = 0; k < 8; k++) {
				t = txt_buf;
				for (int i = 0; i < width; i++) {
					if (i % 8 == 0)
						*t++ = ' ';
					*t++ = (buf[j * width + i] & (1 << k)) ? '#' : ' ';
				}
				*t++ = '\n';
				*t++ = '\0';
				printk(KERN_DEBUG "%s", txt_buf);
			}
			printk(KERN_DEBUG "\n");
		}
	}

	printk(KERN_DEBUG "\n\n");
}
#else
static void print_buffer(unsigned char *buf, const struct rect *rect, unsigned char rotate) { }
#endif

unsigned char t_buf[sizeof(ram_buffer)] = { 0 };
void transpose_buffer(unsigned char *buffer, const struct rect *rect)
{
	unsigned short i;
	if (!rect->width || !rect->height)
		return;

	print_buffer(buffer, rect, 0);
	if (swap_banks_orientation) {
		unsigned char tmp[8];
		unsigned short x, y;
		for (y = 0; y < rect->height; y += 8) {
			for (x = 0; x < rect->width; x++) {
				int s = x + y * rect->width;
				int d = 8 * (rect->width - 1 - x) * (rect->height / 8) + (y / 8);
				for (i = 0; i < 8; i++)
					tmp[i] = buffer[s + (i * rect->width)];
				transpose8rS64(tmp, tmp);
				for (i = 0; i < 8; i++)
					t_buf[d + (i * rect->height / 8)] = tmp[i];
			}
		}
	} else {
		for (i = 0; i < (rect->height * rect->width) / 8; i++) {
			int d = (rect->height - 1 - i % rect->height) * (rect->width / 8) + (i / rect->height);
			transpose8rS64(&buffer[i * 8], &t_buf[d * 8]);
		}
	}
	memcpy(buffer, t_buf, rect->height * rect->width);
	print_buffer(buffer, rect, 1);
}

static void print_string(const char *str, const struct font *font_struct, unsigned char x, unsigned char y)
{
	unsigned char ch = 0;
	unsigned short soffset = 0, doffset = 0, i, j, k;
	struct rect rect;
	unsigned char rect_width = 0, text_width = 0;
	init_rect(&rect, font_struct, str, x, y, gfx_mono_ctrl_display.flags_transpose);
	if (rect.length == 0)
		return;

	if (gfx_mono_ctrl_display.flags_transpose) {
		rect_width = rect.height * 8;
		text_width = rect.text_height;
	} else {
		rect_width = rect.width;
		text_width = rect.text_width;
	}
	for (k = 0; k < rect.length; k += text_width) {
		for (i = 0; i < font_struct->font_height; i++) {
			for (j = 0; j < text_width; j++) {
				doffset = k + j;
				ch = doffset < rect.length ? str[doffset] : ' ';
				if (ch < font_struct->font_offset || ch >= font_struct->font_offset + font_struct->font_char_count)
					ch = ' ';
				ch -= font_struct->font_offset;
				soffset = ch * font_struct->font_char_size + i * font_struct->font_width;
				soffset += 4;
				memcpy(&ram_buffer[i * rect_width + (k * font_struct->font_height + j) * font_struct->font_width], &font_struct->font_bitmaps[soffset], font_struct->font_width);
			}
		}
	}

	rect.length = rect.text_height * rect.text_width;
	if (gfx_mono_ctrl_display.flags_transpose)
		transpose_buffer(ram_buffer, &rect);
	else
		print_buffer(ram_buffer, &rect, 0);
	specific_gfx_mono_ctrl.print_string(ram_buffer, &rect);
}

static unsigned char prepare_and_print_string(const char *str, const struct font *font_struct, unsigned char x, unsigned char y)
{
	char buffer[512];
	struct rect rect;
	init_rect(&rect, font_struct, str, 0, y, 0);
	if (rect.length > 0) {
		if (rect.length < strlen(str)) {
			scnprintf(buffer, sizeof(buffer), "%s", str);
			buffer[rect.length - 1] = 0x7F; // 0x7F = position of ellipsis.
			buffer[rect.length] = '\0';
			print_string(buffer, font_struct, x, y);
		} else {
			print_string(str, font_struct, x, y);
		}
	}
	return rect.length;
}

static void setup_fonts(void)
{
	init_font(&font_indicators, icons16x16_V);
	init_font(&font_small_text, Retro8x16_V);
	switch (rows) {
	case 128:
		init_font(&font_text, Grotesk32x64_H);
		init_font(&font_small_text, Grotesk16x32_H);
		init_font(&font_icons, icons32x32_H);
		init_font(&font_indicators, icons32x32_H);
		display_mode = DISPLAY_MODE_296x128;
		break;
	case 200:
		init_font(&font_text, Grotesk32x64_H);
		init_font(&font_small_text, Grotesk16x32_H);
		init_font(&font_icons, icons32x32_H);
		init_font(&font_indicators, icons32x32_H);
		display_mode = DISPLAY_MODE_200x200;
		break;
	case 6:
		init_font(&font_text, Grotesk16x32_V);
		init_font(&font_icons, icons16x16_V);
		if (columns >= 80) {
			display_mode = DISPLAY_MODE_80x48;
		} else {
			show_colon = 0;
			display_mode = DISPLAY_MODE_64x48;
		}
		break;
	case 8:
		if (columns >= 96) {
			init_font(&font_text, Grotesk24x48_V);
			init_font(&font_icons, icons16x16_V);
			if (columns >= 120) {
				display_mode = DISPLAY_MODE_128x64;
			} else {
				show_colon = 0;
				display_mode = DISPLAY_MODE_96x64;
			}
		} else {
			init_font(&font_text, Grotesk16x32_V);
			init_font(&font_icons, icons32x32_V);
			if (columns >= 80) {
				display_mode = DISPLAY_MODE_80x64;
			} else {
				show_colon = 0;
				display_mode = DISPLAY_MODE_64x64;
			}
		}
		break;
	case 4:
	default:
		init_font(&font_text, Grotesk16x32_V);
		if (columns >= 120) {
			display_mode = DISPLAY_MODE_128x32;
			init_font(&font_icons, icons32x32_V);
		} else if (columns >= 96) {
			display_mode = DISPLAY_MODE_96x32;
			init_font(&font_icons, icons16x16_V);
		} else if (columns >= 80) {
			display_mode = DISPLAY_MODE_80x32;
			show_icons = 0;
		} else {
			show_colon = 0;
			show_icons = 0;
		}
		break;
	}
}

static unsigned char gfx_mono_ctrl_init(void)
{
	old_data.mode = DISPLAY_MODE_NONE;
	if (gfx_mono_ctrl_interface.init != gfx_mono_ctrl_init)
		return gfx_mono_ctrl_interface.init();
	return 0;
}

static unsigned short gfx_mono_ctrl_get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short gfx_mono_ctrl_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char gfx_mono_ctrl_set_brightness_level(unsigned short level)
{
	unsigned char tmp = dev->brightness = level & 0x7;
	dev->power = 1;
	specific_gfx_mono_ctrl.set_contrast(tmp * 36); // ruonds to 0 - 252.
	return 1;
}

static unsigned char gfx_mono_ctrl_get_power(void)
{
	return dev->power;
}

static void gfx_mono_ctrl_set_power(unsigned char state)
{
	specific_gfx_mono_ctrl.set_power(state);
	dev->power = state;
}

static struct vfd_display *gfx_mono_ctrl_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char gfx_mono_ctrl_set_display_type(struct vfd_display *display)
{
	pr_dbg2("gfx_mono_ctrl_set_display_type - not implemented\n");
	return 0;
}

static void gfx_mono_ctrl_set_icon(const char *name, unsigned char state)
{
	enum indicator_icons icon = INDICATOR_ICON_NONE;
	if (strncmp(name,"usb",3) == 0 && indicators.usb != state) {
		icon = INDICATOR_ICON_USB;
		indicators.usb = state;
	} else if (strncmp(name,"sd",2) == 0 && indicators.sd != state) {
		icon = INDICATOR_ICON_SD;
		indicators.sd = state;
	} else if (strncmp(name,"play",4) == 0 && indicators.play != state) {
		icon = INDICATOR_ICON_PLAY;
		indicators.play = state;
	} else if (strncmp(name,"pause",5) == 0 && indicators.pause != state) {
		icon = INDICATOR_ICON_PAUSE;
		indicators.pause = state;
	} else if (strncmp(name,"eth",3) == 0 && indicators.eth != state) {
		icon = INDICATOR_ICON_ETH;
		indicators.eth = state;
	} else if (strncmp(name,"wifi",4) == 0 && indicators.wifi != state) {
		icon = INDICATOR_ICON_WIFI;
		indicators.wifi = state;
	} else if (strncmp(name,"b-t",3) == 0 && indicators.bt != state) {
		icon = INDICATOR_ICON_BT;
		indicators.bt = state;
	} else if (strncmp(name,"apps",4) == 0 && indicators.apps != state) {
		icon = INDICATOR_ICON_APPS;
		indicators.apps = state;
	} else if (strncmp(name,"setup",5) == 0 && indicators.setup != state) {
		icon = INDICATOR_ICON_SETUP;
		indicators.setup = state;
	} else if (strncmp(name,"colon",5) == 0) {
		dev->status_led_mask = state ? (dev->status_led_mask | ledDots[LED_DOT_SEC]) : (dev->status_led_mask & ~ledDots[LED_DOT_SEC]);
	}

	switch (icon) {
	case INDICATOR_ICON_USB:
		if (!indicators.usb && indicators.sd)
			print_indicator(INDICATOR_ICON_SD, 1, 2);
		else
			print_indicator(INDICATOR_ICON_USB, indicators.usb, 2);
		break;
	case INDICATOR_ICON_SD:
		if (!indicators.usb)
			print_indicator(INDICATOR_ICON_SD, indicators.sd, 2);
		break;
	case INDICATOR_ICON_PLAY:
		if (!indicators.play && indicators.pause)
			print_indicator(INDICATOR_ICON_PAUSE, 1, 1);
		else
			print_indicator(INDICATOR_ICON_PLAY, indicators.play, 1);
		break;
	case INDICATOR_ICON_PAUSE:
		if (!indicators.pause && indicators.play)
			print_indicator(INDICATOR_ICON_PLAY, 1, 1);
		else
			print_indicator(INDICATOR_ICON_PAUSE, indicators.pause, 1);
		break;
	case INDICATOR_ICON_ETH:
		if (!indicators.eth && indicators.wifi)
			print_indicator(INDICATOR_ICON_WIFI, 1, 0);
		else
			print_indicator(INDICATOR_ICON_ETH, indicators.eth, 0);
		break;
	case INDICATOR_ICON_WIFI:
		if (!indicators.eth)
			print_indicator(INDICATOR_ICON_WIFI, indicators.wifi, 0);
		break;
	case INDICATOR_ICON_BT:
	case INDICATOR_ICON_APPS:
	case INDICATOR_ICON_SETUP:
		if (indicators.setup)
			print_indicator(INDICATOR_ICON_SETUP, indicators.setup, 3);
		else if (indicators.apps)
			print_indicator(INDICATOR_ICON_APPS, indicators.apps, 3);
		else
			print_indicator(INDICATOR_ICON_BT, indicators.bt, 3);
		break;
	default:
		break;
	}
}

static size_t gfx_mono_ctrl_read_data(unsigned char *data, size_t length)
{
	return 0;
}

static size_t gfx_mono_ctrl_write_data(const unsigned char *_data, size_t length)
{
	return length;
}

static size_t gfx_mono_ctrl_write_display_data(const struct vfd_display_data *data)
{
	size_t status = sizeof(*data);
	if (data->mode != old_data.mode) {
		unsigned char i;
		icon_x_offset = 0;
		memset(&old_data, 0, sizeof(old_data));
		specific_gfx_mono_ctrl.clear();
		switch (data->mode) {
		case DISPLAY_MODE_CLOCK:
			old_data.mode = DISPLAY_MODE_CLOCK;
			for (i = 0; i < MAX_INDICATORS; i++)
				print_indicator(indicators_on_screen[i], 1, i);
			old_data.mode = 0;
			break;
		case DISPLAY_MODE_DATE:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_CALENDAR);
			break;
		case DISPLAY_MODE_CHANNEL:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_CHANNEL);
			break;
		case DISPLAY_MODE_PLAYBACK_TIME:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_MEDIA);
			break;
		case DISPLAY_MODE_TITLE:
			break;
		case DISPLAY_MODE_TEMPERATURE:
			if (show_icons)
				icon_x_offset = print_icon(INDICATOR_ICON_TEMP);
			break;
		default:
			status = 0;
			break;
		}
	}

	switch (data->mode) {
	case DISPLAY_MODE_CLOCK:
		print_clock(data, 1);
		break;
	case DISPLAY_MODE_DATE:
		print_date(data);
		break;
	case DISPLAY_MODE_CHANNEL:
		print_channel(data);
		break;
	case DISPLAY_MODE_PLAYBACK_TIME:
		print_playback_time(data);
		break;
	case DISPLAY_MODE_TITLE:
		print_title(data);
		break;
	case DISPLAY_MODE_TEMPERATURE:
		print_temperature(data);
		break;
	default:
		status = 0;
		break;
	}

	old_data = *data;
	return status;
}

static unsigned char print_icon(unsigned char ch)
{
	char str[] = { ch, 0 };
	unsigned char offset_x = 0;
	unsigned char x, y;
	switch (display_mode) {
	case DISPLAY_MODE_128x32:
		y = (rows - font_icons.font_height) / 2;
		print_string(str, &font_icons, 0, y);
		offset_x = font_icons.font_width + font_text.font_width / 2;
		break;
	case DISPLAY_MODE_96x32	:
		y = (rows - font_icons.font_height) / 2;
		print_string(str, &font_icons, 0, y);
		offset_x = font_icons.font_width;
		break;
	case DISPLAY_MODE_80x32	:
		break;
	case DISPLAY_MODE_64x48	:
	case DISPLAY_MODE_64x64	:
	case DISPLAY_MODE_96x64	:
		print_string(str, &font_icons, 0, font_text.font_height);
		break;
	case DISPLAY_MODE_296x128:
	case DISPLAY_MODE_200x200:
	case DISPLAY_MODE_80x48	:
	case DISPLAY_MODE_128x64:
	case DISPLAY_MODE_80x64	:
		x = (columns - font_icons.font_width) / 2;
		print_string(str, &font_icons, x, font_text.font_height);
		break;
	}

	return offset_x;
}

static void print_indicator(unsigned char ch, unsigned char state, unsigned char index)
{
	char str[] = { state ? ch : INDICATOR_ICON_NONE, 0 };
	unsigned char x, y;
	if (index >= MAX_INDICATORS)
		return;

	indicators_on_screen[index] = str[0];
	if (old_data.mode == DISPLAY_MODE_CLOCK) {
		switch (display_mode) {
		case DISPLAY_MODE_296x128:
		case DISPLAY_MODE_128x32:
		{
			char size = (columns - (font_text.font_width * 5)) / 2;
			x = (size - font_indicators.font_width) / 2;
			if (index >= 2)
				x += size + (font_text.font_width * 5);
			y = font_indicators.font_height * (index % 2);
			print_string(str, &font_indicators, x, y);
			break;
		}
		case DISPLAY_MODE_96x32	:
		case DISPLAY_MODE_80x32	:
			break;
		case DISPLAY_MODE_64x48	:
		case DISPLAY_MODE_64x64	:
		case DISPLAY_MODE_96x64	:
		case DISPLAY_MODE_80x48	:
		case DISPLAY_MODE_128x64:
		case DISPLAY_MODE_80x64	:
			x = columns / MAX_INDICATORS;
			x = (x - font_indicators.font_width) / 2 + index * x;
			print_string(str, &font_indicators, x, font_text.font_height);
			break;
		case DISPLAY_MODE_200x200:
			x = columns / MAX_INDICATORS;
			x = (x - font_indicators.font_width) / 2 + index * x;
			print_string(str, &font_indicators, x, font_text.font_height + ((font_small_text.font_height * 5) / 2));
			break;
		}
	}
}

static void print_clock_date(const struct vfd_display_data *data, unsigned char force_print)
{
	if ((rows == 128 || rows == 200) && !gfx_mono_ctrl_display.flags_transpose)
	{
		force_print |= data->time_date.day != old_data.time_date.day || data->time_date.month != old_data.time_date.month || data->time_date.year != old_data.time_date.year;
		if (force_print)
		{
			char buffer[20];
			const char *days[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
			const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
			int len = scnprintf(buffer, sizeof(buffer), "%s %02d, %04d", months[data->time_date.month], data->time_date.day, data->time_date.year);
			unsigned char offset = (columns - (len * font_small_text.font_width)) / 2;
			print_string(buffer, &font_small_text, offset, font_text.font_height);
			len = scnprintf(buffer, sizeof(buffer), "%s", days[data->time_date.day_of_week]);
			offset = (columns - (len * font_small_text.font_width)) / 2;
			print_string(buffer, &font_small_text, offset, font_text.font_height + font_small_text.font_height);
		}
	}
}

static void print_clock(const struct vfd_display_data *data, unsigned char print_seconds)
{
	char buffer[10];
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE;
	unsigned char offset = 0;
	unsigned char colon_on = data->colon_on || dev->status_led_mask & ledDots[LED_DOT_SEC] ? 1 : 0;
	print_seconds &= gfx_mono_ctrl_display.flags_secs & show_colon;
	if (gfx_mono_ctrl_display.flags_transpose) {
		if (force_print || data->time_date.minutes != old_data.time_date.minutes ||
			data->time_date.hours != old_data.time_date.hours) {
			offset = (columns - (font_text.font_height * 8 * 2 + show_colon * font_text.font_width)) / 2;
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
			print_string(buffer, &font_text, offset, 0);
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
			print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
		}
		if (colon_on != old_data.colon_on && show_colon) {
			unsigned char offset = (columns - font_text.font_width) / 2;
			specific_gfx_mono_ctrl.print_char(colon_on ? ':' : ' ', &font_text, offset, rows - font_text.font_height);
		}
	} else if (!force_print) {
		const int len = print_seconds ? 8 : 5;
		offset = (columns - (font_text.font_width * len)) / 2;
		if (data->time_date.hours != old_data.time_date.hours) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
			print_string(buffer, &font_text, offset, 0);
		}
		offset += 2 * font_text.font_width;
		if (show_colon) {
			if (colon_on != old_data.colon_on)
				specific_gfx_mono_ctrl.print_char(colon_on ? ':' : ' ', &font_text, offset, 0);
			offset += font_text.font_width;
		}
		if (data->time_date.minutes != old_data.time_date.minutes) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
			print_string(buffer, &font_text, offset, 0);
		}
		offset += 2 * font_text.font_width;
		if (print_seconds) {
			if (colon_on != old_data.colon_on)
				specific_gfx_mono_ctrl.print_char(colon_on ? ':' : ' ', &font_text, offset, 0);
			offset += font_text.font_width;
			if (data->time_date.seconds != old_data.time_date.seconds) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
				print_string(buffer, &font_text, offset + (6 * font_text.font_width), 0);
			}
		}
	} else if (print_seconds) {
		int len = scnprintf(buffer, sizeof(buffer), "%02d%c%02d%c%02d", data->time_date.hours, colon_on ? ':' : ' ', data->time_date.minutes,
			colon_on ? ':' : ' ', data->time_date.seconds);
		offset = (columns - (font_text.font_width * len)) / 2;
		print_string(buffer, &font_text, offset, 0);
	} else {
		int len;
		if (show_colon)
			len = scnprintf(buffer, sizeof(buffer), "%02d%c%02d", data->time_date.hours, colon_on ? ':' : ' ', data->time_date.minutes);
		else
			len = scnprintf(buffer, sizeof(buffer), "%02d%02d", data->time_date.hours, data->time_date.minutes);
		offset = (columns - (font_text.font_width * len)) / 2;
		print_string(buffer, &font_text, offset, 0);
	}
	print_clock_date(data, force_print);
}

static void print_channel(const struct vfd_display_data *data)
{
	char buffer[10];
	scnprintf(buffer, sizeof(buffer), "%*d", 4, data->channel_data.channel % 10000);
	print_string(buffer, &font_text, font_icons.font_width + (font_text.font_width / 2), 0);
}

static void print_playback_time(const struct vfd_display_data *data)
{
	char buffer[20];
	unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (show_colon * font_text.font_width + font_text.font_width * 4)) / 2;
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE || data->time_date.hours != old_data.time_date.hours;
	if (gfx_mono_ctrl_display.flags_transpose) {
		if (data->time_date.hours > 0) {
			if (force_print || data->time_date.minutes != old_data.time_date.minutes ||
				data->time_date.hours != old_data.time_date.hours) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
				print_string(buffer, &font_text, offset, 0);
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
			}
		} else {
			if (force_print || data->time_date.seconds != old_data.time_date.seconds ||
				data->time_date.minutes != old_data.time_date.minutes) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset, 0);
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
				print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
			}
		}
		if (show_colon)
			specific_gfx_mono_ctrl.print_char(data->colon_on ? ':' : ' ', &font_text, offset + (font_text.font_height * 8), rows - font_text.font_height);
	} else if (!force_print) {
		if (data->colon_on != old_data.colon_on && show_colon) {
			specific_gfx_mono_ctrl.print_char(data->colon_on ? ':' : ' ', &font_text, offset + (2 * font_text.font_width), 0);
		}
		if (data->time_date.hours > 0) {
			if (data->time_date.hours != old_data.time_date.hours) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
				print_string(buffer, &font_text, offset, 0);
			}
			if (data->time_date.minutes != old_data.time_date.minutes) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset + (show_colon * font_text.font_width + 2 * font_text.font_width), 0);
			}
		} else {
			if (data->time_date.minutes != old_data.time_date.minutes) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
				print_string(buffer, &font_text, offset, 0);
			}
			if (data->time_date.seconds != old_data.time_date.seconds) {
				scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
				print_string(buffer, &font_text, offset + (show_colon * font_text.font_width + 2 * font_text.font_width), 0);
			}
		}
	} else {
		unsigned char pos0, pos1;
		if (data->time_date.hours > 0) {
			pos0 = data->time_date.hours;
			pos1 = data->time_date.minutes;
		} else {
			pos0 = data->time_date.minutes;
			pos1 = data->time_date.seconds;
		}
		if (show_colon)
			scnprintf(buffer, sizeof(buffer), "%02d%c%02d", pos0, data->colon_on ? ':' : ' ', pos1);
		else
			scnprintf(buffer, sizeof(buffer), "%02d%02d", pos0, pos1);
		print_string(buffer, &font_text, offset, 0);
	}
	if (rows >= 6 && !gfx_mono_ctrl_display.flags_transpose && strcmp(data->string_main, old_data.string_main)) {
		struct rect rect;
		offset = show_icons * font_icons.font_width;
		init_rect(&rect, &font_small_text, data->string_main, offset, font_text.font_height, 0);
		if (rect.length > 0) {
			if (show_icons) {
				print_icon(INDICATOR_ICON_NONE);
				buffer[0] = INDICATOR_ICON_MEDIA;
				buffer[1] = '\0';
				print_string(buffer, &font_icons, 0, font_text.font_height);
			}
			if (rect.length < strlen(data->string_main)) {
				scnprintf(buffer, sizeof(buffer), "%s", data->string_main);
				buffer[rect.length - 1] = 0x7F; // 0x7F = position of ellipsis.
				buffer[rect.length] = '\0';
				print_string(buffer, &font_small_text, offset, font_text.font_height);
			} else {
				print_string(data->string_main, &font_small_text, offset, font_text.font_height);
			}
		}
	}
}

static void print_title(const struct vfd_display_data *data)
{
	unsigned char offset = (unsigned char)max((size_t)0, (columns - (font_text.font_width * strlen(data->string_main))) / 2);
	if (strlen(data->string_secondary) > 0 && prepare_and_print_string(data->string_main, &font_text, offset, font_small_text.font_height))
		prepare_and_print_string(data->string_secondary, &font_small_text, 0, 0);
	else
		prepare_and_print_string(data->string_main, &font_text, offset, 0);
}

static void print_date(const struct vfd_display_data *data)
{
	char buffer[10];
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE || data->time_date.day != old_data.time_date.day || data->time_date.month != old_data.time_date.month;
	unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (show_colon * font_text.font_width + font_text.font_width * 4)) / 2;
	if (force_print) {
		if (gfx_mono_ctrl_display.flags_transpose) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_secondary._reserved ? data->time_date.month + 1 : data->time_date.day);
			print_string(buffer, &font_text, offset, 0);
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_secondary._reserved ? data->time_date.day : data->time_date.month + 1);
			print_string(buffer, &font_text, offset + show_colon * font_text.font_width + (font_text.font_height * 8), 0);
			if (show_colon)
				specific_gfx_mono_ctrl.print_char('|', &font_text, offset + font_text.font_height * 8, rows - font_text.font_height);
		} else {
			unsigned char day, month;
			if (data->time_secondary._reserved) {
				day = data->time_date.month + 1;
				month = data->time_date.day;
			} else {
				day = data->time_date.day;
				month = data->time_date.month + 1;
			}
			if (show_colon)
				scnprintf(buffer, sizeof(buffer), "%02d/%02d", day, month);
			else
				scnprintf(buffer, sizeof(buffer), "%02d%02d", day, month);
			print_string(buffer, &font_text, offset, 0);
		}
	}
}

static void print_temperature(const struct vfd_display_data *data)
{
	char buffer[10];
	if (data->temperature != old_data.temperature) {
		if (gfx_mono_ctrl_display.flags_transpose) {
			unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (2 * 8 * font_text.font_height)) / 2;
			scnprintf(buffer, sizeof(buffer), "%02d", data->temperature % 100);
			print_string(buffer, &font_text, offset, 0);
			scnprintf(buffer, sizeof(buffer), "%cC", 0x7F); // 0x7F = position of degree.
			print_string(buffer, &font_text, offset + 8 * font_text.font_height, 0);
		} else {
			size_t len = scnprintf(buffer, sizeof(buffer), "%d%cC", data->temperature % 1000, 0x7F); // 0x7F = position of degree.
			unsigned char offset = icon_x_offset ? icon_x_offset : (columns - (len * font_text.font_width)) / 2;
			print_string(buffer, &font_text, offset, 0);
		}
	}
}

static void init_font(struct font *font_struct, const unsigned char *font_bitmaps)
{
	if (swap_banks_orientation) {
		font_struct->font_width = font_bitmaps[0] / 8;
		font_struct->font_height = font_bitmaps[1];
	} else {
		font_struct->font_width = font_bitmaps[0];
		font_struct->font_height = font_bitmaps[1] / 8;
	}
	font_struct->font_offset = font_bitmaps[2];
	font_struct->font_char_size = font_struct->font_height * font_struct->font_width;
	font_struct->font_char_count = font_bitmaps[3];
	font_struct->font_bitmaps = font_bitmaps;
#if GFX_MONO_CTRL_PRINT_DEBUG
	pr_dbg2("font_width = %d, font_height = %d, font_offset = %d, font_char_size = %d, font_char_count = %d\n",
		font_struct->font_width, font_struct->font_height, font_struct->font_offset, font_struct->font_char_size, font_struct->font_char_count);
#endif
}

static void init_rect(struct rect *rect, const struct font *font, const char *str, unsigned char x, unsigned char y, unsigned char transposed)
{
	unsigned char c_width = 0, c_height = 0;
	memset(rect, 0, sizeof(*rect));
	if (x < columns && y < rows) {
		rect->font = font;
		if (!transposed) {
			rect->x1 = x;
			rect->y1 = y;
			rect->width = columns - x;
			rect->height = rows - y;
			c_width = rect->width / font->font_width;
			c_height = rect->height / font->font_height;
			rect->length = (unsigned char)min(strlen(str), (size_t)(c_width * c_height));
			rect->text_width = min(c_width, rect->length);
			for (rect->text_height = 0; rect->text_height < c_height; rect->text_height++)
				if (rect->text_width * rect->text_height >= rect->length)
					break;
			rect->width = rect->text_width * font->font_width;
			rect->height = rect->text_height * font->font_height;
			rect->x2 = x + rect->width - 1;
			rect->y2 = y + rect->height - 1;
		} else {
			const unsigned short font_height = font->font_height * 8;
			const unsigned short font_width = font->font_width / 8;
			rect->width = columns - x;
			rect->height = rows - y;
			c_width = rect->width / font_height;
			c_height = rect->height / font_width;
			rect->length = (unsigned char)min(strlen(str), (size_t)(c_width * c_height));
			rect->text_height = min(rect->length, c_height);
			for (rect->text_width = 0; rect->text_width < c_width; rect->text_width++)
				if (rect->text_width * rect->text_height >= rect->length)
					break;
			rect->width = rect->text_width * font_height;
			rect->height = rect->text_height * font_width;
			rect->x1 = x;
			rect->y2 = rows - 1 - y;
			rect->x2 = rect->x1 + rect->width - 1;
			rect->y1 = rect->y2 - rect->height + 1;
		}
	}

#if GFX_MONO_CTRL_PRINT_DEBUG
	pr_dbg2("str = %s, x = %d, y = %d, transposed = %d, c_width = %d, c_height = %d, length = %d, xy1 = (%d,%d), xy2 = (%d,%d), size = (%d,%d), text size = (%d,%d)\n",
		str, x, y, transposed, c_width, c_height, rect->length, rect->x1, rect->y1, rect->x2, rect->y2, rect->width, rect->height, rect->text_width, rect->text_height);
#endif
}
