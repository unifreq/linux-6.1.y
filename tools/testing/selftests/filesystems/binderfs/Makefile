# SPDX-License-Identifier: GPL-2.0

CFLAGS += $(KHDR_INCLUDES) -pthread
TEST_GEN_PROGS := binderfs_test

binderfs_test: binderfs_test.c ../../kselftest.h ../../kselftest_harness.h

include ../../lib.mk
