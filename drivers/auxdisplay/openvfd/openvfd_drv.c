/*
 * Open VFD Driver
 *
 * Copyright (C) 2018 Arthur Liberman (arthur_liberman (at) hotmail.com)
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/leds.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "openvfd_drv.h"
#include "controllers/controller_list.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static struct early_suspend openvfd_early_suspend;
#elif CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend openvfd_early_suspend;
#endif

unsigned char vfd_display_auto_power = 1;

static struct vfd_platform_data *pdata = NULL;
struct kp {
	struct led_classdev cdev;
};

static struct kp *kp;

static struct controller_interface *controller = NULL;
static struct mutex mutex;

/****************************************************************
 *	Function Name:		FD628_GetKey
 *	Description:		Read key code value
 *	Parameters:		void
 *	Return value:		INT32U returns the key value
 **************************************************************************************************************************************
Key value encoding
	| 0	| 0	| 0	| 0	| 0	| 0	| KS10	| KS9	| KS8	| KS7	| KS6	| KS5	| KS4	| KS3	| KS2	| KS1	|
KEYI1 	| bit15	| bit14	| bit13	| bit12	| bit11	| bit10	| bit9	| bit8	| bit7	| bit6	| bit5	| bit4	| bit3	| bit2	| bit1	| bit0	|
KEYI2 	| bit31	| bit30	| bit29	| bit28	| bit27	| bit26	| bit25	| bit24	| bit23	| bit22	| bit21	| bit20	| bit19	| bit18	| bit17	| bit16	|
***************************************************************************************************************************************/
static u_int32 FD628_GetKey(struct vfd_dev *dev)
{
	u_int8 i, keyDataBytes[5];
	u_int32 FD628_KeyData = 0;
	mutex_lock(&mutex);
	controller->read_data(keyDataBytes, sizeof(keyDataBytes));
	mutex_unlock(&mutex);
	for (i = 0; i != 5; i++) {			/* Pack 5 bytes of key code values into 2 words */
		if (keyDataBytes[i] & 0x01)
			FD628_KeyData |= (0x00000001 << i * 2);
		if (keyDataBytes[i] & 0x02)
			FD628_KeyData |= (0x00010000 << i * 2);
		if (keyDataBytes[i] & 0x08)
			FD628_KeyData |= (0x00000002 << i * 2);
		if (keyDataBytes[i] & 0x10)
			FD628_KeyData |= (0x00020000 << i * 2);
	}

	return (FD628_KeyData);
}

static void unlocked_set_power(unsigned char state)
{
	if (vfd_display_auto_power && controller) {
		controller->set_power(state);
		if (state && pdata)
			controller->set_brightness_level(pdata->dev->brightness);
	}
}

static void set_power(unsigned char state)
{
	mutex_lock(&mutex);
	unlocked_set_power(state);
	mutex_unlock(&mutex);
}

static void init_controller(struct vfd_dev *dev)
{
	struct controller_interface *temp_ctlr;

	switch (dev->dtb_active.display.controller) {
	case CONTROLLER_FD628:
	case CONTROLLER_FD620:
	case CONTROLLER_TM1618:
		pr_dbg2("Select FD628 controller\n");
		temp_ctlr = init_fd628(dev);
		break;
	case CONTROLLER_HBS658:
		pr_dbg2("Select HBS658 controller\n");
		temp_ctlr = init_fd628(dev);
		break;
	case CONTROLLER_FD650:
		pr_dbg2("Select FD650 controller\n");
		temp_ctlr = init_fd650(dev);
		break;
	case CONTROLLER_FD655:
		pr_dbg2("Select FD655 controller\n");
		temp_ctlr = init_fd650(dev);
		break;
	case CONTROLLER_FD6551:
		pr_dbg2("Select FD6551 controller\n");
		temp_ctlr = init_fd650(dev);
		break;
	case CONTROLLER_IL3829:
		pr_dbg2("Select IL3829 controller\n");
		temp_ctlr = init_il3829(dev);
		break;
	case CONTROLLER_PCD8544:
		pr_dbg2("Select PCD8544 controller\n");
		temp_ctlr = init_pcd8544(dev);
		break;
	case CONTROLLER_SH1106:
		pr_dbg2("Select SH1106 controller\n");
		temp_ctlr = init_ssd1306(dev);
		break;
	case CONTROLLER_SSD1306:
		pr_dbg2("Select SSD1306 controller\n");
		temp_ctlr = init_ssd1306(dev);
		break;
	case CONTROLLER_HD44780:
		pr_dbg2("Select HD44780 controller\n");
		temp_ctlr = init_hd47780(dev);
		break;
	default:
		pr_dbg2("Select Dummy controller\n");
		temp_ctlr = init_dummy(dev);
		break;
	}

	if (controller != temp_ctlr) {
		unlocked_set_power(0);
		controller = temp_ctlr;
		if (!controller->init()) {
			pr_dbg2("Failed to initialize the controller, reverting to Dummy controller\n");
			controller = init_dummy(dev);
			dev->dtb_active.display.controller = CONTROLLER_7S_MAX;
		}
	}
}

