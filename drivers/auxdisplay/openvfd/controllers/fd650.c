#include "../protocols/i2c_sw.h"
#include "fd650.h"

/* ****************************** Define FD650 Commands ****************************** */
#define FD650_KEY_RDCMD		0x4F	/* Read keys command			*/
#define FD650_MODE_WRCMD		0x48	/* Write mode command			*/
#define FD650_DISP_ON			0x01	/* FD650 Display On			*/
#define FD650_DISP_OFF			0x00	/* FD650 Display Off			*/
#define FD650_7SEG_CMD			0x40	/* Set FD650 to work in 7-segment mode	*/
#define FD650_8SEG_CMD			0x00	/* Set FD650 to work in 8-segment mode	*/
#define FD650_BASE_ADDR		0x68	/* Base data address			*/
#define FD655_BASE_ADDR		0x66	/* Base data address			*/
#define FD650_DISP_STATE_WRCMD		0x00	/* Set display modw command		*/
/* *********************************************************************************** */

static unsigned char fd650_init(void);
static unsigned short fd650_get_brightness_levels_count(void);
static unsigned short fd650_get_brightness_level(void);
static unsigned char fd650_set_brightness_level(unsigned short level);
static unsigned char fd650_get_power(void);
static void fd650_set_power(unsigned char state);
static void fd650_power_suspend(void) { fd650_set_power(0); }
static void fd650_power_resume(void) { fd650_set_power(1); }
static struct vfd_display *fd650_get_display_type(void);
static unsigned char fd650_set_display_type(struct vfd_display *display);
static void fd650_set_icon(const char *name, unsigned char state);
static size_t fd650_read_data(unsigned char *data, size_t length);
static size_t fd650_write_data(const unsigned char *data, size_t length);
static size_t fd650_write_display_data(const struct vfd_display_data *data);

static struct controller_interface fd650_interface = {
	.init = fd650_init,
	.get_brightness_levels_count = fd650_get_brightness_levels_count,
	.get_brightness_level = fd650_get_brightness_level,
	.set_brightness_level = fd650_set_brightness_level,
	.get_power = fd650_get_power,
	.set_power = fd650_set_power,
	.power_suspend = fd650_power_suspend,
	.power_resume = fd650_power_resume,
	.get_display_type = fd650_get_display_type,
	.set_display_type = fd650_set_display_type,
	.set_icon = fd650_set_icon,
	.read_data = fd650_read_data,
	.write_data = fd650_write_data,
	.write_display_data = fd650_write_display_data,
};

size_t seg7_write_display_data(const struct vfd_display_data *data, unsigned short *raw_wdata, size_t sz);

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol = NULL;
static unsigned char ram_grid_count = 5;
static unsigned char ram_size = 10;
static struct vfd_display_data vfd_display_data;
extern const led_bitmap *ledCodes;
extern unsigned char ledDot;

struct controller_interface *init_fd650(struct vfd_dev *_dev)
{
	dev = _dev;
	return &fd650_interface;
}

inline static void fd650_write_cmd_data(unsigned char cmd, unsigned char data)
{
	protocol->write_cmd_data(&cmd, 1, &data, 1);
}

static size_t fd650_write_data_real(unsigned char address, const unsigned char *data, size_t length)
{
	unsigned char cmd = FD655_BASE_ADDR | (address & 0x07), i;
	if (length + address > ram_grid_count)
		return (-1);

	for (i = 0; i < length; i++, cmd += 2)
		fd650_write_cmd_data(cmd, *data++);
	return (0);
}

