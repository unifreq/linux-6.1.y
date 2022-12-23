// SPDX-License-Identifier: GPL-2.0
/*
 * Allwinner xHCI Host Controller Driver
 *
 * Copyright (c) 2017 Icenowy Zheng <icenowy@aosc.io>
 *
 * Based on code from Allwinner BSP, which is:
 *
 * Copyright (c) 2010-2015 Allwinner Technology Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/slab.h>

#include "xhci.h"

/* Global Registers */
#define SUNXI_GLOBALS_REGS_GCTL		0xc110
#define SUNXI_GUSB2PHYCFG(n)		(0xc200 + (n * 0x04))
#define SUNXI_GUSB3PIPECTL(n)		(0xc2c0 + (n * 0x04))

/* Interface Status and Control Registers */
#define SUNXI_APP			0x10000
#define SUNXI_PIPE_CLOCK_CONTROL	0x10014
#define SUNXI_PHY_TUNE_LOW		0x10018
#define SUNXI_PHY_TUNE_HIGH		0x1001c
#define SUNXI_PHY_EXTERNAL_CONTROL	0x10020

/* Global Configuration Register */
#define SUNXI_GCTL_PRTCAP_HOST		(1 << 12)
#define SUNXI_GCTL_PRTCAP_DEVICE	(2 << 12)
#define SUNXI_GCTL_SOFITPSYNC		(0x01 << 10)
#define SUNXI_GCTL_CORESOFTRESET	(1 << 11)

/* Global USB2 PHY Configuration Register n */
#define SUNXI_USB2PHYCFG_SUSPHY		(0x01 << 6)
#define SUNXI_USB2PHYCFG_PHYSOFTRST	(1 << 31)

/* Global USB3 PIPE Control Register */
#define SUNXI_USB3PIPECTL_PHYSOFTRST	(1 << 31)

/* USB2.0 Interface Status and Control Register */
#define SUNXI_APP_FORCE_VBUS		(0x03 << 12)

/* PIPE Clock Control Register */
#define SUNXI_PCC_PIPE_CLK_OPEN		(0x01 << 6)

/* PHY External Control Register */
#define SUNXI_PEC_EXTERN_VBUS		(0x03 << 1)
#define SUNXI_PEC_SSC_EN		(0x01 << 24)
#define SUNXI_PEC_REF_SSP_EN		(0x01 << 26)

/* PHY Tune High Register */
#define SUNXI_TX_DEEMPH_3P5DB(n)	((n) << 19)
#define SUNXI_TX_DEEMPH_3P5DB_MASK	GENMASK(24, 19)
#define SUNXI_TX_DEEMPH_6DB(n)		((n) << 13)
#define SUNXI_TX_DEEMPH_6GB_MASK	GENMASK(18, 13)
#define SUNXI_TX_SWING_FULL(n)		((n) << 6)
#define SUNXI_TX_SWING_FULL_MASK	GENMASK(12, 6)
#define SUNXI_LOS_BIAS(n)		((n) << 3)
#define SUNXI_LOS_BIAS_MASK		GENMASK(5, 3)
#define SUNXI_TXVBOOSTLVL(n)		((n) << 0)
#define SUNXI_TXVBOOSTLVL_MASK		GENMASK(2, 0)

struct xhci_sunxi {
	struct device *dev;
	void __iomem *regs;
	struct usb_hcd *hcd;

	struct clk *hci_clk;
	struct clk *phy_clk;

	struct reset_control *hci_rst;
	struct reset_control *phy_rst;

	struct regulator *vbus;
};

static inline struct xhci_sunxi *hcd_to_sunxi(struct usb_hcd *hcd)
{
	return dev_get_drvdata(hcd->self.controller);
}

static void xhci_sunxi_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	xhci->quirks |= XHCI_PLAT;
}