static int openvfd_dev_open(struct inode *inode, struct file *file)
{
	struct vfd_dev *dev = NULL;
	file->private_data = pdata->dev;
	dev = file->private_data;
	memset(dev->wbuf, 0x00, sizeof(dev->wbuf));
	set_power(1);
	pr_dbg("openvfd_dev_open now.............................\r\n");
	return 0;
}

static int openvfd_dev_release(struct inode *inode, struct file *file)
{
	set_power(0);
	file->private_data = NULL;
	pr_dbg("succes to close  openvfd_dev.............\n");
	return 0;
}

static ssize_t openvfd_dev_read(struct file *filp, char __user * buf,
				  size_t count, loff_t * f_pos)
{
	__u32 disk = 0;
	struct vfd_dev *dev = filp->private_data;
	__u32 diskvalue = 0;
	int ret = 0;
	int rbuf[2] = { 0 };
	//pr_dbg("start read keyboard value...............\r\n");
	if (dev->Keyboard_diskstatus == 1) {
		diskvalue = FD628_GetKey(dev);
		if (diskvalue == 0)
			return 0;
	}
	dev->key_respond_status = 0;
	rbuf[1] = dev->key_fg;
	if (dev->key_fg)
		rbuf[0] = disk;
	else
		rbuf[0] = diskvalue;
	//pr_dbg("Keyboard value:%d\n, status : %d\n",rbuf[0],rbuf[1]);
	ret = copy_to_user(buf, rbuf, sizeof(rbuf));
	if (ret == 0)
		return sizeof(rbuf);
	else
		return ret;
}

/**
 * @param buf: Incoming LED codes.
 * 		  [0]	Display indicators mask (wifi, eth, usb, etc.)
 * 		  [1-4]	7 segment characters, to be displayed left to right.
 * @return
 */
static ssize_t openvfd_dev_write(struct file *filp, const char __user * buf,
				   size_t count, loff_t * f_pos)
{
	ssize_t status = 0;
	unsigned long missing;
	static struct vfd_display_data data;

	if (count == sizeof(data)) {
		missing = copy_from_user(&data, buf, count);
		if (missing == 0 && count > 0) {
			mutex_lock(&mutex);
			if (controller->write_display_data(&data))
				pr_dbg("openvfd_dev_write count : %ld\n", count);
			else {
				status = -1;
				pr_error("openvfd_dev_write failed to write %ld bytes (display_data)\n", count);
			}
			mutex_unlock(&mutex);
		}
	} else if (count > 0) {
		unsigned char *raw_data;
		pr_dbg2("openvfd_dev_write: count = %ld, sizeof(data) = %ld\n", count, sizeof(data));
		raw_data = kzalloc(count, GFP_KERNEL);
		if (raw_data) {
			missing = copy_from_user(raw_data, buf, count);
			mutex_lock(&mutex);
			if (controller->write_data((unsigned char*)raw_data, count))
				pr_dbg("openvfd_dev_write count : %ld\n", count);
			else {
				status = -1;
				pr_error("openvfd_dev_write failed to write %ld bytes (raw_data)\n", count);
			}
			mutex_unlock(&mutex);
			kfree(raw_data);
		}
		else {
			status = -1;
			pr_error("openvfd_dev_write failed to allocate %ld bytes (raw_data)\n", count);
		}
	}

	return status;
}

static int set_display_brightness(struct vfd_dev *dev, u_int8 new_brightness)
{
	return controller->set_brightness_level(new_brightness);
}

static void set_display_type(struct vfd_dev *dev, int new_display_type)
{
	memcpy(&dev->dtb_active.display, &new_display_type, sizeof(struct vfd_display));
	init_controller(dev);
}

