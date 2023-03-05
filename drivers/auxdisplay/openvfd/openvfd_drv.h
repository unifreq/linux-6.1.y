#ifndef __OPENVFD_DRV_H__
#define __OPENVFD_DRV_H__

#ifdef MODULE
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#endif
#include "glyphs.h"

#ifdef MODULE
#if 0
#define pr_dbg(args...) printk(KERN_ALERT "OpenVFD: " args)
#else
#define pr_dbg(args...)
#endif
#endif

#define pr_error(args...) printk(KERN_ALERT "OpenVFD: " args)
#define pr_dbg2(args...) printk(KERN_DEBUG "OpenVFD: " args)

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define  OPENVFD_DRIVER_VERSION	"V1.4.2"

/*
 * Ioctl definitions
 */

/* Use 'M' as magic number */
#define VFD_IOC_MAGIC			'M'
#define VFD_IOC_SMODE			_IOW(VFD_IOC_MAGIC,  1, int)
#define VFD_IOC_GMODE			_IOR(VFD_IOC_MAGIC,  2, int)
#define VFD_IOC_SBRIGHT		_IOW(VFD_IOC_MAGIC,  3, int)
#define VFD_IOC_GBRIGHT		_IOR(VFD_IOC_MAGIC,  4, int)
#define VFD_IOC_POWER			_IOW(VFD_IOC_MAGIC,  5, int)
#define VFD_IOC_GVER			_IOR(VFD_IOC_MAGIC,  6, int)
#define VFD_IOC_STATUS_LED		_IOW(VFD_IOC_MAGIC,  7, int)
#define VFD_IOC_GDISPLAY_TYPE		_IOR(VFD_IOC_MAGIC,  8, int)
#define VFD_IOC_SDISPLAY_TYPE		_IOW(VFD_IOC_MAGIC,  9, int)
#define VFD_IOC_SCHARS_ORDER		_IOW(VFD_IOC_MAGIC, 10, u_int8[7])
#define VFD_IOC_USE_DTB_CONFIG		_IOW(VFD_IOC_MAGIC, 11, int)
#define VFD_IOC_MAXNR			12

#ifdef MODULE

#define MOD_NAME_CLK       "openvfd_gpio_clk"
#define MOD_NAME_DAT       "openvfd_gpio_dat"
#define MOD_NAME_STB       "openvfd_gpio_stb"
#define MOD_NAME_GPIO0     "openvfd_gpio0"
#define MOD_NAME_GPIO1     "openvfd_gpio1"
#define MOD_NAME_GPIO2     "openvfd_gpio2"
#define MOD_NAME_GPIO3     "openvfd_gpio3"
#define MOD_NAME_PROT      "openvfd_gpio_protocol"
#define MOD_NAME_CHARS     "openvfd_chars"
#define MOD_NAME_DOTS      "openvfd_dot_bits"
#define MOD_NAME_TYPE      "openvfd_display_type"

#endif

#define DEV_NAME           "openvfd"

struct vfd_display {
	u_int8 type;
	u_int8 reserved;
	u_int8 flags;
	u_int8 controller;
};

#ifdef MODULE

struct vfd_dtb_config {
	u_int8 dat_index[7];
	u_int8 led_dots[8];
	struct vfd_display display;
};

struct vfd_pin {
	int pin;
	union {
		struct {
			u_int8 active_low	: 1;
			u_int8 single_ended	: 1;
			u_int8 open_drain	: 1;
			u_int8 sleep_keep	: 1;
			u_int8 pullup_on	: 1;
			u_int8 pulldown_on	: 1;
			u_int8 kick_high	: 1;
			u_int8 kick_low		: 1;
			u_int8 reserved2;
			u_int8 reserved3;
			u_int8 reserved4	: 7;
			u_int8 is_requested	: 1;
		} bits;
		unsigned int value;
	} flags;
};

struct vfd_protocol {
	u_int8 protocol;
	u_int8 device_id;
};

struct vfd_dev {
	struct vfd_pin clk_pin;
	struct vfd_pin dat_pin;
	struct vfd_pin stb_pin;
	struct vfd_pin gpio0_pin;
	struct vfd_pin gpio1_pin;
	struct vfd_pin gpio2_pin;
	struct vfd_pin gpio3_pin;
	struct vfd_protocol hw_protocol;
	u_int16 wbuf[7];
	struct vfd_dtb_config dtb_active;
	struct vfd_dtb_config dtb_default;
	u_int8 mode;
	u_int8 power;
	u_int8 brightness;
	struct mutex *mutex;
	wait_queue_head_t kb_waitq;	/* read and write queues */
	struct timer_list timer;
	int key_respond_status;
	int Keyboard_diskstatus;
	u_int8 KeyPressCnt;
	u_int8  key_fg;
	u_int8  key_val;
	u_int8 status_led_mask;		/* Indicators mask */
};

struct vfd_platform_data {
	struct vfd_dev *dev;
};

#endif

struct vfd_display_data {
	u_int16 mode;
	u_int8 colon_on;
	u_int8 temperature;

