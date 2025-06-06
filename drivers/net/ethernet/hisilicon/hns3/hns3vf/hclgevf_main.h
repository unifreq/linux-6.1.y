/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2016-2017 Hisilicon Limited. */

#ifndef __HCLGEVF_MAIN_H
#define __HCLGEVF_MAIN_H
#include <linux/fs.h>
#include <linux/if_vlan.h>
#include <linux/types.h>
#include <net/devlink.h>
#include "hclge_mbx.h"
#include "hclgevf_cmd.h"
#include "hnae3.h"
#include "hclge_comm_rss.h"
#include "hclge_comm_tqp_stats.h"

#define HCLGEVF_MOD_VERSION "1.0"
#define HCLGEVF_DRIVER_NAME "hclgevf"

#define HCLGEVF_MAX_VLAN_ID	4095
#define HCLGEVF_MISC_VECTOR_NUM		0

#define HCLGEVF_INVALID_VPORT		0xffff
#define HCLGEVF_GENERAL_TASK_INTERVAL	  5
#define HCLGEVF_KEEP_ALIVE_TASK_INTERVAL  2

/* This number in actual depends upon the total number of VFs
 * created by physical function. But the maximum number of
 * possible vector-per-VF is {VFn(1-32), VECTn(32 + 1)}.
 */
#define HCLGEVF_MAX_VF_VECTOR_NUM	(32 + 1)

#define HCLGEVF_VECTOR_REG_BASE		0x20000
#define HCLGEVF_MISC_VECTOR_REG_BASE	0x20400
#define HCLGEVF_VECTOR_REG_OFFSET	0x4
#define HCLGEVF_VECTOR_VF_OFFSET		0x100000

/* bar registers for common func */
#define HCLGEVF_GRO_EN_REG			0x28000
#define HCLGEVF_RXD_ADV_LAYOUT_EN_REG		0x28008

/* bar registers for rcb */
#define HCLGEVF_RING_RX_ADDR_L_REG		0x80000
#define HCLGEVF_RING_RX_ADDR_H_REG		0x80004
#define HCLGEVF_RING_RX_BD_NUM_REG		0x80008
#define HCLGEVF_RING_RX_BD_LENGTH_REG		0x8000C
#define HCLGEVF_RING_RX_MERGE_EN_REG		0x80014
#define HCLGEVF_RING_RX_TAIL_REG		0x80018
#define HCLGEVF_RING_RX_HEAD_REG		0x8001C
#define HCLGEVF_RING_RX_FBD_NUM_REG		0x80020
#define HCLGEVF_RING_RX_OFFSET_REG		0x80024
#define HCLGEVF_RING_RX_FBD_OFFSET_REG		0x80028
#define HCLGEVF_RING_RX_STASH_REG		0x80030
#define HCLGEVF_RING_RX_BD_ERR_REG		0x80034
#define HCLGEVF_RING_TX_ADDR_L_REG		0x80040
#define HCLGEVF_RING_TX_ADDR_H_REG		0x80044
#define HCLGEVF_RING_TX_BD_NUM_REG		0x80048
#define HCLGEVF_RING_TX_PRIORITY_REG		0x8004C
#define HCLGEVF_RING_TX_TC_REG			0x80050
#define HCLGEVF_RING_TX_MERGE_EN_REG		0x80054
#define HCLGEVF_RING_TX_TAIL_REG		0x80058
#define HCLGEVF_RING_TX_HEAD_REG		0x8005C
#define HCLGEVF_RING_TX_FBD_NUM_REG		0x80060
#define HCLGEVF_RING_TX_OFFSET_REG		0x80064
#define HCLGEVF_RING_TX_EBD_NUM_REG		0x80068
#define HCLGEVF_RING_TX_EBD_OFFSET_REG		0x80070
#define HCLGEVF_RING_TX_BD_ERR_REG		0x80074
#define HCLGEVF_RING_EN_REG			0x80090

/* bar registers for tqp interrupt */
#define HCLGEVF_TQP_INTR_CTRL_REG		0x20000
#define HCLGEVF_TQP_INTR_GL0_REG		0x20100
#define HCLGEVF_TQP_INTR_GL1_REG		0x20200
#define HCLGEVF_TQP_INTR_GL2_REG		0x20300
#define HCLGEVF_TQP_INTR_RL_REG			0x20900

/* CMDQ register bits for RX event(=MBX event) */
#define HCLGEVF_VECTOR0_RX_CMDQ_INT_B	1
/* RST register bits for RESET event */
#define HCLGEVF_VECTOR0_RST_INT_B	2