static long openvfd_dev_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int err = 0, ret = 0, temp = 0;
	struct vfd_dev *dev;
	__u8 val = 1;
	__u8 temp_chars_order[sizeof(dev->dtb_active.dat_index)];
	dev = filp->private_data;

	if (_IOC_TYPE(cmd) != VFD_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= VFD_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
#else
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
#endif
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0))
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
#else
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
#endif
	if (err)
		return -EFAULT;

	mutex_lock(&mutex);
	switch (cmd) {
	case VFD_IOC_USE_DTB_CONFIG:
		dev->dtb_active = dev->dtb_default;
		init_controller(dev);
		break;
	case VFD_IOC_GDISPLAY_TYPE:
		memcpy(&temp, &dev->dtb_active.display, sizeof(int));
		ret = __put_user(temp, (int __user *)arg);
		break;
	case VFD_IOC_SDISPLAY_TYPE:
		ret = __get_user(temp, (int __user *)arg);
		if (!ret)
			set_display_type(dev, temp);
		break;
	case VFD_IOC_SCHARS_ORDER:
		ret = __copy_from_user(temp_chars_order, (__u8 __user *)arg, sizeof(dev->dtb_active.dat_index));
		if (!ret)
			memcpy(dev->dtb_active.dat_index, temp_chars_order, sizeof(dev->dtb_active.dat_index));
		break;
	case VFD_IOC_SMODE:	/* Set: arg points to the value */
		ret = __get_user(dev->mode, (int __user *)arg);
		//FD628_SET_DISPLAY_MODE(dev->mode, dev);
		break;
	case VFD_IOC_GMODE:	/* Get: arg is pointer to result */
		ret = __put_user(dev->mode, (int __user *)arg);
		break;
	case VFD_IOC_GVER:
		ret =
			copy_to_user((unsigned char __user *)arg,
				 OPENVFD_DRIVER_VERSION,
				 sizeof(OPENVFD_DRIVER_VERSION));
		break;
	case VFD_IOC_SBRIGHT:
		ret = __get_user(temp, (int __user *)arg);
		if (!ret && !set_display_brightness(dev, (u_int8)temp))
			ret = -ERANGE;
		break;
	case VFD_IOC_GBRIGHT:
		ret = __put_user(dev->brightness, (int __user *)arg);
		break;
	case VFD_IOC_POWER:
		ret = __get_user(val, (int __user *)arg);
		controller->set_power(val);
		break;
	case VFD_IOC_STATUS_LED:
		ret = __get_user(dev->status_led_mask, (int __user *)arg);
		break;
	default:		/* redundant, as cmd was checked against MAXNR */
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&mutex);
	return ret;
}

static unsigned int openvfd_dev_poll(struct file *filp, poll_table * wait)
{
	unsigned int mask = 0;
	struct vfd_dev *dev = filp->private_data;
	poll_wait(filp, &dev->kb_waitq, wait);
	if (dev->key_respond_status)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static struct file_operations openvfd_fops = {
	.owner = THIS_MODULE,
	.open = openvfd_dev_open,
	.release = openvfd_dev_release,
	.read = openvfd_dev_read,
	.write = openvfd_dev_write,
	.unlocked_ioctl = openvfd_dev_ioctl,
	.compat_ioctl = openvfd_dev_ioctl,
	.poll = openvfd_dev_poll,
};

static struct miscdevice openvfd_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEV_NAME,
	.fops = &openvfd_fops,
};

static int register_openvfd_driver(void)
{
	int ret = 0;
	ret = misc_register(&openvfd_device);
	if (ret)
		pr_dbg("%s: failed to add openvfd module\n", __func__);
	else
		pr_dbg("%s: Succeeded to add openvfd module \n", __func__);
	return ret;
}

static void deregister_openvfd_driver(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
	int ret = 0;
	ret = misc_deregister(&openvfd_device);
	if (ret)
		pr_dbg("%s: failed to deregister openvfd module\n", __func__);
	else
		pr_dbg("%s: Succeeded to deregister openvfd module \n", __func__);
#else
	misc_deregister(&openvfd_device);
#endif
}


static void openvfd_brightness_set(struct led_classdev *cdev,
	enum led_brightness brightness)
{
	pr_info("brightness = %d\n", brightness);

	if(pdata == NULL)
		return;
}

static int led_cmd_ioc = 0;

static ssize_t led_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	*buf = '\0';

	switch(led_cmd_ioc) {
		case VFD_IOC_GMODE:
			ret = scnprintf(buf, PAGE_SIZE, "%d", pdata->dev->mode);
			break;
		case VFD_IOC_GBRIGHT:
			ret = scnprintf(buf, PAGE_SIZE, "%d", pdata->dev->brightness);
			break;
		case VFD_IOC_GVER:
			ret = scnprintf(buf, PAGE_SIZE, "%s", OPENVFD_DRIVER_VERSION);
			break;
		case VFD_IOC_GDISPLAY_TYPE:
			ret = scnprintf(buf, PAGE_SIZE, "0x%02X%02X%02X%02X", pdata->dev->dtb_active.display.reserved, pdata->dev->dtb_active.display.flags,
				pdata->dev->dtb_active.display.controller, pdata->dev->dtb_active.display.type);
			break;
	}

	led_cmd_ioc = 0;
	return ret;
}