static unsigned char fd650_init(void)
{
	unsigned char slow_freq = dev->dtb_active.display.flags & DISPLAY_FLAG_LOW_FREQ;
	protocol = init_sw_i2c(0, MSB_FIRST, 0, dev->clk_pin, dev->dat_pin, slow_freq ? I2C_DELAY_20KHz : I2C_DELAY_100KHz, NULL);
	if (!protocol)
		return 0;

	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	fd650_write_data((unsigned char *)dev->wbuf, sizeof(dev->wbuf));
	fd650_set_brightness_level(dev->brightness);
	switch(dev->dtb_active.display.type) {
		case DISPLAY_TYPE_5D_7S_T95:
			ledCodes = LED_decode_tab1;
			break;
		case DISPLAY_TYPE_4D_7S_FREESATGTC:
			ledCodes = LED_decode_tab4;
			ledDot = p4;
			break;
		default:
			ledCodes = LED_decode_tab2;
			break;
	}
	return 1;
}

inline static unsigned char is_fd650(void)
{
	return dev->dtb_active.display.controller == CONTROLLER_FD650;
}

inline static unsigned char is_fd655(void)
{
	return dev->dtb_active.display.controller == CONTROLLER_FD655;
}

inline static unsigned char is_fd6551(void)
{
	return dev->dtb_active.display.controller == CONTROLLER_FD6551;
}

static unsigned short fd650_get_brightness_levels_count(void)
{
	return is_fd655() ? 3 : 8;
}

static unsigned short fd650_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char get_actual_brightness(void)
{
	unsigned char brightness = 0;
	if (is_fd655())
		brightness = min(2, (dev->brightness + 1) & 0x3) << 5;	// 11B disables current limit.
	else if (is_fd6551())
		brightness = min(7, 7 - dev->brightness) << 1;
	else
		brightness = ((dev->brightness + 1) & 0x7) << 4;	// 000B => 8/8 Duty cycle, 001B - 111B => 1/8 - 7/8 Duty cycle
	return brightness;
}

static unsigned char fd650_set_brightness_level(unsigned short level)
{
	dev->brightness = level & 0x7;
	fd650_write_cmd_data(FD650_MODE_WRCMD, FD650_DISP_STATE_WRCMD | get_actual_brightness() | FD650_DISP_ON);
	dev->power = 1;
	return 1;
}

static unsigned char fd650_get_power(void)
{
	return dev->power;
}

static void fd650_set_power(unsigned char state)
{
	dev->power = state;
	if (state)
		fd650_set_brightness_level(dev->brightness);
	else
		fd650_write_cmd_data(FD650_MODE_WRCMD, FD650_DISP_STATE_WRCMD | FD650_DISP_OFF);
}

static struct vfd_display *fd650_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char fd650_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->type < DISPLAY_TYPE_MAX && (is_fd650() || is_fd655() || is_fd6551()))
	{
		dev->dtb_active.display = *display;
		fd650_init();
		ret = 1;
	}

	return ret;
}

