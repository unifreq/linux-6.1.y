#include "../protocols/i2c_sw.h"
#include "../protocols/spi_sw.h"
#include "fd628.h"

/* ****************************** Define FD628 Commands ****************************** */
#define FD628_KEY_RDCMD		0x42	/* Read keys command			*/
#define FD628_4DIG_CMD			0x00	/* Set FD628 to work in 4-digit mode	*/
#define FD628_5DIG_CMD			0x01	/* Set FD628 to work in 5-digit mode	*/
#define FD628_6DIG_CMD			0x02	/* Set FD628 to work in 6-digit mode	*/
#define FD628_7DIG_CMD			0x03	/* Set FD628 to work in 7-digit mode	*/
#define FD628_DIGADDR_WRCMD		0xC0	/* Write FD628 address			*/
#define FD628_ADDR_INC_DIGWR_CMD	0x40	/* Set Address Increment Mode		*/
#define FD628_ADDR_STATIC_DIGWR_CMD	0x44	/* Set Static Address Mode		*/
#define FD628_DISP_STATUE_WRCMD	0x80	/* Set display brightness command	*/
/* *********************************************************************************** */

static unsigned char fd628_init(void);
static unsigned short fd628_get_brightness_levels_count(void);
static unsigned short fd628_get_brightness_level(void);
static unsigned char fd628_set_brightness_level(unsigned short level);
static unsigned char fd628_get_power(void);
static void fd628_set_power(unsigned char state);
static void fd628_power_suspend(void) { fd628_set_power(0); }
static void fd628_power_resume(void) { fd628_set_power(1); }
static struct vfd_display *fd628_get_display_type(void);
static unsigned char fd628_set_display_type(struct vfd_display *display);
static void fd628_set_icon(const char *name, unsigned char state);
static size_t fd628_read_data(unsigned char *data, size_t length);
static size_t fd628_write_data(const unsigned char *data, size_t length);
static size_t fd628_write_display_data(const struct vfd_display_data *data);

static struct controller_interface fd628_interface = {
	.init = fd628_init,
	.get_brightness_levels_count = fd628_get_brightness_levels_count,
	.get_brightness_level = fd628_get_brightness_level,
	.set_brightness_level = fd628_set_brightness_level,
	.get_power = fd628_get_power,
	.set_power = fd628_set_power,
	.power_suspend = fd628_power_suspend,
	.power_resume = fd628_power_resume,
	.get_display_type = fd628_get_display_type,
	.set_display_type = fd628_set_display_type,
	.set_icon = fd628_set_icon,
	.read_data = fd628_read_data,
	.write_data = fd628_write_data,
	.write_display_data = fd628_write_display_data,
};

size_t seg7_write_display_data(const struct vfd_display_data *data, unsigned short *raw_wdata, size_t sz);

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol = NULL;
static unsigned char ram_grid_size = 2;
static unsigned char ram_grid_count = 7;
static unsigned char ram_size = 14;
static struct vfd_display_data vfd_display_data;
extern const led_bitmap *ledCodes;
extern unsigned char ledDot;

struct controller_interface *init_fd628(struct vfd_dev *_dev)
{
	dev = _dev;
	return &fd628_interface;
}

static size_t fd628_write_data_real(unsigned char address, const unsigned char *data, size_t length)
{
	unsigned char cmd = FD628_DIGADDR_WRCMD | address;
	if (length + address > ram_size)
		return (-1);

	protocol->write_byte(FD628_ADDR_INC_DIGWR_CMD);
	protocol->write_cmd_data(&cmd, 1, data, length);
	return (0);
}