static ssize_t led_cmd_store(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct vfd_dev *dev = pdata->dev;
	int cmd, temp;
	led_cmd_ioc = 0;

	if (size < 2*sizeof(int))
		return -EFAULT;
	memcpy(&cmd, buf, sizeof(int));
	if (_IOC_TYPE(cmd) != VFD_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= VFD_IOC_MAXNR)
		return -ENOTTY;

	buf += sizeof(int);
	memcpy(&temp, buf, sizeof(int));
	mutex_lock(&mutex);
	switch (cmd) {
		case VFD_IOC_SMODE:
			dev->mode = (u_int8)temp;
			//FD628_SET_DISPLAY_MODE(dev->mode, dev);
			break;
		case VFD_IOC_SBRIGHT:
			if (!set_display_brightness(dev, (u_int8)temp))
				size = -ERANGE;
			break;
		case VFD_IOC_POWER:
			controller->set_power(temp);
			break;
		case VFD_IOC_STATUS_LED:
			dev->status_led_mask = (u_int8)temp;
			break;
		case VFD_IOC_SDISPLAY_TYPE:
			set_display_type(dev, temp);
			break;
		case VFD_IOC_SCHARS_ORDER:
			if (size >= sizeof(dev->dtb_active.dat_index)+sizeof(int))
				memcpy(dev->dtb_active.dat_index, buf, sizeof(dev->dtb_active.dat_index));
			else
				size = -EFAULT;
			break;
		case VFD_IOC_USE_DTB_CONFIG:
			pdata->dev->dtb_active = pdata->dev->dtb_default;
			init_controller(dev);
			break;
		case VFD_IOC_GMODE:
		case VFD_IOC_GBRIGHT:
		case VFD_IOC_GVER:
		case VFD_IOC_GDISPLAY_TYPE:
			led_cmd_ioc = cmd;
			break;
	}

	mutex_unlock(&mutex);
	return size;
}

static ssize_t led_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "led status is 0x%x\n", pdata->dev->status_led_mask);
}

static ssize_t led_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	mutex_lock(&mutex);
	controller->set_icon(buf, 1);
	mutex_unlock(&mutex);
	return size;
}

static ssize_t led_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "led status is 0x%x\n", pdata->dev->status_led_mask);
}

static ssize_t led_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	mutex_lock(&mutex);
	controller->set_icon(buf, 0);
	mutex_unlock(&mutex);
	return size;
}

static DEVICE_ATTR(led_cmd , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, led_cmd_show , led_cmd_store);
static DEVICE_ATTR(led_on , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, led_on_show , led_on_store);
static DEVICE_ATTR(led_off , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, led_off_show , led_off_store);

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)
static void openvfd_suspend(struct early_suspend *h)
{
	pr_info("%s!\n", __func__);
	set_power(0);
}

static void openvfd_resume(struct early_suspend *h)
{
	pr_info("%s!\n", __func__);
	set_power(1);
}
#endif

char *vfd_gpio_chip_name = NULL;
unsigned int vfd_gpio_clk[3];
unsigned int vfd_gpio_dat[3];
unsigned int vfd_gpio_stb[3];
unsigned int vfd_gpio0[3] = { 0x00, 0x00, 0xFF };
unsigned int vfd_gpio1[3] = { 0x00, 0x00, 0xFF };
unsigned int vfd_gpio2[3] = { 0x00, 0x00, 0xFF };
unsigned int vfd_gpio3[3] = { 0x00, 0x00, 0xFF };
unsigned int vfd_gpio_protocol[2] = { 0x00, 0x00 };
unsigned int vfd_chars[7] = { 0, 1, 2, 3, 4, 5, 6 };
unsigned int vfd_dot_bits[8] = { 0, 1, 2, 3, 4, 5, 6, 0 };
unsigned int vfd_display_type[4] = { 0x00, 0x00, 0x00, 0x00 };
int vfd_gpio_clk_argc = 0;
int vfd_gpio_dat_argc = 0;
int vfd_gpio_stb_argc = 0;
int vfd_gpio0_argc = 3;
int vfd_gpio1_argc = 3;
int vfd_gpio2_argc = 3;
int vfd_gpio3_argc = 3;
int vfd_gpio_protocol_argc = 2;
int vfd_chars_argc = 0;
int vfd_dot_bits_argc = 0;
int vfd_display_type_argc = 0;

module_param(vfd_gpio_chip_name, charp, 0000);
module_param_array(vfd_gpio_clk, uint, &vfd_gpio_clk_argc, 0000);
module_param_array(vfd_gpio_dat, uint, &vfd_gpio_dat_argc, 0000);
module_param_array(vfd_gpio_stb, uint, &vfd_gpio_stb_argc, 0000);
module_param_array(vfd_gpio0, uint, &vfd_gpio0_argc, 0000);
module_param_array(vfd_gpio1, uint, &vfd_gpio1_argc, 0000);
module_param_array(vfd_gpio2, uint, &vfd_gpio2_argc, 0000);
module_param_array(vfd_gpio3, uint, &vfd_gpio3_argc, 0000);
module_param_array(vfd_gpio_protocol, uint, &vfd_gpio_protocol_argc, 0000);
module_param_array(vfd_chars, uint, &vfd_chars_argc, 0000);
module_param_array(vfd_dot_bits, uint, &vfd_dot_bits_argc, 0000);
module_param_array(vfd_display_type, uint, &vfd_display_type_argc, 0000);
module_param(vfd_display_auto_power, byte, 0000);

