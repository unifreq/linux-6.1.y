#include "../protocols/i2c_sw.h"
#include "hd44780.h"

/* **************************** Define HD44780 Constants ****************************** */
#define HD44780_CLEAR_RAM		0x01	/* Write mode command			*/
#define HD44780_HOME			0x02	/* Increment or decrement address	*/
#define HD44780_ENTRY_MODE		0x04	/* Write mode command			*/
#define HD44780_EM_ID			0x02	/* Increment or decrement address	*/
#define HD44780_EM_S			0x01	/* Accompanies display shift		*/
#define HD44780_DISPLAY_CONTROL		0x08	/* Write mode command			*/
#define HD44780_DC_D			0x04	/* Increment or decrement address	*/
#define HD44780_DC_C			0x02	/* Accompanies display shift		*/
#define HD44780_DC_B			0x01	/* Accompanies display shift		*/
#define HD44780_CURESOR_SHIFT		0x10	/* Write mode command			*/
#define HD44780_CS			0x08	/* FD650 Display On			*/
#define HD44780_RL			0x04	/* FD650 Display Off			*/
#define HD44780_FUNCTION		0x20	/* Set FD650 to work in 7-segment mode	*/
#define HD44780_F_DL			0x10	/* Set FD650 to work in 8-segment mode	*/
#define HD44780_F_N			0x08	/* Base data address			*/
#define HD44780_F_F			0x04	/* Set display brightness command	*/
#define HD44780_CGRA			0x40	/* Set CGRAM Address			*/
#define HD44780_DDRA			0x80	/* Set DDRAM Address			*/
/* ************************************************************************************ */
#define BACKPACK_BACKLIGHT		0x08
#define BACKPACK_ENABLE		0x04
#define BACKPACK_READ			0x02
#define BACKPACK_RS			0x01
#define BACKPACK_CMD			0x00

#define FLAGS_SHOW_SEC		0x01

#define BIG_2L_DOT			0xA5
#define BIG_4L_DOT			0x07
#define CUSTOM_ELLIPSIS		0x07

static const unsigned char cgram_4l_chars[8][8] = {
	{ 0x03, 0x07, 0x0F, 0x1F, 0x00, 0x00, 0x00, 0x00 },	// 0 - Top left
	{ 0x18, 0x1C, 0x1E, 0x1F, 0x00, 0x00, 0x00, 0x00 },	// 1 - Top right
	{ 0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00 },	// 2 - Top center
	{ 0x1F, 0x0F, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00 },	// 3 - Bottom left
	{ 0x1F, 0x1E, 0x1C, 0x18, 0x00, 0x00, 0x00, 0x00 },	// 4 - Bottom right
	{ 0x03, 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },	// 5 - Long left
	{ 0x18, 0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },	// 6 - Long right
	{ 0x00, 0x00, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E, 0x00 },	// 7 - Dot
};

static const unsigned char big_4l_chars[11][9] = {
	{   5,   2,   6, 255, ' ', 255,   3,   2,   4 },	// 0
	{   0, 255, ' ', ' ', 255, ' ',   2,   2,   2 },	// 1
	{   0,   2,   6,   5,   2,   4,   3,   2,   4 },	// 2
	{   0,   2,   6, ' ',   2, 255,   3,   2,   4 },	// 3
	{ 255, ' ', 255,   3,   2, 255, ' ', ' ',   2 },	// 4
	{ 255,   2,   4,   2,   2,   6,   3,   2,   4 },	// 5
	{   5,   2,   1, 255,   2,   6,   3,   2,   4 },	// 6
	{   3,   2, 255, ' ',   5,   4, ' ',   2, ' ' },	// 7
	{   5,   2,   6, 255,   2, 255,   3,   2,   4 },	// 8
	{   5,   2,   6,   3,   2, 255,   3,   2,   4 },	// 9

	{   5,   2,   1, 255, ' ', ' ',   3,   2,   4 },	// C
};

static const unsigned char cgram_2l_chars[8][8] = {
	{ 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 },	// char 1
	{ 0x18, 0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },	// char 2
	{ 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0F, 0x07, 0x03 },	// char 3
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F },	// char 4
	{ 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1C, 0x18 },	// char 5
	{ 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x1F, 0x1F },	// char 6
	{ 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F },	// char 7
	{ 0x03, 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },	// char 8
};

