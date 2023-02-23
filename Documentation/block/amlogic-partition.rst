==========================================
Amlogic proprietary eMMC partition parsing
==========================================

Available kernel command line options:

apt_checksum (bool):
	decides whether the table should be checked with Amlogic's
	checksum algorithm to decide if it's a valid table

	any value other than 0 enables the checksum (default), this
	should be OK for tables you didn't touch or have modified
	with ampart as it will update the checksum automatically

	0 disables the checksum, which is only recommended if you
	modify the table by hand

apt_blkdevs (list of <blkdev-id>):
	a list of block devices that the table should be parsed on,
	seperated by ',', if kept empty every block device could be
	used

	default value: (empty), all block devices could be parsed

	suggested value: mmcblk2, so only eMMC will be parsed