static void print_param_debug(const char *label, int argc, unsigned int param[])
{
	int i, len = 0;
	char buffer[1024];
	len = scnprintf(buffer, sizeof(buffer), "%s", label);
	if (argc)
		for (i = 0; i < argc; i++)
			len += scnprintf(buffer + len, sizeof(buffer), "#%d = 0x%02X; ", i, param[i]);
	else
		len += scnprintf(buffer + len, sizeof(buffer), "Empty.");
	pr_dbg2("%s\n", buffer);
}

static int is_right_chip(struct gpio_chip *chip, void *data)
{
	pr_dbg("is_right_chip %s | %s | %d\n", chip->label, (char*)data, strcmp(data, chip->label));
	if (strcmp(data, chip->label) == 0)
		return 1;
	return 0;
}

static int get_chip_pin_number(const unsigned int gpio[])
{
	int pin = -1;
	struct gpio_chip *chip;
	const char *bank_name = vfd_gpio_chip_name;
	if (!bank_name && gpio[0] < 6) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
		const char *pin_banks[] = { "banks", "ao-bank", "gpio0", "gpio1", "gpio2", "gpio3" };
#else
		const char *pin_banks[] = { "periphs-banks", "aobus-banks", "gpio0", "gpio1", "gpio2", "gpio3" };
#endif
		bank_name = pin_banks[gpio[0]];
	}

	if (bank_name) {
		chip = gpiochip_find((char *)bank_name, is_right_chip);
		if (chip) {
			if (chip->ngpio > gpio[1])
				pin = chip->base + gpio[1];
			pr_dbg2("\"%s\" chip found.\tbase = %d, pin count = %d, pin = %d, offset = %d\n", bank_name, chip->base, chip->ngpio, gpio[1], pin);
		} else {
			pr_dbg2("\"%s\" chip was not found\n", bank_name);
		}
	} else {
		pr_dbg2("gpiochip name was not provided or gpiochip index %d is out of bounds\n", gpio[0]);
	}

	return pin;
}

int evaluate_pin(const char *name, const unsigned int *vfd_arg, struct vfd_pin *pin, unsigned char enable_skip_evaluation)
{
	int ret = 0;
	if (enable_skip_evaluation && vfd_arg[2] == 0xFF) {
		pin->pin = -2;
		pr_dbg2("Skipping %s evaluation (0xFF)\n", name);
	}
	else if ((ret = pin->pin = get_chip_pin_number(vfd_arg)) >= 0)
		pin->flags.value = vfd_arg[2];
	else
		pr_error("Could not get pin number for %s\n", name);
	return ret;
}

char gpio_chip_names[1024] = { 0 };

static int enum_gpio_chips(struct gpio_chip *chip, void *data)
{
	static unsigned char first_iteration = 1;
	const char *sep = ", ";
	size_t str_len = strlen(gpio_chip_names);
	if (first_iteration)
		sep = "";
	scnprintf(gpio_chip_names + str_len, sizeof(gpio_chip_names) - str_len, "%s%s", sep, chip->label);
	first_iteration = 0;
	return 0;
}