static const unsigned char big_2l_chars[11][6] = {
	{ 0x07, 0x00, 0x01, 0x02, 0x03, 0x04 },			// 0
	{ 0x08, 0x01, 0x20, 0x03, 0xFF, 0x03 },			// 1
	{ 0x05, 0x05, 0x01, 0xFF, 0x06, 0x06 },			// 2
	{ 0x00, 0x05, 0x01, 0x03, 0x06, 0x04 },			// 3
	{ 0x02, 0x03, 0xFF, 0x20, 0x20, 0xFF },			// 4
	{ 0xFF, 0x05, 0x05, 0x06, 0x06, 0x04 },			// 5
	{ 0x07, 0x05, 0x05, 0x02, 0x06, 0x04 },			// 6
	{ 0x00, 0x00, 0x01, 0x20, 0x07, 0x20 },			// 7
	{ 0x07, 0x05, 0x01, 0x02, 0x06, 0x04 },			// 8
	{ 0x07, 0x05, 0x01, 0x06, 0x06, 0x04 },			// 9

	{ 0x07, 0x00, 0x00, 0x02, 0x03, 0x03 },			// C
};

static const unsigned char cgram_ellipsis_chars[8] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00
};

static unsigned char hd47780_init(void);
static unsigned short hd47780_get_brightness_levels_count(void);
static unsigned short hd47780_get_brightness_level(void);
static unsigned char hd47780_set_brightness_level(unsigned short level);
static unsigned char hd47780_get_power(void);
static void hd47780_set_power(unsigned char state);
static void hd47780_power_suspend(void) { hd47780_set_power(0); }
static void hd47780_power_resume(void) { hd47780_init(); }
static struct vfd_display *hd47780_get_display_type(void);
static unsigned char hd47780_set_display_type(struct vfd_display *display);
static void hd47780_set_icon(const char *name, unsigned char state);
static size_t hd47780_read_data(unsigned char *data, size_t length);
static size_t hd47780_write_data(const unsigned char *data, size_t length);
static size_t hd47780_write_display_data(const struct vfd_display_data *data);

static struct controller_interface hd47780_interface = {
	.init = hd47780_init,
	.get_brightness_levels_count = hd47780_get_brightness_levels_count,
	.get_brightness_level = hd47780_get_brightness_level,
	.set_brightness_level = hd47780_set_brightness_level,
	.get_power = hd47780_get_power,
	.set_power = hd47780_set_power,
	.power_suspend = hd47780_power_suspend,
	.power_resume = hd47780_power_resume,
	.get_display_type = hd47780_get_display_type,
	.set_display_type = hd47780_set_display_type,
	.set_icon = hd47780_set_icon,
	.read_data = hd47780_read_data,
	.write_data = hd47780_write_data,
	.write_display_data = hd47780_write_display_data,
};

static void print_2l_char(unsigned short ch, unsigned char pos, unsigned char is_clock);
static void print_4l_char(unsigned short ch, unsigned char pos, unsigned char is_clock);
static void print_clock(const struct vfd_display_data *data, unsigned char print_seconds);
static void print_channel(const struct vfd_display_data *data);
static void print_playback_time(const struct vfd_display_data *data);
static void print_title(const struct vfd_display_data *data);
static void print_date(const struct vfd_display_data *data);
static void print_temperature(const struct vfd_display_data *data);

static struct vfd_dev *dev = NULL;
static struct protocol_interface *protocol = NULL;
static unsigned char columns = 16;
static unsigned char rows = 2;
static unsigned char backlight = BACKPACK_BACKLIGHT;
static unsigned char big_dot = BIG_2L_DOT;
static struct vfd_display_data old_data;

const char *days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

struct controller_interface *init_hd47780(struct vfd_dev *_dev)
{
	dev = _dev;
	memset(&old_data, 0, sizeof(old_data));
	return &hd47780_interface;
}

static void write_4_bits(unsigned char data) {
	unsigned char buffer[3] = { data, (unsigned char)(data | BACKPACK_ENABLE), data };
	protocol->write_data(buffer, 3);
}

