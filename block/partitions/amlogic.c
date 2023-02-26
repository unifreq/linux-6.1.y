// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Guoxin "7Ji" Pu
 * Author: Pu Guoxin <pugokushin@gmail.com>
 *
 * Read Amlogic's proprietary eMMC partition table that is mainly
 * used on the eMMC on TV boxes running Android with their SoCs.
 * 
 * The usage is not limited to eMMC but can be limited with kernel
 * commandline.
 *
 * For further information, see "Documentation/block/amlogic-partition.rst"
 *
 */

#include "check.h"

#define APT_PART_NAME_MAXLEN	15
#define APT_MAX_PARTS			32
#define APT_MAGIC_STRING		"MPT"
#define APT_MAGIC_LE32			(__le32)0x0054504DU
#define APT_VERSION_STRING		"01.00.00"

struct amlogic_header {
	__le32 magic;
	char version[12];
	__le32 count;
	__le32 checksum;
} __packed;

struct amlogic_partition {
	char name[16];
	__le64 size;
	__le64 offset;
	__le32 mask_flags;
	u32 padding;
} __packed;

struct amlogic_table {
	struct amlogic_header header;
	struct amlogic_partition parts[APT_MAX_PARTS];
} __packed;
	

u32 amlogic_checksum(struct amlogic_table *apt) {
	u32 checksum = 0;
	u32 *p;
	for (u32 i = 0; i < le32_to_cpu(apt->header.count); ++i) {
		p = (u32 *)apt->parts;
		for (u32 j = sizeof *apt->parts / sizeof *p; j > 0; --j) {
			checksum += le32_to_cpu(*p++);
		}
	}
	return checksum;
}

static bool apt_checksum = true;
static int __init apt_checksum_setup(char *s) {
	if (s && s[0] == '0' && s[1] == '\0') {
		pr_warn("Amlogic partition: checksum is disabled\n");
		apt_checksum = false;
	}
	return 1;
}
__setup("apt_checksum=", apt_checksum_setup);

bool amlogic_is_partition_name_valid(char *name) {
	char safe_name[19];
	for (u8 i = 0; i < 16; ++i) {
		switch (name[i]) {
			case 'a'...'z':
			case '_':
				break;
			case '\0':
				if (i == 0) {
					pr_warn("Amlogic partition: partition name empty, which is illegal\n");
					return false;
				} else {
					return true;
				}
			default:
				pr_warn("Amlogic partition: illegal character in apt partition name: %c, allowed for now, MAYBE FORBIDDEN IN THE FUTURE\n", name[i]);
				break;
		}
	}
	strncpy(safe_name, name, 15);
	strncpy(safe_name + 15, "...", 3);
	pr_warn("Amlogic partition: partition name not ended properly: %s\n", safe_name);
	return false;
}

bool amlogic_is_partition_valid(struct amlogic_partition *part) {
	u64 offset_raw;
	u64 offset_round;
	u64 size_raw;
	u64 size_round;
	if (!amlogic_is_partition_name_valid(part->name)) {
		return false;
	}
	/* They could be not  */
	offset_raw = le64_to_cpu(part->offset);
	offset_round = offset_raw >> 9 << 9;
	if (offset_raw != offset_round) {
		pr_warn("Amlogic partition: partition's offset is not multiple of 512: %llx\n", offset_raw);
		return false;
	}
	size_raw = le64_to_cpu(part->size);
	size_round = size_raw >> 9 << 9;
	if (size_raw != size_round) {
		pr_warn("Amlogic partition: partition's size is not multiple of 512: %llx\n", size_raw);
		return false;
	}
	return true;
}

bool amlogic_is_valid(struct amlogic_table *apt) {
	u32 checksum;
	if (!apt) {
		pr_warn("Amlogic partition: not allocated properly\n");
		return false;
	}
	if (apt->header.magic != APT_MAGIC_LE32) {
		pr_warn("Amlogic partition: header magic not right: %x != %x\n", apt->header.magic, APT_MAGIC_LE32);
		return false;
	}
	if (strncmp(apt->header.version, APT_VERSION_STRING, strlen(APT_VERSION_STRING))) {
		char safe_version[15] = "";
		strncpy(safe_version, apt->header.version, 11);
		if (safe_version[11]) {
			strncpy(safe_version + 11, "...", 3);
		}
		pr_warn("Amlogic partition: header version not right: %s != %s\n", safe_version, APT_VERSION_STRING);
		return false;
	}
	if (!apt->header.count) {
		pr_warn("Amlogic partition: header entries count is 0, skipped\n");
		return false;
	}
	if (le32_to_cpu(apt->header.count) > APT_MAX_PARTS) {
		pr_warn("Amlogic partition: entries overflow: %u > %u\n", le32_to_cpu(apt->header.count), APT_MAX_PARTS);
		return false;
	}
	checksum = amlogic_checksum(apt);
	if (apt_checksum && checksum != le32_to_cpu(apt->header.checksum)) {
		pr_warn("Amlogic partition: checksum mismatch: calculated %x != recorded %x. (This check can be turned off by setting apt_checksum=0)\n", checksum, le32_to_cpu(apt->header.checksum));
		return false;
	}
	for (u32 i = 0; i < le32_to_cpu(apt->header.count); i++) {
		if (!amlogic_is_partition_valid(apt->parts + i)) {
			pr_warn("Amlogic partition: partition entry %u invalid\n", i);
			return false;
		}
	}
	return true;
}