	struct {
		u_int8 seconds;
		u_int8 minutes;
		u_int8 hours;
		u_int8 day_of_week;
		u_int8 day;
		u_int8 month;
		u_int16 year;
	} time_date;
	struct {
		u_int8 seconds;
		u_int8 minutes;
		u_int8 hours;
		u_int8 _reserved;
	} time_secondary;
	struct {
		u_int16 channel;
		u_int16 channel_count;
	} channel_data;

	char string_main[512];
	char string_secondary[128];
};

enum {
	PROTOCOL_NONE,
	PROTOCOL_I2C,
	PROTOCOL_MAX
};

enum {
	DISPLAY_MODE_NONE,
	DISPLAY_MODE_CLOCK,
	DISPLAY_MODE_CHANNEL,
	DISPLAY_MODE_PLAYBACK_TIME,
	DISPLAY_MODE_TITLE,
	DISPLAY_MODE_TEMPERATURE,
	DISPLAY_MODE_DATE,
	DISPLAY_MODE_MAX,
};

enum {
	CONTROLLER_FD628	= 0x00,
	CONTROLLER_FD620,
	CONTROLLER_TM1618,
	CONTROLLER_FD650,
	CONTROLLER_HBS658,
	CONTROLLER_FD655,
	CONTROLLER_FD6551,
	CONTROLLER_7S_MAX,
	CONTROLLER_IL3829	= 0xFA,
	CONTROLLER_PCD8544,
	CONTROLLER_SH1106,
	CONTROLLER_SSD1306,
	CONTROLLER_HD44780,
};

enum {
	DISPLAY_TYPE_5D_7S_NORMAL,	// T95U
	DISPLAY_TYPE_5D_7S_T95,		// T95K is different.
	DISPLAY_TYPE_5D_7S_X92,
	DISPLAY_TYPE_5D_7S_ABOX,
	DISPLAY_TYPE_FD620_REF,
	DISPLAY_TYPE_4D_7S_COL,
	DISPLAY_TYPE_5D_7S_M9_PRO,
	DISPLAY_TYPE_5D_7S_G9SX,
	DISPLAY_TYPE_4D_7S_FREESATGTC,
	DISPLAY_TYPE_5D_7S_TAP1,
	DISPLAY_TYPE_MAX,
};

#define DISPLAY_FLAG_TRANSPOSED	0x01
#define DISPLAY_FLAG_TRANSPOSED_INT	0x00010000
#define DISPLAY_FLAG_LOW_FREQ		0x40
#define DISPLAY_FLAG_LOW_FREQ_INT	0x00400000

enum {
	LED_DOT1_ALARM,
	LED_DOT1_USB,
	LED_DOT1_PLAY,
	LED_DOT1_PAUSE,
	LED_DOT1_SEC,
	LED_DOT1_ETH,
	LED_DOT1_WIFI,
	LED_DOT1_MAX
};

enum {
	LED_DOT2_APPS,
	LED_DOT2_SETUP,
	LED_DOT2_USB,
	LED_DOT2_CARD,
	LED_DOT2_SEC,
	LED_DOT2_HDMI,
	LED_DOT2_CVBS,
	LED_DOT2_MAX
};

enum {
	LED_DOT3_UNUSED1,
	LED_DOT3_UNUSED2,
	LED_DOT3_POWER,
	LED_DOT3_LAN,
	LED_DOT3_SEC,
	LED_DOT3_WIFIHI,
	LED_DOT3_WIFILO,
	LED_DOT3_MAX
};

enum {
	LED_DOT4_BT,
	LED_DOT4_ETH,
	LED_DOT4_WIFI,
	LED_DOT4_SPDIF,
	LED_DOT4_SEC,
	LED_DOT4_HDMI,
	LED_DOT4_AV,
	LED_DOT4_MAX
};

#define LED_DOT_SEC	LED_DOT1_SEC
#define LED_DOT_MAX	LED_DOT1_MAX

static const u_int8 ledDots[LED_DOT_MAX] = {
	0x01,
	0x02,
	0x04,
	0x08,
	0x10,
	0x20,
	0x40
};

/* *************************************************************************************************************************************** *
*	Status Description	| bit7	| bit6	| bit5	| bit4	| bit3		| bit2	| bit1	| bit0	| Display_EN: Display enable bit, 1: Turn on display; 0: Turn off display
*				| 0	| 0	| 0	| 0	| Display_EN	|    brightness[3:0]    | Brightness: display brightness control bits, 000 ~ 111 represents brightness of 1 (min) ~ 8 (max)
* *************************************************************************************************************************************** */
#define FD628_DISP_ON	0x08					/* FD628 Display On */
#define FD628_DISP_OFF	0x00					/* FD628 Display Off */

typedef enum  _Brightness {					/* FD628 Brightness levels */
	FD628_Brightness_1 = 0x00,
	FD628_Brightness_2,
	FD628_Brightness_3,
	FD628_Brightness_4,
	FD628_Brightness_5,
	FD628_Brightness_6,
	FD628_Brightness_7,
	FD628_Brightness_8
}Brightness;

#endif