#define HCLGEVF_TQP_RESET_TRY_TIMES	10
/* Reset related Registers */
#define HCLGEVF_RST_ING			0x20C00
#define HCLGEVF_FUN_RST_ING_BIT		BIT(0)
#define HCLGEVF_GLOBAL_RST_ING_BIT	BIT(5)
#define HCLGEVF_CORE_RST_ING_BIT	BIT(6)
#define HCLGEVF_IMP_RST_ING_BIT		BIT(7)
#define HCLGEVF_RST_ING_BITS \
	(HCLGEVF_FUN_RST_ING_BIT | HCLGEVF_GLOBAL_RST_ING_BIT | \
	 HCLGEVF_CORE_RST_ING_BIT | HCLGEVF_IMP_RST_ING_BIT)

#define HCLGEVF_VF_RST_ING		0x07008
#define HCLGEVF_VF_RST_ING_BIT		BIT(16)

#define HCLGEVF_WAIT_RESET_DONE		100

#define HCLGEVF_RSS_IND_TBL_SIZE		512

#define HCLGEVF_TQP_MEM_SIZE		0x10000
#define HCLGEVF_MEM_BAR			4
/* in the bar4, the first half is for roce, and the second half is for nic */
#define HCLGEVF_NIC_MEM_OFFSET(hdev)	\
	(pci_resource_len((hdev)->pdev, HCLGEVF_MEM_BAR) >> 1)
#define HCLGEVF_TQP_MEM_OFFSET(hdev, i)		\
	(HCLGEVF_NIC_MEM_OFFSET(hdev) + HCLGEVF_TQP_MEM_SIZE * (i))

#define HCLGEVF_MAC_MAX_FRAME		9728

#define HCLGEVF_STATS_TIMER_INTERVAL	36U

#define hclgevf_read_dev(a, reg) \
	hclge_comm_read_reg((a)->hw.io_base, reg)
#define hclgevf_write_dev(a, reg, value) \
	hclge_comm_write_reg((a)->hw.io_base, reg, value)

enum hclgevf_evt_cause {
	HCLGEVF_VECTOR0_EVENT_RST,
	HCLGEVF_VECTOR0_EVENT_MBX,
	HCLGEVF_VECTOR0_EVENT_OTHER,
};

/* states of hclgevf device & tasks */
enum hclgevf_states {
	/* device states */
	HCLGEVF_STATE_DOWN,
	HCLGEVF_STATE_DISABLED,
	HCLGEVF_STATE_IRQ_INITED,
	HCLGEVF_STATE_REMOVING,
	HCLGEVF_STATE_NIC_REGISTERED,
	HCLGEVF_STATE_ROCE_REGISTERED,
	HCLGEVF_STATE_SERVICE_INITED,
	/* task states */
	HCLGEVF_STATE_RST_SERVICE_SCHED,
	HCLGEVF_STATE_RST_HANDLING,
	HCLGEVF_STATE_MBX_SERVICE_SCHED,
	HCLGEVF_STATE_MBX_HANDLING,
	HCLGEVF_STATE_LINK_UPDATING,
	HCLGEVF_STATE_PROMISC_CHANGED,
	HCLGEVF_STATE_RST_FAIL,
	HCLGEVF_STATE_PF_PUSH_LINK_STATUS,
};

struct hclgevf_mac {
	u8 media_type;
	u8 module_type;
	u8 mac_addr[ETH_ALEN];
	int link;
	u8 duplex;
	u32 speed;
	u64 supported;
	u64 advertising;
};

struct hclgevf_hw {
	struct hclge_comm_hw hw;
	int num_vec;
	struct hclgevf_mac mac;
};

struct hclgevf_cfg {
	u8 tc_num;
	u16 tqp_desc_num;
	u16 rx_buf_len;
	u8 phy_addr;
	u8 media_type;
	u8 mac_addr[ETH_ALEN];
	u32 numa_node_map;
};

struct hclgevf_misc_vector {
	u8 __iomem *addr;
	int vector_irq;
	char name[HNAE3_INT_NAME_LEN];
};

struct hclgevf_rst_stats {
	u32 rst_cnt;			/* the number of reset */
	u32 vf_func_rst_cnt;		/* the number of VF function reset */
	u32 flr_rst_cnt;		/* the number of FLR */
	u32 vf_rst_cnt;			/* the number of VF reset */
	u32 rst_done_cnt;		/* the number of reset completed */
	u32 hw_rst_done_cnt;		/* the number of HW reset completed */
	u32 rst_fail_cnt;		/* the number of VF reset fail */
};

