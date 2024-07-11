# SPDX-License-Identifier: GPL-2.0-only
################################################################################
#
# r8168 is the Linux device driver released for Realtek Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2024 Realtek Semiconductor Corp. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################

################################################################################
#  This product is covered by one or more of the following patents:
#  US6,570,884, US6,115,776, and US6,327,625.
################################################################################

# Modify by flippy (flippy@sina.com) 2024-3-8

ENABLE_LIB_SUPPORT = n

obj-$(CONFIG_R8168) := r8168.o
r8168-objs := r8168_n.o r8168_asf.o rtl_eeprom.o rtltool.o

ifeq ($(ENABLE_LIB_SUPPORT), y)
	r8168-objs += r8168_lib.o
	EXTRA_CFLAGS += -DENABLE_LIB_SUPPORT
endif

ifeq ($(CONFIG_R8168_SOC_LAN), y)
	EXTRA_CFLAGS += -DCONFIG_SOC_LAN
endif

ifeq ($(CONFIG_R8168_FIBER_SUPPORT), y)
	r8168-objs += r8168_fiber.o
	EXTRA_CFLAGS += -DENABLE_FIBER_SUPPORT
endif

ifeq ($(CONFIG_R8168_REALWOW_SUPPORT), y)
	r8168-objs += r8168_realwow.o
	EXTRA_CFLAGS += -DENABLE_REALWOW_SUPPORT
endif

ifeq ($(CONFIG_R8168_DASH_SUPPORT), y)
	r8168-objs += r8168_dash.o
	EXTRA_CFLAGS += -DENABLE_DASH_SUPPORT
endif

ifeq ($(CONFIG_R8168_DASH_PRINT_SUPPORT), y)
	r8168-objs += r8168_dash.o
	EXTRA_CFLAGS += -DENABLE_DASH_SUPPORT -DENABLE_DASH_PRINTER_SUPPORT
endif

ifeq ($(CONFIG_R8168_DOWN_SPEED_100), y)
	EXTRA_CFLAGS += -DCONFIG_DOWN_SPEED_100
endif

ifeq ($(CONFIG_R8168_ASPM), y)
	EXTRA_CFLAGS += -DCONFIG_ASPM
	ifeq ($(CONFIG_R8168_DYNAMIC_ASPM), y)
		EXTRA_CFLAGS += -DCONFIG_DYNAMIC_ASPM
	endif
endif

ifeq ($(CONFIG_R8168_WOL_SUPPORT), y)
	ifeq ($(CONFIG_R8168_S5WOL), y)
		EXTRA_CFLAGS += -DENABLE_S5WOL
	endif

	ifeq ($(CONFIG_R8168_S5_KEEP_CURR_MAC), y)
		EXTRA_CFLAGS += -DENABLE_S5_KEEP_CURR_MAC
	endif
else
	EXTRA_CFLAGS += -DDISABLE_WOL_SUPPORT
endif

ifeq ($(CONFIG_R8168_EEE), y)
	EXTRA_CFLAGS += -DENABLE_EEE
endif

ifeq ($(CONFIG_R8168_R8168_S0_MAGIC_PACKET), y)
	EXTRA_CFLAGS += -DENABLE_S0_MAGIC_PACKET
endif

ifeq ($(CONFIG_R8168_USE_FIRMWARE_FILE), y)
	r8168-objs += r8168_firmware.o
	EXTRA_CFLAGS += -DENABLE_USE_FIRMWARE_FILE
endif

ifeq ($(CONFIG_R8168_CTAP_SHORT_OFF), y)
	EXTRA_CFLAGS += -DCONFIG_CTAP_SHORT_OFF
endif

ifeq ($(CONFIG_R8168_MULTIPLE_TX_QUEUE), y)
	EXTRA_CFLAGS += -DENABLE_MULTIPLE_TX_QUEUE
endif

ifeq ($(CONFIG_R8168_RSS_SUPPORT), y)
	r8168-objs += r8168_rss.o
	EXTRA_CFLAGS += -DENABLE_RSS_SUPPORT
else
	EXTRA_CFLAGS += -DCONFIG_R8168_NAPI
endif

EXTRA_CFLAGS += -DCONFIG_R8168_VLAN