static int verify_module_params(struct vfd_dev *dev)
{
	int ret = (vfd_gpio_clk_argc == 3 && vfd_gpio_dat_argc == 3 && vfd_gpio_stb_argc == 3 &&
			vfd_chars_argc >= 5 && vfd_dot_bits_argc >= 7 && vfd_display_type_argc == 4) ? 1 : -1;
	u_int8 allow_skip_clk_dat_evaluation = vfd_gpio_protocol[0] > 0;

	print_param_debug("vfd_gpio_clk:\t\t", vfd_gpio_clk_argc, vfd_gpio_clk);
	print_param_debug("vfd_gpio_dat:\t\t", vfd_gpio_dat_argc, vfd_gpio_dat);
	print_param_debug("vfd_gpio_stb:\t\t", vfd_gpio_stb_argc, vfd_gpio_stb);
	print_param_debug("vfd_gpio0:\t\t", vfd_gpio0_argc, vfd_gpio0);
	print_param_debug("vfd_gpio1:\t\t", vfd_gpio1_argc, vfd_gpio1);
	print_param_debug("vfd_gpio2:\t\t", vfd_gpio2_argc, vfd_gpio2);
	print_param_debug("vfd_gpio3:\t\t", vfd_gpio3_argc, vfd_gpio3);
	print_param_debug("vfd_gpio_protocol:\t", vfd_gpio_protocol_argc, vfd_gpio_protocol);
	print_param_debug("vfd_chars:\t\t", vfd_chars_argc, vfd_chars);
	print_param_debug("vfd_dot_bits:\t\t", vfd_dot_bits_argc, vfd_dot_bits);
	print_param_debug("vfd_display_type:\t", vfd_display_type_argc, vfd_display_type);

	dev->hw_protocol.protocol = (u_int8)vfd_gpio_protocol[0];
	dev->hw_protocol.device_id = (u_int8)vfd_gpio_protocol[1];

	gpiochip_find(NULL, enum_gpio_chips);
	pr_dbg2("Detected gpio chips:\t%s.\n", gpio_chip_names);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio_clk", vfd_gpio_clk, &dev->clk_pin, allow_skip_clk_dat_evaluation);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio_dat", vfd_gpio_dat, &dev->dat_pin, allow_skip_clk_dat_evaluation);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio_stb", vfd_gpio_stb, &dev->stb_pin, 1);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio0", vfd_gpio0, &dev->gpio0_pin, 1);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio1", vfd_gpio1, &dev->gpio1_pin, 1);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio2", vfd_gpio2, &dev->gpio2_pin, 1);
	if (ret >= 0)
		ret = evaluate_pin("vfd_gpio3", vfd_gpio3, &dev->gpio3_pin, 1);

	if (ret >= 0) {
		int i;
		for (i = 0; i < 7; i++)
			dev->dtb_active.dat_index[i] = (u_int8)vfd_chars[i];
		for (i = 0; i < 8; i++)
			dev->dtb_active.led_dots[i] = (u_int8)ledDots[vfd_dot_bits[i]];
		dev->dtb_active.display.type = (u_int8)vfd_display_type[0];
		dev->dtb_active.display.reserved = (u_int8)vfd_display_type[1];
		dev->dtb_active.display.flags = (u_int8)vfd_display_type[2];
		dev->dtb_active.display.controller = (u_int8)vfd_display_type[3];
	}

	return ret >= 0;
}

void get_pin_from_dt(const char *name, const struct platform_device *pdev, struct vfd_pin *pin)
{
	if (of_find_property(pdev->dev.of_node, name, NULL)) {
		pin->pin = of_get_named_gpio_flags(pdev->dev.of_node, name, 0, &pin->flags.value);
		pr_dbg2("%s: pin = %d, flags = 0x%02X\n", name, pin->pin, pin->flags.value);
	} else {
		pin->pin = -2;
		pr_dbg2("%s pin entry not found\n", name);
	}
}

int request_pin(const char *name, struct vfd_pin *pin, unsigned char enable_skip)
{
	int ret = 0;
	pin->flags.bits.is_requested = 0;
	if (!enable_skip || pin->pin != -2) {
		ret = -1;
		if (pin->pin >= 0)
			ret = gpio_request(pin->pin, DEV_NAME);
		if (!ret)
			pin->flags.bits.is_requested = 1;
		else
			pr_error("can't request gpio of %s", name);
	}
	return ret;
}