enum HCLGEVF_MAC_ADDR_TYPE {
	HCLGEVF_MAC_ADDR_UC,
	HCLGEVF_MAC_ADDR_MC
};

enum HCLGEVF_MAC_NODE_STATE {
	HCLGEVF_MAC_TO_ADD,
	HCLGEVF_MAC_TO_DEL,
	HCLGEVF_MAC_ACTIVE
};

struct hclgevf_mac_addr_node {
	struct list_head node;
	enum HCLGEVF_MAC_NODE_STATE state;
	u8 mac_addr[ETH_ALEN];
};

struct hclgevf_mac_table_cfg {
	spinlock_t mac_list_lock; /* protect mac address need to add/detele */
	struct list_head uc_mac_list;
	struct list_head mc_mac_list;
};

struct hclgevf_dev {
	struct pci_dev *pdev;
	struct hnae3_ae_dev *ae_dev;
	struct hclgevf_hw hw;
	struct hclgevf_misc_vector misc_vector;
	struct hclge_comm_rss_cfg rss_cfg;
	unsigned long state;
	unsigned long flr_state;
	unsigned long default_reset_request;
	unsigned long last_reset_time;
	enum hnae3_reset_type reset_level;
	unsigned long reset_pending;
	enum hnae3_reset_type reset_type;
	struct timer_list reset_timer;

#define HCLGEVF_RESET_REQUESTED		0
#define HCLGEVF_RESET_PENDING		1
	unsigned long reset_state;	/* requested, pending */
	struct hclgevf_rst_stats rst_stats;
	u32 reset_attempts;
	struct semaphore reset_sem;	/* protect reset process */

	u32 fw_version;
	u16 mbx_api_version;
	u16 num_tqps;		/* num task queue pairs of this VF */

	u16 alloc_rss_size;	/* allocated RSS task queue */
	u16 rss_size_max;	/* HW defined max RSS task queue */

	u16 num_alloc_vport;	/* num vports this driver supports */
	nodemask_t numa_node_mask;
	u16 rx_buf_len;
	u16 num_tx_desc;	/* desc num of per tx queue */
	u16 num_rx_desc;	/* desc num of per rx queue */
	u8 hw_tc_map;
	u8 has_pf_mac;

	u16 num_msi;
	u16 num_msi_left;
	u16 num_msi_used;
	u16 num_nic_msix;	/* Num of nic vectors for this VF */
	u16 num_roce_msix;	/* Num of roce vectors for this VF */
	u16 roce_base_msix_offset;
	u16 *vector_status;
	int *vector_irq;

	bool gro_en;
	bool rxvtag_strip_en;

	unsigned long vlan_del_fail_bmap[BITS_TO_LONGS(VLAN_N_VID)];

	struct hclgevf_mac_table_cfg mac_table;

	struct hclgevf_mbx_resp_status mbx_resp; /* mailbox response */
	struct hclgevf_mbx_arq_ring arq; /* mailbox async rx queue */

	struct delayed_work service_task;

	struct hclge_comm_tqp *htqp;

	struct hnae3_handle nic;
	struct hnae3_handle roce;

	struct hnae3_client *nic_client;
	struct hnae3_client *roce_client;
	u32 flag;
	unsigned long serv_processed_cnt;
	unsigned long last_serv_processed;

	struct devlink *devlink;
};

static inline bool hclgevf_is_reset_pending(struct hclgevf_dev *hdev)
{
	return !!hdev->reset_pending;
}

int hclgevf_send_mbx_msg(struct hclgevf_dev *hdev,
			 struct hclge_vf_to_pf_msg *send_msg, bool need_resp,
			 u8 *resp_data, u16 resp_len);
void hclgevf_mbx_handler(struct hclgevf_dev *hdev);
void hclgevf_mbx_async_handler(struct hclgevf_dev *hdev);

void hclgevf_update_link_status(struct hclgevf_dev *hdev, int link_state);
void hclgevf_update_speed_duplex(struct hclgevf_dev *hdev, u32 speed,
				 u8 duplex);
void hclgevf_reset_task_schedule(struct hclgevf_dev *hdev);
void hclgevf_mbx_task_schedule(struct hclgevf_dev *hdev);
void hclgevf_update_port_base_vlan_info(struct hclgevf_dev *hdev, u16 state,
			struct hclge_mbx_port_base_vlan *port_base_vlan);
#endif
