#include "dummy.h"

static unsigned char dummy_init(void);
static unsigned short dummy_get_brightness_levels_count(void);
static unsigned short dummy_get_brightness_level(void);
static unsigned char dummy_set_brightness_level(unsigned short level);
static unsigned char dummy_get_power(void);
static void dummy_set_power(unsigned char state);
static struct vfd_display *dummy_get_display_type(void);
static unsigned char dummy_set_display_type(struct vfd_display *display);
static void dummy_set_icon(const char *name, unsigned char state);
static size_t dummy_read_data(unsigned char *data, size_t length);
static size_t dummy_write_data(const unsigned char *data, size_t length);
static size_t dummy_write_display_data(const struct vfd_display_data *data);

static struct controller_interface dummy_interface = {
	.init = dummy_init,
	.get_brightness_levels_count = dummy_get_brightness_levels_count,
	.get_brightness_level = dummy_get_brightness_level,
	.set_brightness_level = dummy_set_brightness_level,
	.get_power = dummy_get_power,
	.set_power = dummy_set_power,
	.get_display_type = dummy_get_display_type,
	.set_display_type = dummy_set_display_type,
	.set_icon = dummy_set_icon,
	.read_data = dummy_read_data,
	.write_data = dummy_write_data,
	.write_display_data = dummy_write_display_data,
};

static struct vfd_dev *dev = NULL;

struct controller_interface *init_dummy(struct vfd_dev *_dev)
{
	dev = _dev;
	return &dummy_interface;
}

static unsigned char dummy_init(void)
{
	return 1;
}

static unsigned short dummy_get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short dummy_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char dummy_set_brightness_level(unsigned short level)
{
	dev->brightness = level & 0x7;
	dev->power = 1;
	return 1;
}

static unsigned char dummy_get_power(void)
{
	return dev->power;
}

static void dummy_set_power(unsigned char state)
{
	dev->power = state;
}

static struct vfd_display *dummy_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char dummy_set_display_type(struct vfd_display *display)
{
	dev->dtb_active.display = *display;
	return 1;
}

static void dummy_set_icon(const char *name, unsigned char state)
{
}

static size_t dummy_read_data(unsigned char *data, size_t length)
{
	return 0;
}

static size_t dummy_write_data(const unsigned char *_data, size_t length)
{
	return length;
}

static size_t dummy_write_display_data(const struct vfd_display_data *data)
{
	return sizeof(*data);
}