static void write_lcd(unsigned char data, unsigned char mode) {
	unsigned char lo = (data & 0x0F) << 4;
	unsigned char hi = (data & 0xF0);
	mode |= backlight;
	write_4_bits(hi | mode);
	write_4_bits(lo | mode);
}

static void write_buf_lcd(const unsigned char *buf, unsigned int length)
{
	while (length--) {
		write_lcd(*buf, BACKPACK_RS);
		buf++;
	}
}

static unsigned char read_4_bits(unsigned char mode) {
	unsigned char data[2] = { (unsigned char)(mode | 0xF0), (unsigned char)(mode | 0xF0 | BACKPACK_ENABLE) };
	protocol->write_data(data, 2);
	protocol->read_byte(data + 1);
	protocol->write_data(&mode, 1);
	return data[1];
}

static unsigned char read_lcd(unsigned char mode) {
	unsigned char lo;
	unsigned char hi;
	mode &= ~BACKPACK_ENABLE;
	mode |= backlight | BACKPACK_READ;
	hi = read_4_bits(mode);
	lo = read_4_bits(mode);
	return ((hi & 0xF0) | (lo >> 4));
}

static unsigned char hd47780_init(void)
{
	unsigned char cmd = 0;
	protocol = init_sw_i2c(dev->dtb_active.display.reserved & 0x7F, MSB_FIRST, 1, dev->clk_pin, dev->dat_pin, I2C_DELAY_500KHz, NULL);
	if (!protocol)
		return 0;

	old_data.mode = DISPLAY_MODE_NONE;
	columns = (dev->dtb_active.display.type & 0x1F) << 1;
	rows = (dev->dtb_active.display.type >> 5) & 0x07;
	rows++;
	big_dot = rows > 2 ? BIG_4L_DOT : BIG_2L_DOT;

	write_4_bits(0x03 << 4);
	usleep_range(4300, 5000);
	write_4_bits(0x03 << 4);
	udelay(150);
	write_4_bits(0x03 << 4);
	udelay(150);
	write_4_bits(0x02 << 4);
	udelay(150);
	cmd = HD44780_FUNCTION;
	if (rows > 1)
		cmd |= HD44780_F_N | HD44780_F_F;
	write_lcd(cmd, BACKPACK_CMD);
	udelay(150);
	write_lcd(HD44780_DISPLAY_CONTROL, BACKPACK_CMD);
	write_lcd(HD44780_CLEAR_RAM, BACKPACK_CMD);
	usleep_range(1600, 2000);
	write_lcd(HD44780_ENTRY_MODE | HD44780_EM_ID, BACKPACK_CMD);
	write_lcd(HD44780_DISPLAY_CONTROL | HD44780_DC_D, BACKPACK_CMD);

	if (rows >= 3) {
		write_lcd(HD44780_CGRA, BACKPACK_CMD);
		write_buf_lcd((const unsigned char *)cgram_4l_chars, 64);
	} else if (rows == 2) {
		write_lcd(HD44780_CGRA, BACKPACK_CMD);
		write_buf_lcd((const unsigned char *)cgram_2l_chars, 64);
	}

	hd47780_set_brightness_level(dev->brightness);
	return 1;
}

static unsigned short hd47780_get_brightness_levels_count(void)
{
	return 2;
}

static unsigned short hd47780_get_brightness_level(void)
{
	return dev->brightness;
}

static unsigned char hd47780_set_brightness_level(unsigned short level)
{
	dev->brightness = level;
	dev->power = 1;
	backlight = dev->power && dev->brightness > 0 ? BACKPACK_BACKLIGHT : 0;
	write_lcd(HD44780_DISPLAY_CONTROL | HD44780_DC_D, BACKPACK_CMD);
	return 1;
}

static unsigned char hd47780_get_power(void)
{
	return dev->power;
}

static void hd47780_set_power(unsigned char state)
{
	dev->power = state;
	if (state)
		hd47780_set_brightness_level(dev->brightness);
	else {
		backlight = 0;
		write_lcd(HD44780_DISPLAY_CONTROL, BACKPACK_CMD);
	}
}

static struct vfd_display *hd47780_get_display_type(void)
{
	return &dev->dtb_active.display;
}