static unsigned char fd628_init(void)
{
	unsigned char slow_freq = dev->dtb_active.display.flags & DISPLAY_FLAG_LOW_FREQ;
	protocol = dev->dtb_active.display.controller == CONTROLLER_HBS658 ?
		init_sw_i2c(0, LSB_FIRST, 1, dev->clk_pin, dev->dat_pin, slow_freq ? I2C_DELAY_20KHz : I2C_DELAY_100KHz, NULL) :
		init_sw_spi_3w(LSB_FIRST, dev->clk_pin, dev->dat_pin, dev->stb_pin, slow_freq ? SPI_DELAY_20KHz : SPI_DELAY_100KHz);
	if (!protocol)
		return 0;

	switch(dev->dtb_active.display.type) {
		case DISPLAY_TYPE_5D_7S_T95:
			ledCodes = LED_decode_tab1;
			break;
		case DISPLAY_TYPE_5D_7S_G9SX:
			ledCodes = LED_decode_tab3;
			break;
		case DISPLAY_TYPE_5D_7S_TAP1:
			ledCodes = LED_decode_tab5;
			break;
		default:
			ledCodes = LED_decode_tab2;
			break;
	}
	switch (dev->dtb_active.display.controller) {
		case CONTROLLER_FD628:
		default:
			ram_grid_size = 2;
			ram_grid_count = 7;
			protocol->write_byte(FD628_7DIG_CMD);
			break;
		case CONTROLLER_FD620:
			ram_grid_size = 2;
			ram_grid_count = 5;
			switch (dev->dtb_active.display.type) {
			case DISPLAY_TYPE_FD620_REF:
				protocol->write_byte(FD628_4DIG_CMD);
				break;
			default:
				protocol->write_byte(FD628_5DIG_CMD);
				break;
			}
			break;
		case CONTROLLER_TM1618:
			ram_grid_size = 2;
			ram_grid_count = 7;
			switch (dev->dtb_active.display.type) {
			case DISPLAY_TYPE_4D_7S_COL:
				protocol->write_byte(FD628_7DIG_CMD);
				break;
			case DISPLAY_TYPE_FD620_REF:
				protocol->write_byte(FD628_4DIG_CMD);
				break;
			default:
				protocol->write_byte(FD628_5DIG_CMD);
				break;
			}
			break;
		case CONTROLLER_HBS658:
			ram_grid_size = 1;
			ram_grid_count = 5;
			break;
	}

	ram_size = ram_grid_size * ram_grid_count;
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	fd628_write_data((unsigned char *)dev->wbuf, sizeof(dev->wbuf));
	fd628_set_brightness_level(dev->brightness);
	return 1;
}

static unsigned short fd628_get_brightness_levels_count(void)
{
	return 8;
}

static unsigned short fd628_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char fd628_set_brightness_level(unsigned short level)
{
	dev->brightness = level & 0x7;
	protocol->write_byte(FD628_DISP_STATUE_WRCMD | dev->brightness | FD628_DISP_ON);
	dev->power = 1;
	return 1;
}

static unsigned char fd628_get_power(void)
{
	return dev->power;
}

static void fd628_set_power(unsigned char state)
{
	dev->power = state;
	if (state)
		fd628_set_brightness_level(dev->brightness);
	else
		protocol->write_byte(FD628_DISP_STATUE_WRCMD | FD628_DISP_OFF);
}

static struct vfd_display *fd628_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char fd628_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->type < DISPLAY_TYPE_MAX && display->controller < CONTROLLER_7S_MAX && display->controller == CONTROLLER_FD650)
	{
		dev->dtb_active.display = *display;
		fd628_init();
		ret = 1;
	}

	return ret;
}

static void fd628_set_icon(const char *name, unsigned char state)
{
	struct vfd_dtb_config *dtb = &dev->dtb_active;
	switch (dtb->display.type) {
	case DISPLAY_TYPE_5D_7S_NORMAL:
	case DISPLAY_TYPE_5D_7S_T95:
	case DISPLAY_TYPE_5D_7S_G9SX:
	default:
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
	}
}

static size_t fd628_read_data(unsigned char *data, size_t length)
{
	protocol->write_byte(FD628_KEY_RDCMD);
	return protocol->read_data(data, length) == 0 ? length : -1;
}

extern void transpose8rS64(unsigned char* A, unsigned char* B);