static char *apt_blkdevs;

static int __init apt_blkdevs_setup(char *s) {
	apt_blkdevs = s;
	return 1;
}
__setup("apt_blkdevs=", apt_blkdevs_setup);

bool amlogic_should_parse_block(struct parsed_partitions *state) {
	char *blkdev;
	if (!apt_blkdevs) {
		pr_debug("Amlogic partition: apt_blkdevs is not set, no block device should be parsed\n");
		return false;
	}
	if (!apt_blkdevs[0]) {
		pr_debug("Amlogic partition: apt_blkdevs is empty, no block devices should be parsed\n");
		return false;
	}
	if (!strncmp(apt_blkdevs, "all", 3)) {
		pr_debug("Amlogic partition: apt_blkdevs set to all, all block devices should be parsed\n");
		return true;
	}
	/* apt_blkdevs set and not empty, only parse block if its in the list */
	blkdev = apt_blkdevs;
	for (char *c = apt_blkdevs;; ++c) {
		switch (*c) {
			case ',':
			case '\0':
				if (strncmp(blkdev, state->disk->disk_name, c - blkdev)) {
					if (*c == '\0') {
						return false;
					}
				} else {
					return true;
				}
				blkdev = c + 1;
				/* end of a blkdev definition */
				break;
			default:
				break;
		}

	}
	return false;
}

/**
 * amlogic_partition - scan for Amlogic proprietary partitions
 * @state: disk parsed partitions
 * 
 * Returns:
 * -1 if unable to read the partition table
 *  0 if this isn't our partition table
 *  1 if successful
 *
 */
int amlogic_partition(struct parsed_partitions *state){
	sector_t disk_sectors;
	sector_t disk_size;
	struct amlogic_table *apt;
	u8 apt_cache[((sizeof *apt >> 9) + 1) << 9] = {0};

	if (!amlogic_should_parse_block(state)) {
		return 0;
	}

	disk_sectors = get_capacity(state->disk);
	if (disk_sectors < 0x12003) {
		return 0;
	}
	disk_size = disk_sectors << 9;
	 
	for (sector_t i = 0; i < 3; ++i) {
		Sector sect;
		u8 *data = read_part_sector(state, 0x12000 + i, &sect);
		if (!data) {
			return -1;
		}
		memcpy(apt_cache + 512 * i, data, 512);
		put_dev_sector(sect);
	}

	apt = (struct amlogic_table *)apt_cache;
	if (!amlogic_is_valid(apt)) {
		return 0;
	}

	pr_debug("Amlogic partition: partition table is valid, now parsing it\n");

	for (u32 i = 0; i < le32_to_cpu(apt->header.count) && i < state->limit-1; i++) {
		struct amlogic_partition *part = apt->parts + i;
		u64 offset = le64_to_cpu(part->offset) >> 9;
		u64 size = le64_to_cpu(part->size) >> 9;
		u64 end;
		struct partition_meta_info *info;
		size_t name_min;
		char tmp[sizeof(info->volname) + 4];
		if (offset > disk_sectors) {
			pr_warn("Amlogic partition: partition %s's offset is larger than disk size (sectors 0x%llx > 0x%llx), shifting its offset to disk end\n", part->name, offset, disk_sectors);
			offset = disk_sectors;
		}
		
		end = offset + size;

		if (end > disk_sectors) {
			u64 diff = end - disk_sectors;
			pr_warn("Amlogic partition: partition %s's size is too large and it exceeds the disk size (sectors end 0x%llx > 0x%llx), shrinking its size by 0x%llx sectors\n", part->name, end, disk_sectors, diff);
			size -= diff;
		}

		if (size == 0) {
			pr_warn("Amlogic partition: partition %s's size is 0, you will probably not be able to access it\n", part->name);
		}

		put_partition(state, i + 1, offset, size);

		info = &state->parts[i + 1].info;
		name_min = min_t(size_t, sizeof info->volname, sizeof part->name);
		strncpy(info->volname, part->name, name_min);
		info->volname[name_min] = '\0';

		snprintf(tmp, sizeof(tmp), "(%s)", info->volname);
		strlcat(state->pp_buf, tmp, PAGE_SIZE);

		state->parts[i + 1].has_info = true;
	}

	strlcat(state->pp_buf, "\n", PAGE_SIZE);
	return 1;
}