static unsigned char hd47780_set_display_type(struct vfd_display *display)
{
	unsigned char ret = 0;
	if (display->controller == CONTROLLER_HD44780)
	{
		dev->dtb_active.display = *display;
		hd47780_init();
		ret = 1;
	}
	return ret;
}

static void hd47780_set_icon(const char *name, unsigned char state)
{
	if (strncmp(name,"colon",5) == 0)
		dev->status_led_mask = state ? (dev->status_led_mask | ledDots[LED_DOT_SEC]) : (dev->status_led_mask & ~ledDots[LED_DOT_SEC]);
}

static size_t hd47780_read_data(unsigned char *data, size_t length)
{
	size_t count = length;
	write_lcd(HD44780_HOME, BACKPACK_CMD);
	while (count--) {
		*data = read_lcd(BACKPACK_RS);
		data++;
	}
	return length;
}

static size_t hd47780_write_data(const unsigned char *data, size_t length)
{
	size_t i;
	if (length == 0)
		return 0;

	if (rows >= 3) {
		unsigned char dot;
		const unsigned short *wdata = (const unsigned short *)data;
		length /= 2;
		for (i = 1; i < length; i++)
			print_4l_char(wdata[i] - 0x30, i-1, 1);
		if ((data[0] | dev->status_led_mask) & ledDots[LED_DOT_SEC])
			dot = big_dot;
		else
			dot = ' ';
		write_lcd(HD44780_DDRA + 0x06, BACKPACK_CMD);
		write_lcd(dot, BACKPACK_RS);
		write_lcd(HD44780_DDRA + 0x46, BACKPACK_CMD);
		write_lcd(dot, BACKPACK_RS);
	}
	else {
		write_lcd(HD44780_HOME, BACKPACK_CMD);
		usleep_range(1600, 2000);
		if (length > 2)
			write_lcd(data[2], BACKPACK_RS);
		if (length > 4)
			write_lcd(data[4], BACKPACK_RS);
		if ((data[0] | dev->status_led_mask) & ledDots[LED_DOT_SEC])
			write_lcd(':', BACKPACK_RS);
		else
			write_lcd(' ', BACKPACK_RS);
		if (length > 6)
			write_lcd(data[6], BACKPACK_RS);
		if (length > 8)
			write_lcd(data[8], BACKPACK_RS);
	}

	return length;
}