static void xhci_sunxi_soft_reset(struct xhci_sunxi *sunxi)
{
	u32 val;

	/* Before Resetting PHY, put Core in Reset */
	val = readl(sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);
	val |= SUNXI_GCTL_CORESOFTRESET;
	writel(val, sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);

	/* Assert USB3 PHY reset */
	val = readl(sunxi->regs + SUNXI_GUSB3PIPECTL(0));
	val |= SUNXI_USB3PIPECTL_PHYSOFTRST;
	writel(val, sunxi->regs + SUNXI_GUSB3PIPECTL(0));

	/* Assert USB2 PHY reset */
	val = readl(sunxi->regs + SUNXI_GUSB2PHYCFG(0));
	val |= SUNXI_USB2PHYCFG_PHYSOFTRST;
	writel(val, sunxi->regs + SUNXI_GUSB2PHYCFG(0));

	/* The delay time is directly taken from the BSP */
	mdelay(100);

	/* Clear USB3 PHY reset */
	val = readl(sunxi->regs + SUNXI_GUSB3PIPECTL(0));
	val &= ~SUNXI_USB3PIPECTL_PHYSOFTRST;
	writel(val, sunxi->regs + SUNXI_GUSB3PIPECTL(0));

	/* Clear USB2 PHY reset */
	val = readl(sunxi->regs + SUNXI_GUSB2PHYCFG(0));
	val &= ~SUNXI_USB2PHYCFG_PHYSOFTRST;
	writel(val, sunxi->regs + SUNXI_GUSB2PHYCFG(0));

	mdelay(100);

	/* After PHYs are stable we can take Core out of reset state */
	val = readl(sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);
	val &= ~SUNXI_GCTL_CORESOFTRESET;
	writel(val, sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);
}

static void xhci_sunxi_init_phy(struct xhci_sunxi *sunxi)
{
	u32 val;

	val = readl(sunxi->regs + SUNXI_PHY_EXTERNAL_CONTROL);
	val |= SUNXI_PEC_EXTERN_VBUS; /* Use extern vbus to phy */
	val |= SUNXI_PEC_SSC_EN | SUNXI_PEC_REF_SSP_EN;
	writel(val, sunxi->regs + SUNXI_PHY_EXTERNAL_CONTROL);

	val = readl(sunxi->regs + SUNXI_PIPE_CLOCK_CONTROL);
	val |= SUNXI_PCC_PIPE_CLK_OPEN;
	writel(val, sunxi->regs + SUNXI_PIPE_CLOCK_CONTROL);

	val = readl(sunxi->regs + SUNXI_APP);
	val |= SUNXI_APP_FORCE_VBUS;
	writel(val, sunxi->regs + SUNXI_APP);

	/*
	 * All the magic numbers written to the PHY_TUNE_{LOW_HIGH}
	 * registers are directly taken from the BSP USB3 driver from
	 * Allwiner.
	 */
	writel(0x0047fc87, sunxi->regs + SUNXI_PHY_TUNE_LOW);

	val = readl(sunxi->regs + SUNXI_PHY_TUNE_HIGH);
	val &= ~(SUNXI_TXVBOOSTLVL_MASK | SUNXI_LOS_BIAS_MASK |
		     SUNXI_TX_SWING_FULL_MASK | SUNXI_TX_DEEMPH_6GB_MASK |
		     SUNXI_TX_DEEMPH_3P5DB_MASK);
	val |= SUNXI_TXVBOOSTLVL(0x7);
	val |= SUNXI_LOS_BIAS(0x7);
	val |= SUNXI_TX_SWING_FULL(0x55);
	val |= SUNXI_TX_DEEMPH_6DB(0x20);
	val |= SUNXI_TX_DEEMPH_3P5DB(0x15);
	writel(val, sunxi->regs + SUNXI_PHY_TUNE_HIGH);

	/* Enable USB2 PHY Suspend mode. */
/*	val = readl(sunxi->regs + SUNXI_GUSB2PHYCFG(0));
	val |= SUNXI_USB2PHYCFG_SUSPHY;
	writel(val, sunxi->regs + SUNXI_GUSB2PHYCFG(0));*/

	/* Enable SOFITPSYNC for suspend. */
/*	val = readl(sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);
	val |= SUNXI_GCTL_SOFITPSYNC;
	writel(val, sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);*/
}