static void fd650_set_icon(const char *name, unsigned char state)
{
	struct vfd_dtb_config *dtb = &dev->dtb_active;
	switch (dtb->display.type) {
	case DISPLAY_TYPE_5D_7S_NORMAL:
	case DISPLAY_TYPE_5D_7S_T95:
	case DISPLAY_TYPE_5D_7S_G9SX:
		if (strncmp(name,"alarm",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_ALARM]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_ALARM]);
		} else if (strncmp(name,"usb",3) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_USB]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_USB]);
		} else if (strncmp(name,"play",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_PLAY]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_PLAY]);
		} else if (strncmp(name,"pause",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_PAUSE]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_PAUSE]);
		} else if (strncmp(name,"colon",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_SEC]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_SEC]);
		} else if (strncmp(name,"eth",3) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_ETH]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_ETH]);
		} else if (strncmp(name,"wifi",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT1_WIFI]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT1_WIFI]);
		}
		break;
	case DISPLAY_TYPE_5D_7S_X92:
		if (strncmp(name,"apps",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_APPS]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_APPS]);
		} else if (strncmp(name,"setup",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_SETUP]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_SETUP]);
		} else if (strncmp(name,"usb",3) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_USB]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_USB]);
		} else if (strncmp(name,"sd",2) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_CARD]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_CARD]);
		} else if (strncmp(name,"colon",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_SEC]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_SEC]);
		} else if (strncmp(name,"hdmi",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_HDMI]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_HDMI]);
		} else if (strncmp(name,"cvbs",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT2_CVBS]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT2_CVBS]);
		}
		break;
	case DISPLAY_TYPE_5D_7S_ABOX:
		if (strncmp(name,"power",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT3_POWER]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT3_POWER]);
		} else if (strncmp(name,"eth",3) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT3_LAN]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT3_LAN]);
		} else if (strncmp(name,"colon",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT3_SEC]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT3_SEC]);
		} else if (strncmp(name,"wifi",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT3_WIFIHI] | dtb->led_dots[LED_DOT3_WIFILO]) : (dev->status_led_mask & ~(dtb->led_dots[LED_DOT3_WIFIHI] | dtb->led_dots[LED_DOT3_WIFILO]));
		}
		break;
	case DISPLAY_TYPE_5D_7S_M9_PRO:
		if (strncmp(name,"b-t",3) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_BT]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_BT]);
		} else if (strncmp(name,"eth",3) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_ETH]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_ETH]);
		} else if (strncmp(name,"wifi",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_WIFI]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_WIFI]);
		} else if (strncmp(name,"spdif",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_SPDIF]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_SPDIF]);
		} else if (strncmp(name,"colon",5) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_SEC]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_SEC]);
		} else if (strncmp(name,"hdmi",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_HDMI]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_HDMI]);
		} else if (strncmp(name,"cvbs",4) == 0) {
			dev->status_led_mask = state ? (dev->status_led_mask | dtb->led_dots[LED_DOT4_AV]) : (dev->status_led_mask & ~dtb->led_dots[LED_DOT4_AV]);
		}
		break;
	default:
		if (strncmp(name,"colon",5) == 0)
			dev->status_led_mask = state ? (dev->status_led_mask | ledDots[LED_DOT_SEC]) : (dev->status_led_mask & ~ledDots[LED_DOT_SEC]);
		break;
	}
}

static size_t fd650_read_data(unsigned char *data, size_t length)
{
	unsigned char cmd = FD650_KEY_RDCMD;
	return protocol->read_cmd_data(&cmd, 1, data, length > 1 ? 1 : length) == 0 ? 1 : -1;
}

static size_t fd650_write_data(const unsigned char *_data, size_t length)
{
	size_t i;
	struct vfd_dtb_config *dtb = &dev->dtb_active;
	unsigned short *data = (unsigned short *)_data;
	unsigned char *tempBuf = (unsigned char *)dev->wbuf;

	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	length = min(length, (size_t)ram_size) / sizeof(unsigned short);
	if (data[0] & ledDots[LED_DOT_SEC]) {
		data[0] &= ~ledDots[LED_DOT_SEC];
		data[0] |= dtb->led_dots[LED_DOT_SEC];
	}
	// Apply LED indicators mask (usb, eth, wifi etc.)
	if (vfd_display_data.mode == DISPLAY_MODE_CLOCK)
		data[0] |= dev->status_led_mask;
	else
		data[0] |= (dev->status_led_mask & ~dtb->led_dots[LED_DOT_SEC]);
	for (i = 0; i <= length; i++)
		tempBuf[dtb->dat_index[i]] = (unsigned char)(data[i] & 0xFF);
	if (is_fd650())
		tempBuf[dtb->dat_index[0]] |= ((data[0] | dev->status_led_mask) & ledDots[LED_DOT_SEC]) ? ledDot : 0x00;

	return fd650_write_data_real(0, tempBuf, length) == 0 ? length : 0;
}

static size_t fd650_write_display_data(const struct vfd_display_data *data)
{
	unsigned short wdata[7];
	size_t status = seg7_write_display_data(data, wdata, sizeof(wdata));
	vfd_display_data = *data;
	if (status && !fd650_write_data((unsigned char*)wdata, 5*sizeof(wdata[0])))
		status = 0;
	return status;
}