static size_t hd47780_write_display_data(const struct vfd_display_data *data)
{
	size_t status = sizeof(*data);
	if (data->mode != old_data.mode) {
		memset(&old_data, 0, sizeof(old_data));
		write_lcd(HD44780_CLEAR_RAM, BACKPACK_CMD);
		usleep_range(2000, 2500);
		switch (data->mode) {
		case DISPLAY_MODE_CLOCK:
		case DISPLAY_MODE_PLAYBACK_TIME:
		case DISPLAY_MODE_DATE:
			write_lcd(HD44780_CGRA | 0x38, BACKPACK_CMD);
			if (rows != 2)
				write_buf_lcd(cgram_4l_chars[7], 8);
			else
				write_buf_lcd(cgram_2l_chars[7], 8);
			break;
		case DISPLAY_MODE_CHANNEL:
			write_lcd(HD44780_CGRA | 0x38, BACKPACK_CMD);
			if (rows != 2)
				write_buf_lcd(cgram_ellipsis_chars, 8);
			else
				write_buf_lcd(cgram_2l_chars[7], 8);
			break;
		case DISPLAY_MODE_TITLE:
			write_lcd(HD44780_CGRA | 0x38, BACKPACK_CMD);
			write_buf_lcd(cgram_ellipsis_chars, 8);
			break;
		case DISPLAY_MODE_TEMPERATURE:
			if (rows == 2) {
				write_lcd(HD44780_CGRA | 0x38, BACKPACK_CMD);
				write_buf_lcd(cgram_2l_chars[7], 8);
			}
			break;
		default:
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
		write_lcd(HD44780_CLEAR_RAM, BACKPACK_CMD);
		usleep_range(2000, 2500);
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

static void set_xy(unsigned short row, unsigned char column)
{
	unsigned char offset = 0x00;
	if (column < columns && row < rows) {
		switch (rows) {
		case 2:
			if (row == 1)
				offset = 0x40;
			break;
		case 4:
			if (row % 2)
				offset = 0x40;
			if (row >= 2)
				offset += columns;
			break;
		};

		offset += column;
		write_lcd(HD44780_DDRA | offset, BACKPACK_CMD);
	}
}

static void print_2l_char(unsigned short ch, unsigned char pos, unsigned char is_clock)
{
	unsigned char i;
	if (ch >= 10) {
		pr_dbg2("print_2l_char: ch = %d, out of bounds\n", ch);
		return;
	}
	pos *= 3;
	if (is_clock) {
		if (pos >= 6)
			pos++;
		if (pos >= 12)
			pos++;
	}
	for (i = 0; i < 2; i++) {
		set_xy(i, pos);
		write_buf_lcd(big_2l_chars[ch] + (i * 3), 3);
	}
}

static void print_4l_char(unsigned short ch, unsigned char pos, unsigned char is_clock)
{
	unsigned char i;
	if (ch >= 10) {
		pr_dbg2("print_4l_char: ch = %d, out of bounds\n", ch);
		return;
	}
	pos *= 3;
	if (is_clock) {
		if (pos >= 6)
			pos++;
		if (pos >= 12)
			pos++;
	}
	for (i = 0; i < 3; i++) {
		set_xy(i, pos);
		write_buf_lcd(big_4l_chars[ch] + (i * 3), 3);
	}
}

static void print_colon(unsigned char colon_on, unsigned char print_seconds)
{
	unsigned char dot;
	if (colon_on != old_data.colon_on) {
		if (rows >= 2) {
			dot = colon_on ? big_dot : ' ';
			write_lcd(HD44780_DDRA + 6, BACKPACK_CMD);
			write_lcd(dot, BACKPACK_RS);
			write_lcd(HD44780_DDRA + 0x40 + 6, BACKPACK_CMD);
			write_lcd(dot, BACKPACK_RS);
			if (print_seconds) {
				write_lcd(HD44780_DDRA + 13, BACKPACK_CMD);
				write_lcd(dot, BACKPACK_RS);
				write_lcd(HD44780_DDRA + 0x40 + 13, BACKPACK_CMD);
				write_lcd(dot, BACKPACK_RS);
			}
		} else {
			dot = colon_on ? ':' : ' ';
			write_lcd(HD44780_DDRA + 2, BACKPACK_CMD);
			write_lcd(dot, BACKPACK_RS);
			if (print_seconds) {
				write_lcd(HD44780_DDRA + 5, BACKPACK_CMD);
				write_lcd(dot, BACKPACK_RS);
			}
		}
	}
}

static void print_number(const char *buffer, size_t length, unsigned char start_index, unsigned char is_clock)
{
	unsigned char i;
	if (rows > 2) {
		for (i = 0; i < length; i++)
			print_4l_char(buffer[i] - 0x30, i + start_index, is_clock);
	} else if (rows == 2) {
		for (i = 0; i < length; i++)
			print_2l_char(buffer[i] - 0x30, i + start_index, is_clock);
	} else {
		if (is_clock) {
			if (start_index >= 2)
				start_index++;
			if (start_index >= 4)
				start_index++;
		}
		write_lcd(HD44780_DDRA + start_index, BACKPACK_CMD);
		for (i = 0; i < length; i++)
			write_lcd(buffer[i], BACKPACK_RS);
	}
}

static void print_clock(const struct vfd_display_data *data, unsigned char print_seconds)
{
	char buffer[10];
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE;
	print_seconds &= (dev->dtb_active.display.flags & FLAGS_SHOW_SEC) && columns >= 20;
	print_colon(data->colon_on, print_seconds);
	if (data->time_date.hours != old_data.time_date.hours || force_print) {
		scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
		print_number(buffer, 2, 0, TRUE);
	}
	if (data->time_date.minutes != old_data.time_date.minutes || force_print) {
		scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
		print_number(buffer, 2, 2, TRUE);
	}
	if (print_seconds && (data->time_date.seconds != old_data.time_date.seconds || force_print)) {
		scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
		print_number(buffer, 2, 4, TRUE);
	}
	if (rows >= 2 && columns >= 20 && (data->time_date.day != old_data.time_date.day ||
		data->time_date.month != old_data.time_date.month || data->time_date.year != old_data.time_date.year)) {
		size_t len;
		len = scnprintf(buffer, sizeof(buffer), "%s %02d", months[data->time_date.month], data->time_date.day);
		set_xy(0, columns - len);
		write_buf_lcd(buffer, len);
		len = scnprintf(buffer, sizeof(buffer), "%d", data->time_date.year);
		set_xy(1, columns - len);
		write_buf_lcd(buffer, len);
		if (rows >= 3) {
			len = strlen(days[data->time_date.day_of_week]);
			set_xy(2, columns - len);
			write_buf_lcd(days[data->time_date.day_of_week], len);
		}
		if (rows >= 4) {
			set_xy(3, 0);
			write_buf_lcd(data->string_main, min(strlen(data->string_main), (size_t)columns));
		}
	}
}

static void print_channel(const struct vfd_display_data *data)
{
	size_t len, max_len = (columns / 3);
	char buffer[81];
	if (rows >= 2) {
		len = scnprintf(buffer, sizeof(buffer), "%d", data->channel_data.channel);
		if (len > max_len) {
			print_number(buffer + (len - max_len), max_len, 0, FALSE);
			len = max_len;
		} else
			print_number(buffer, len, 0, FALSE);
		len *= 3;
		if (columns - len >= 7) {
			unsigned char row = 0;
			if (rows > 2 && data->channel_data.channel_count > 0) {
				len = scnprintf(buffer, sizeof(buffer), "/%d", data->channel_data.channel_count);
				if (len <= 7) {
					set_xy(row++, columns - len);
					write_buf_lcd(buffer, len);
				}
			}
			if (data->time_date.hours < 24 && data->time_secondary.hours < 24) {
				len = scnprintf(buffer, sizeof(buffer), "%02d:%02d-", data->time_date.hours, data->time_date.minutes);
				set_xy(row++, columns - len);
				write_buf_lcd(buffer, len);
				len = scnprintf(buffer, sizeof(buffer), "%02d:%02d ", data->time_secondary.hours, data->time_secondary.minutes);
				set_xy(row++, columns - len);
				write_buf_lcd(buffer, len);
			}
			if (rows >= 4) {
				set_xy(3, 0);
				len = scnprintf(buffer, sizeof(buffer), "%s", data->string_main);
				if (len > columns) {
					len = columns;
					buffer[len - 1] = CUSTOM_ELLIPSIS;
				}
				write_buf_lcd(buffer, len);
			}
		}
	} else {
		write_lcd(HD44780_DDRA, BACKPACK_CMD);
		len = scnprintf(buffer, sizeof(buffer), "%d/%d", data->channel_data.channel, data->channel_data.channel_count);
		if (len > columns) {
			len = columns;
			buffer[len - 1] = CUSTOM_ELLIPSIS;
		}
		write_buf_lcd(buffer, len);
	}
}

static void print_playback_time(const struct vfd_display_data *data)
{
	char buffer[21];
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE || data->time_date.hours != old_data.time_date.hours;
	print_colon(data->colon_on, FALSE);
	if (data->time_date.hours > 0) {
		if (data->time_date.hours != old_data.time_date.hours || force_print) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.hours);
			print_number(buffer, 2, 0, TRUE);
			if (rows >= 3)
				write_lcd('H', BACKPACK_RS);
		}
		if (data->time_date.minutes != old_data.time_date.minutes || force_print) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
			print_number(buffer, 2, 2, TRUE);
			write_lcd('M', BACKPACK_RS);
		}
	} else {
		if (data->time_date.minutes != old_data.time_date.minutes || force_print) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.minutes);
			print_number(buffer, 2, 0, TRUE);
			if (rows >= 3)
				write_lcd('M', BACKPACK_RS);
		}
		if (data->time_date.seconds != old_data.time_date.seconds || force_print) {
			scnprintf(buffer, sizeof(buffer), "%02d", data->time_date.seconds);
			print_number(buffer, 2, 2, TRUE);
			write_lcd('S', BACKPACK_RS);
		}
	}

	if (rows >= 2 && columns >= 20) {
		size_t len = 0;
		unsigned char mins = data->time_secondary.minutes + ((data->time_secondary.seconds >= 30) ? 1 : 0);
		unsigned char hours = data->time_secondary.hours;
		if (mins >= 60) {
			mins %= 60;
			hours++;
		}
		len = scnprintf(buffer, sizeof(buffer), "/%02d:%02d", hours, mins);
		set_xy(0, columns - len);
		write_buf_lcd(buffer, len);
	}

	if (rows >= 4) {
		set_xy(3, 0);
		write_buf_lcd(data->string_main, min(strlen(data->string_main), (size_t)columns));
	}
}

