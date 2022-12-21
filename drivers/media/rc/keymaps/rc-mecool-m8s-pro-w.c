// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2022 Christian Hewitt <christianshewitt@gmail.com>

#include <media/rc-map.h>
#include <linux/module.h>

//
// Keytable for the MeCool M8S Pro W remote control
//

static struct rc_map_table mecool_m8s_pro_w[] = {

	{ 0x59, KEY_POWER },

	// TV CONTROLS

	{ 0x08, KEY_PREVIOUS },
	{ 0x0b, KEY_NEXT },
	{ 0x18, KEY_TEXT }, // INTERNET
	{ 0x19, KEY_MUTE },
	{ 0x13, KEY_VOLUMEUP },
	{ 0x17, KEY_VOLUMEDOWN },

	{ 0x0d, KEY_HOME },
	{ 0x05, KEY_BACK },

	{ 0x06, KEY_UP },
	{ 0x5a, KEY_LEFT },
	{ 0x1b, KEY_RIGHT },
	{ 0x1a, KEY_ENTER },
	{ 0x16, KEY_DOWN },

	{ 0x45, KEY_MENU },
	{ 0x12, KEY_CONTEXT_MENU }, // MOUSE

	{ 0x52, KEY_NUMERIC_1 },
	{ 0x50, KEY_NUMERIC_2 },
	{ 0x10, KEY_NUMERIC_3 },
	{ 0x56, KEY_NUMERIC_4 },
	{ 0x54, KEY_NUMERIC_5 },
	{ 0x14, KEY_NUMERIC_6 },
	{ 0x4e, KEY_NUMERIC_7 },
	{ 0x4c, KEY_NUMERIC_8 },
	{ 0x0c, KEY_NUMERIC_9 },
	{ 0x22, KEY_INFO }, // SEARCH
	{ 0x0f, KEY_NUMERIC_0 },
	{ 0x51, KEY_BACKSPACE },

};

static struct rc_map_list mecool_m8s_pro_w_map = {
	.map = {
		.scan     = mecool_m8s_pro_w,
		.size     = ARRAY_SIZE(mecool_m8s_pro_w),
		.rc_proto = RC_PROTO_NEC,
		.name     = RC_MAP_MECOOL_M8S_PRO_W,
	}
};

static int __init init_rc_map_mecool_m8s_pro_w(void)
{
	return rc_map_register(&mecool_m8s_pro_w_map);
}

static void __exit exit_rc_map_mecool_m8s_pro_w(void)
{
	rc_map_unregister(&mecool_m8s_pro_w_map);
}

module_init(init_rc_map_mecool_m8s_pro_w)
module_exit(exit_rc_map_mecool_m8s_pro_w)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christian Hewitt <christianshewitt@gmail.com");