static size_t fd628_write_data(const unsigned char *_data, size_t length)
{
	size_t i;
	struct vfd_dtb_config *dtb = &dev->dtb_active;
	unsigned short *data = (unsigned short *)_data;

	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	length = length > ram_size ? ram_grid_count : (length / sizeof(unsigned short));
	if (data[0] & ledDots[LED_DOT_SEC]) {
		data[0] &= ~ledDots[LED_DOT_SEC];
		data[0] |= dtb->led_dots[LED_DOT_SEC];
	}
	// Apply LED indicators mask (usb, eth, wifi etc.)
	if (vfd_display_data.mode == DISPLAY_MODE_CLOCK)
		data[0] |= dev->status_led_mask;
	else
		data[0] |= (dev->status_led_mask & ~dtb->led_dots[LED_DOT_SEC]);

	switch (dtb->display.type) {
	case DISPLAY_TYPE_5D_7S_NORMAL:
	case DISPLAY_TYPE_5D_7S_T95:
	case DISPLAY_TYPE_5D_7S_X92:
	case DISPLAY_TYPE_5D_7S_ABOX:
	case DISPLAY_TYPE_4D_7S_COL:
	case DISPLAY_TYPE_5D_7S_M9_PRO:
	case DISPLAY_TYPE_5D_7S_G9SX:
	default:
		for (i = 0; i < length; i++)
			dev->wbuf[dtb->dat_index[i]] = data[i];
		break;
	case DISPLAY_TYPE_FD620_REF:
	case DISPLAY_TYPE_4D_7S_FREESATGTC:
		for (i = 1; i < length; i++) {
			dev->wbuf[dtb->dat_index[i]] = data[i];
			if (data[0] & dtb->led_dots[LED_DOT_SEC])
				dev->wbuf[dtb->dat_index[i]] |= ledDot;				// DP is the colon.
		}
		break;
	}

	if (dtb->display.flags & DISPLAY_FLAG_TRANSPOSED) {
		unsigned char trans[8];
		length = ram_grid_count;
		memset(trans, 0, sizeof(trans));
		for (i = 0; i < length; i++)
			trans[i] = (unsigned char)dev->wbuf[i] << 1;
		trans[ram_grid_count] = trans[0];
		transpose8rS64(trans, trans);
		memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
		for (i = 0; i < ram_grid_count; i++)
			dev->wbuf[i] = trans[i+1];
	}

	switch (dtb->display.controller) {
	case CONTROLLER_FD628:
		// Memory map:
		// S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 xx S12 S13 S14 xx xx
		// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1  b2 b3  b4  b5  b6 b7
		for (i = 0; i < length; i++)
			dev->wbuf[i] |= (dev->wbuf[i] & 0xFC00) << 1;
		break;
	case CONTROLLER_FD620:
		// Memory map:
		// S1 S2 S3 S4 S5 S6 S7 xx xx xx xx xx xx S8 xx xx
		// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1 b2 b3 b4 b5 b6 b7
		for (i = 0; i < length; i++)
			dev->wbuf[i] |= (dev->wbuf[i] & 0x80) ? 0x2000 : 0;
		break;
	case CONTROLLER_TM1618:
		// Memory map:
		// S1 S2 S3 S4 S5 xx xx xx xx xx xx S12 S13 S14 xx xx
		// b0 b1 b2 b3 b4 b5 b6 b7 b0 b1 b2 b3  b4  b5  b6 b7
		for (i = 0; i < length; i++)
			dev->wbuf[i] |= (dev->wbuf[i] & 0x00E0) << 6;
		break;
	case CONTROLLER_HBS658: {
		// Memory map:
		// S1 S2 S3 S4 S5 S6 S7 xx
		// b0 b1 b2 b3 b4 b5 b6 b7
			unsigned char *tempBuf = (unsigned char *)dev->wbuf;
			for (i = 1; i < length; i++)
				tempBuf[i] = (unsigned char)(dev->wbuf[i] & 0xFF);
		}
		break;
	}

	length *= ram_grid_size;
	return fd628_write_data_real(0, (unsigned char *)dev->wbuf, length) == 0 ? length : 0;
}

static size_t fd628_write_display_data(const struct vfd_display_data *data)
{
	unsigned short wdata[7];
	size_t status = seg7_write_display_data(data, wdata, sizeof(wdata));
	vfd_display_data = *data;
	if (status && !fd628_write_data((unsigned char*)wdata, 5*sizeof(wdata[0])))
		status = 0;
	return status;
}