static void print_title(const struct vfd_display_data *data)
{
	size_t len;
	char buffer[81];
	if (rows >= 2) {
		unsigned char i, row, max_len = columns * (rows - 1);
		set_xy(0, 0);
		write_buf_lcd(data->string_secondary, min(strlen(data->string_secondary), (size_t)columns));

		len = scnprintf(buffer, sizeof(buffer), "%s", data->string_main);
		if (len > max_len) {
			len = max_len;
			buffer[len - 1] = CUSTOM_ELLIPSIS;
		}
		for (i = 0, row = 1; i < len; i += columns, row++) {
			set_xy(row, 0);
			write_buf_lcd(buffer + i, min(len - i, (size_t)columns));
		}
	} else {
		set_xy(0, 0);
		len = scnprintf(buffer, sizeof(buffer), "%s", data->string_main);
		if (len > columns) {
			len = columns;
			buffer[len - 1] = CUSTOM_ELLIPSIS;
		}
		write_buf_lcd(buffer, len);
	}
}

static void print_date(const struct vfd_display_data *data)
{
	char buffer[21];
	size_t len;
	unsigned char i = 0;
	unsigned char force_print = old_data.mode == DISPLAY_MODE_NONE || data->time_date.month != old_data.time_date.month || data->time_date.day != old_data.time_date.day;
	if (force_print) {
		if (rows >= 2) {
			if (data->time_secondary._reserved)
				scnprintf(buffer, sizeof(buffer), "%02d%02d", data->time_date.month + 1, data->time_date.day);
			else
				scnprintf(buffer, sizeof(buffer), "%02d%02d", data->time_date.day, data->time_date.month + 1);
			for (i = 0; i < min(rows, (unsigned char)3); i++) {
				set_xy(i, 6);
				write_lcd('|', BACKPACK_RS);
			}
			print_number(buffer, 4, 0, 1);
			if (rows >= 4) {
				len = scnprintf(buffer, sizeof(buffer), "%04d, %s, %s", data->time_date.year, months[data->time_date.month], days[data->time_date.day_of_week]);
				set_xy(3, 0);
				write_buf_lcd(buffer, min((size_t)columns, len));
			}
		} else {
			if (data->time_secondary._reserved)
				len = scnprintf(buffer, sizeof(buffer), "%02d/%02d", data->time_date.month + 1, data->time_date.day);
			else
				len = scnprintf(buffer, sizeof(buffer), "%02d/%02d", data->time_date.day, data->time_date.month + 1);
			if (columns <= 8)
				len += scnprintf(buffer + len, sizeof(buffer) - len, "/%02d", data->time_date.year % 100);
			else
				len += scnprintf(buffer + len, sizeof(buffer) - len, "/%04d", data->time_date.year);
			set_xy(0, 0);
			write_buf_lcd(buffer, min((size_t)columns, len));
		}
	}
}

static void print_temperature(const struct vfd_display_data *data)
{
	unsigned char i;
	char buffer[10];
	if (data->temperature != old_data.temperature) {
		size_t len = scnprintf(buffer, sizeof(buffer), "%d", data->temperature);
		print_number(buffer, len, 0, FALSE);
		if (rows >= 2) {
			len *= 3;
			set_xy(0, len);
			write_lcd('o', BACKPACK_RS);
			len++;
			if (rows > 2) {
				for (i = 0; i < 3; i++) {
					set_xy(i, len);
					write_buf_lcd(big_4l_chars[10] + (i * 3), 3);
				}
			}
			else {
				for (i = 0; i < 2; i++) {
					set_xy(i, len);
					write_buf_lcd(big_2l_chars[10] + (i * 3), 3);
				}
			}
		} else {
			size_t len = scnprintf(buffer, sizeof(buffer), "%cC", 0xDF);
			write_buf_lcd(buffer, len);
		}
	}
}
