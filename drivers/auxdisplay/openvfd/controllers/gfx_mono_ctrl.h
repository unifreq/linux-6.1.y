#ifndef __GFXMONOCTRLH__
#define __GFXMONOCTRLH__

#include "controller.h"

struct font {
	unsigned char font_height;
	unsigned char font_width;
	unsigned short font_char_size;
	unsigned char font_offset;
	unsigned char font_char_count;
	const unsigned char *font_bitmaps;
};

struct rect {
	unsigned short x1;
	unsigned short y1;
	unsigned short x2;
	unsigned short y2;
	unsigned short width;
	unsigned short height;
	unsigned char text_width;
	unsigned char text_height;
	unsigned char length;
	const struct font *font;
};

struct screen_view {
	unsigned short columns;
	unsigned short rows;
	unsigned char colomn_offset;
	unsigned char swap_banks_orientation	: 1;
	unsigned char reserved			: 7;
};

struct specific_gfx_mono_ctrl
{
	unsigned char (*init)(void);
	unsigned char (*set_display_type)(struct vfd_display *display);

	void (*clear)(void);
	void (*set_power)(unsigned char state);
	void (*set_contrast)(unsigned char value);
	unsigned char (*set_xy)(unsigned short x, unsigned short y);
	void (*print_char)(char ch, const struct font *font_struct, unsigned char x, unsigned char y);
	void (*print_string)(const unsigned char *buffer, const struct rect *rect);

	void (*write_ctrl_command_buf)(const unsigned char *buf, unsigned int length);
	void (*write_ctrl_command)(unsigned char cmd);
	void (*write_ctrl_data_buf)(const unsigned char *buf, unsigned int length);
	void (*write_ctrl_data)(unsigned char data);

	const struct screen_view *screen_view;
};

struct controller_interface *init_gfx_mono_ctrl(struct vfd_dev *_dev, const struct specific_gfx_mono_ctrl *specific_gfx_mono_ctrl);

#endif