static int openvfd_driver_probe(struct platform_device *pdev)
{
	int state = -EINVAL;
	struct property *chars_prop = NULL;
	struct property *dot_bits_prop = NULL;
	struct property *display_type_prop = NULL;
	int ret = 0;
	u_int8 allow_skip_clk_dat_request = vfd_gpio_protocol[0] > 0;

	pr_dbg("%s get in\n", __func__);

	if (!pdev->dev.of_node) {
		pr_error("openvfd_driver: pdev->dev.of_node == NULL!\n");
		state = -EINVAL;
		goto get_openvfd_node_fail;
	}

	pdata = kzalloc(sizeof(struct vfd_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_error("platform data is required!\n");
		state = -EINVAL;
		goto get_openvfd_mem_fail;
	}

	pdata->dev = kzalloc(sizeof(*(pdata->dev)), GFP_KERNEL);
	if (!(pdata->dev)) {
		pr_error("platform dev is required!\n");
		goto get_param_mem_fail;
	}

	pdata->dev->mutex = &mutex;
	pr_dbg2("Version: %s\n", OPENVFD_DRIVER_VERSION);
	if (!verify_module_params(pdata->dev)) {
		int i;
		__u8 j;
		pr_error("Failed to verify VFD configuration file, attempt using device tree as fallback.\n");
		get_pin_from_dt(MOD_NAME_CLK, pdev, &pdata->dev->clk_pin);
		get_pin_from_dt(MOD_NAME_DAT, pdev, &pdata->dev->dat_pin);
		get_pin_from_dt(MOD_NAME_STB, pdev, &pdata->dev->stb_pin);
		get_pin_from_dt(MOD_NAME_GPIO0, pdev, &pdata->dev->gpio0_pin);
		get_pin_from_dt(MOD_NAME_GPIO1, pdev, &pdata->dev->gpio1_pin);
		get_pin_from_dt(MOD_NAME_GPIO2, pdev, &pdata->dev->gpio2_pin);
		get_pin_from_dt(MOD_NAME_GPIO3, pdev, &pdata->dev->gpio3_pin);

		chars_prop = of_find_property(pdev->dev.of_node, MOD_NAME_CHARS, NULL);
		if (!chars_prop || !chars_prop->value) {
			pr_error("can't find %s list, falling back to defaults.", MOD_NAME_CHARS);
			chars_prop = NULL;
		}
		else if (chars_prop->length < 5) {
			pr_error("%s list is too short, falling back to defaults.", MOD_NAME_CHARS);
			chars_prop = NULL;
		}

		for (j = 0; j < (sizeof(pdata->dev->dtb_active.dat_index) / sizeof(char)); j++)
			pdata->dev->dtb_active.dat_index[j] = j;
		pr_dbg2("chars_prop = %p\n", chars_prop);
		if (chars_prop) {
			__u8 *c = (__u8*)chars_prop->value;
			const int length = min(chars_prop->length, (int)(sizeof(pdata->dev->dtb_active.dat_index) / sizeof(char)));
			pr_dbg2("chars_prop->length = %d\n", chars_prop->length);
			for (i = 0; i < length; i++) {
				pdata->dev->dtb_active.dat_index[i] = c[i];
				pr_dbg2("char #%d: %d\n", i, c[i]);
			}
		}

		dot_bits_prop = of_find_property(pdev->dev.of_node, MOD_NAME_DOTS, NULL);
		if (!dot_bits_prop || !dot_bits_prop->value) {
			pr_error("can't find %s list, falling back to defaults.", MOD_NAME_DOTS);
			dot_bits_prop = NULL;
		}
		else if (dot_bits_prop->length < LED_DOT_MAX) {
			pr_error("%s list is too short, falling back to defaults.", MOD_NAME_DOTS);
			dot_bits_prop = NULL;
		}

		for (i = 0; i < LED_DOT_MAX; i++)
			pdata->dev->dtb_active.led_dots[i] = ledDots[i];
		pr_dbg2("dot_bits_prop = %p\n", dot_bits_prop);
		if (dot_bits_prop) {
			__u8 *d = (__u8*)dot_bits_prop->value;
			pr_dbg2("dot_bits_prop->length = %d\n", dot_bits_prop->length);
			for (i = 0; i < dot_bits_prop->length; i++) {
				pdata->dev->dtb_active.led_dots[i] = ledDots[d[i]];
				pr_dbg2("dot_bit #%d: %d\n", i, d[i]);
			}
		}

		memset(&pdata->dev->dtb_active.display, 0, sizeof(struct vfd_display));
		display_type_prop = of_find_property(pdev->dev.of_node, MOD_NAME_TYPE, NULL);
		if (display_type_prop && display_type_prop->value)
			of_property_read_u32(pdev->dev.of_node, MOD_NAME_TYPE, (int*)&pdata->dev->dtb_active.display);
		pr_dbg2("display.type = %d, display.controller = %d, pdata->dev->dtb_active.display.flags = 0x%02X\n",
			pdata->dev->dtb_active.display.type, pdata->dev->dtb_active.display.controller, pdata->dev->dtb_active.display.flags);
	}

	if (request_pin("gpio_clk", &pdata->dev->clk_pin, allow_skip_clk_dat_request))
		goto get_gpio_req_fail;
	if (request_pin("dat_pin", &pdata->dev->dat_pin, allow_skip_clk_dat_request))
		goto get_gpio_req_fail;
	if (request_pin("stb_pin", &pdata->dev->stb_pin, 1))
		goto get_gpio_req_fail;
	if (request_pin("gpio0_pin", &pdata->dev->gpio0_pin, 1))
		goto get_gpio_req_fail;
	if (request_pin("gpio1_pin", &pdata->dev->gpio1_pin, 1))
		goto get_gpio_req_fail;
	if (request_pin("gpio2_pin", &pdata->dev->gpio2_pin, 1))
		goto get_gpio_req_fail;
	if (request_pin("gpio3_pin", &pdata->dev->gpio3_pin, 1))
		goto get_gpio_req_fail;

	pdata->dev->dtb_default = pdata->dev->dtb_active;
	pdata->dev->brightness = 0xFF;

	mutex_lock(&mutex);
	register_openvfd_driver();
	kp = kzalloc(sizeof(struct kp) ,  GFP_KERNEL);
	if (!kp) {
		kfree(kp);
		mutex_unlock(&mutex);
		return -ENOMEM;
	}
	kp->cdev.name = DEV_NAME;
	kp->cdev.brightness_set = openvfd_brightness_set;
	ret = led_classdev_register(&pdev->dev, &kp->cdev);
	if (ret < 0) {
		kfree(kp);
		mutex_unlock(&mutex);
		return ret;
	}

	device_create_file(kp->cdev.dev, &dev_attr_led_on);
	device_create_file(kp->cdev.dev, &dev_attr_led_off);
	device_create_file(kp->cdev.dev, &dev_attr_led_cmd);
	init_controller(pdata->dev);
#if 0
	// TODO: Display 'boot' during POST/boot.
	// 'boot'
	//  1 1 0  0 1 1 1  b => 0x7C
	//  1 1 0  0 0 1 1  o => 0x5C
	//  1 0 0  0 1 1 1  t => 0x78
	__u8 data[7];
	data[0] = 0x00;
	data[1] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x7C : 0x67;
	data[2] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x5C : 0x63;
	data[3] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x5C : 0x63;
	data[4] = pdata->dev->dtb_active.display.flags & DISPLAY_TYPE_TRANSPOSED ? 0x78 : 0x47;
	for (i = 0; i < 5; i++) {
		pdata->dev->wbuf[pdata->dev->dtb_active.dat_index[i]] = data[i];
	}
	// Write data in incremental mode
	FD628_WrDisp_AddrINC(0x00, 2*5, pdata->dev);
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)
	openvfd_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	openvfd_early_suspend.suspend = openvfd_suspend;
	openvfd_early_suspend.resume = openvfd_resume;
	register_early_suspend(&openvfd_early_suspend);
#endif

	mutex_unlock(&mutex);
	return 0;

	  get_gpio_req_fail:
	if (pdata->dev->gpio3_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio3_pin.pin);
	if (pdata->dev->gpio2_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio2_pin.pin);
	if (pdata->dev->gpio1_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio1_pin.pin);
	if (pdata->dev->gpio0_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio0_pin.pin);
	if (pdata->dev->stb_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->stb_pin.pin);
	if (pdata->dev->dat_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->dat_pin.pin);
	if (pdata->dev->clk_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->clk_pin.pin);
	  get_param_mem_fail:
	kfree(pdata->dev);
	  get_openvfd_mem_fail:
	kfree(pdata);
	  get_openvfd_node_fail:
	if (pdata && pdata->dev)
		mutex_unlock(&mutex);
	return state;
}

