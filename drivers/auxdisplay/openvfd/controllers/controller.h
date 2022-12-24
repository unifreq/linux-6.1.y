#ifndef __CONTROLLERH__
#define __CONTROLLERH__

#include "../openvfd_drv.h"

struct controller_interface {
	unsigned char (*init)(void);

	unsigned short (*get_brightness_levels_count)(void);
	unsigned short (*get_brightness_level)(void);
	unsigned char (*set_brightness_level)(unsigned short level);

	unsigned char (*get_power)(void);
	void (*set_power)(unsigned char state);
	void (*power_suspend)(void);
	void (*power_resume)(void);

	struct vfd_display *(*get_display_type)(void);
	unsigned char (*set_display_type)(struct vfd_display *display);

	void (*set_icon)(const char *name, unsigned char state);

	size_t (*read_data)(unsigned char *data, size_t length);
	size_t (*write_data)(const unsigned char *data, size_t length);
	size_t (*write_display_data)(const struct vfd_display_data *data);
};

#endif