static void xhci_sunxi_set_host_mode(struct xhci_sunxi *sunxi)
{
	u32 val;

	val = readl(sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);
	val &= ~(SUNXI_GCTL_PRTCAP_DEVICE);
	val |= SUNXI_GCTL_PRTCAP_HOST;
	writel(val, sunxi->regs + SUNXI_GLOBALS_REGS_GCTL);
}

static int xhci_sunxi_setup(struct usb_hcd *hcd)
{
	int ret;
	struct xhci_sunxi *sunxi = hcd_to_sunxi(hcd);

//	xhci_sunxi_soft_reset(sunxi);

	ret = xhci_gen_setup(hcd, xhci_sunxi_quirks);
	if (ret)
		return ret;

	xhci_sunxi_init_phy(sunxi);
	xhci_sunxi_set_host_mode(sunxi);

	return 0;
}

static const struct xhci_driver_overrides xhci_sunxi_overrides __initconst = {
	.reset = xhci_sunxi_setup,
};

static struct hc_driver __read_mostly xhci_sunxi_hc_driver;

static int xhci_sunxi_enable_clk_rst(struct xhci_sunxi *sunxi)
{
	int ret;

	ret = clk_prepare_enable(sunxi->hci_clk);
	if (ret)
		return ret;

	ret = clk_prepare_enable(sunxi->phy_clk);
	if (ret)
		goto disable_hci_clk;

	ret = reset_control_deassert(sunxi->hci_rst);
	if (ret)
		goto disable_phy_clk;

	ret = reset_control_deassert(sunxi->phy_rst);
	if (ret)
		goto assert_hci_rst;

	return 0;

assert_hci_rst:
	reset_control_assert(sunxi->hci_rst);

disable_phy_clk:
	clk_disable_unprepare(sunxi->phy_clk);

disable_hci_clk:
	clk_disable_unprepare(sunxi->hci_clk);
	return ret;
}

static void xhci_sunxi_disable_clk_rst(struct xhci_sunxi *sunxi)
{
	reset_control_assert(sunxi->phy_rst);
	reset_control_assert(sunxi->hci_rst);
	clk_disable_unprepare(sunxi->phy_clk);
	clk_disable_unprepare(sunxi->hci_clk);
}