static int openvfd_driver_remove(struct platform_device *pdev)
{
	set_power(0);
#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND)
	unregister_early_suspend(&openvfd_early_suspend);
#endif
	led_classdev_unregister(&kp->cdev);
	deregister_openvfd_driver();
#ifdef CONFIG_OF
	if (pdata->dev->gpio3_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio3_pin.pin);
	if (pdata->dev->gpio2_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio2_pin.pin);
	if (pdata->dev->gpio1_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio1_pin.pin);
	if (pdata->dev->gpio0_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->gpio0_pin.pin);
	if (pdata->dev->stb_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->stb_pin.pin);
	if (pdata->dev->dat_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->dat_pin.pin);
	if (pdata->dev->clk_pin.flags.bits.is_requested)
		gpio_free(pdata->dev->clk_pin.pin);
	kfree(pdata->dev);
	kfree(pdata);
	pdata = NULL;
#endif
	return 0;
}

static void openvfd_driver_shutdown(struct platform_device *dev)
{
	pr_dbg("openvfd_driver_shutdown");
	set_power(0);
}

static int openvfd_driver_suspend(struct platform_device *dev, pm_message_t state)
{
	pr_dbg("openvfd_driver_suspend");
	if (vfd_display_auto_power && controller && controller->power_suspend) {
		controller->power_suspend();
	}
	return 0;
}

static int openvfd_driver_resume(struct platform_device *dev)
{
	pr_dbg("openvfd_driver_resume");
	if (vfd_display_auto_power && controller && controller->power_resume) {
		controller->power_resume();
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id openvfd_dt_match[] = {
	{.compatible = "open,vfd",},
	{},
};
#else
#define openvfd_dt_match NULL
#endif

static struct platform_driver openvfd_driver = {
	.probe = openvfd_driver_probe,
	.remove = openvfd_driver_remove,
	.suspend = openvfd_driver_suspend,
	.shutdown = openvfd_driver_shutdown,
	.resume = openvfd_driver_resume,
	.driver = {
		   .name = DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = openvfd_dt_match,
		   },
};

static int __init openvfd_driver_init(void)
{
	pr_dbg("OpenVFD Driver init.\n");
	mutex_init(&mutex);
	return platform_driver_register(&openvfd_driver);
}

static void __exit openvfd_driver_exit(void)
{
	pr_dbg("OpenVFD Driver exit.\n");
	mutex_destroy(&mutex);
	platform_driver_unregister(&openvfd_driver);
}

module_init(openvfd_driver_init);
module_exit(openvfd_driver_exit);

MODULE_AUTHOR("Arthur Liberman");
MODULE_DESCRIPTION("OpenVFD Driver");
MODULE_LICENSE("GPL");