static int xhci_sunxi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct hc_driver *driver;
	struct resource *res;
	struct usb_hcd *hcd;
	struct xhci_sunxi *sunxi;
	struct xhci_hcd *xhci;
	int ret, irq;

	driver = &xhci_sunxi_hc_driver;
	sunxi = devm_kzalloc(&pdev->dev, sizeof(*sunxi), GFP_KERNEL);
	if (!sunxi)
		return -ENOMEM;

	sunxi->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sunxi->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(sunxi->regs)) {
		dev_err(dev, "cannot get MMIO zone\n");
		return PTR_ERR(sunxi->regs);
	}

	sunxi->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(sunxi->vbus)) {
		dev_err(dev, "cannot get Vbus regulator\n");
		return PTR_ERR(sunxi->vbus);
	}

	sunxi->hci_clk = devm_clk_get(dev, "xhci");
	if (IS_ERR(sunxi->hci_clk)) {
		dev_err(dev, "cannot get XHCI clock\n");
		return PTR_ERR(sunxi->hci_clk);
	}

	sunxi->phy_clk = devm_clk_get(dev, "phy");
	if (IS_ERR(sunxi->phy_clk)) {
		dev_err(dev, "cannot get PHY clock\n");
		return PTR_ERR(sunxi->phy_clk);
	}

	sunxi->hci_rst = devm_reset_control_get(dev, "xhci");
	if (IS_ERR(sunxi->hci_rst)) {
		dev_err(dev, "cannot get XHCI reset\n");
		return PTR_ERR(sunxi->hci_rst);
	}

	sunxi->phy_rst = devm_reset_control_get(dev, "phy");
	if (IS_ERR(sunxi->phy_rst)) {
		dev_err(dev, "cannot get PHY reset\n");
		return PTR_ERR(sunxi->phy_rst);
	}

	ret = xhci_sunxi_enable_clk_rst(sunxi);
	if (ret) {
		dev_err(dev, "cannot enable clocks and deassert resets\n");
		return ret;
	}

	ret = regulator_enable(sunxi->vbus);
	if (ret) {
		dev_err(dev, "cannot enable Vbus regulator\n");
		goto disable_clk_rst;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "cannot get IRQ\n");
		ret = irq;
		goto disable_regulator;
	}

	hcd = usb_create_hcd(driver, dev, dev_name(dev));
	if (!hcd) {
		dev_err(dev, "cannot allocate memory for the main HCD\n");
		ret = -ENOMEM;
		goto disable_regulator;
	}

	/*
	 * USB 2.0 roothub is stored in the platform_device.
	 * Swap it with sunxi HCD.
	 */
	sunxi->hcd = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, sunxi);

	hcd->regs = sunxi->regs;
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	device_init_wakeup(dev, true);

	xhci = hcd_to_xhci(hcd);
	xhci->main_hcd = hcd;
	xhci->shared_hcd = usb_create_shared_hcd(driver, dev,
			dev_name(dev), hcd);
	if (!xhci->shared_hcd) {
		dev_err(dev, "cannot allocate memory for the shared HCD\n");
		ret = -ENOMEM;
		goto put_hcd;
	}

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(dev, "cannot add main HCD\n");
		goto put_shared_hcd;
	}

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(dev, "cannot add shared HCD\n");
		goto remove_hcd;
	}

	return 0;

remove_hcd:
	usb_remove_hcd(hcd);

put_shared_hcd:
	usb_put_hcd(xhci->shared_hcd);

put_hcd:
	device_init_wakeup(&pdev->dev, false);
	usb_put_hcd(hcd);

disable_regulator:
	regulator_disable(sunxi->vbus);

disable_clk_rst:
	xhci_sunxi_disable_clk_rst(sunxi);
	return ret;
}

static int xhci_sunxi_remove(struct platform_device *pdev)
{
	struct xhci_sunxi *sunxi = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = sunxi->hcd;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	usb_remove_hcd(xhci->shared_hcd);
	usb_remove_hcd(hcd);
	usb_put_hcd(xhci->shared_hcd);
	device_init_wakeup(&pdev->dev, false);
	usb_put_hcd(hcd);
	regulator_disable(sunxi->vbus);
	xhci_sunxi_disable_clk_rst(sunxi);

	return 0;
}

static const struct of_device_id sunxi_xhci_of_match[] = {
	{ .compatible = "allwinner,sun50i-h6-xhci"},
	{ },
};
MODULE_DEVICE_TABLE(of, sunxi_xhci_of_match);

static struct platform_driver sunxi_xhci_driver = {
	.probe	= xhci_sunxi_probe,
	.remove	= xhci_sunxi_remove,
	.driver	= {
		.name = "xhci-sunxi",
		.of_match_table = of_match_ptr(sunxi_xhci_of_match),
	},
};

static int __init xhci_sunxi_init(void)
{
	xhci_init_driver(&xhci_sunxi_hc_driver, &xhci_sunxi_overrides);
	return platform_driver_register(&sunxi_xhci_driver);
}
module_init(xhci_sunxi_init);

static void __exit xhci_sunxi_exit(void)
{
	platform_driver_unregister(&sunxi_xhci_driver);
}
module_exit(xhci_sunxi_exit);

MODULE_AUTHOR("Icenowy Zheng <icenowy@aosc.io>");
MODULE_DESCRIPTION("Allwinner xHCI Host Controller Driver");
MODULE_LICENSE("GPL v2");
