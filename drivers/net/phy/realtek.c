/*
 * drivers/net/phy/realtek.c
 *
 * Driver for Realtek PHYs
 *
 * Author: Johnson Leung <r58129@freescale.com>
 *
 * Copyright (c) 2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/bitops.h>
#include <linux/phy.h>
#include <linux/module.h>
#include <linux/delay.h>

#define RTL821x_PHYSR				0x11
#define RTL821x_PHYSR_DUPLEX			BIT(13)
#define RTL821x_PHYSR_SPEED			GENMASK(15, 14)

#define RTL821x_INER				0x12
#define RTL8211B_INER_INIT			0x6400
#define RTL8211E_INER_LINK_STATUS		BIT(10)
#define RTL8211F_INER_LINK_STATUS		BIT(4)

#define RTL821x_INSR				0x13

#define RTL821x_PAGE_SELECT			0x1f

#define RTL8211F_INSR				0x1d

#define RTL8211F_TX_DELAY			BIT(8)

#define RTL8201F_ISR				0x1e
#define RTL8201F_IER				0x13

#define RTL8366RB_POWER_SAVE			0x15
#define RTL8366RB_POWER_SAVE_ON			BIT(12)

MODULE_DESCRIPTION("Realtek PHY driver");
MODULE_AUTHOR("Johnson Leung");
MODULE_LICENSE("GPL");

static int rtl821x_read_page(struct phy_device *phydev)
{
	return __phy_read(phydev, RTL821x_PAGE_SELECT);
}

static int rtl821x_write_page(struct phy_device *phydev, int page)
{
	return __phy_write(phydev, RTL821x_PAGE_SELECT, page);
}

static int rtl8201_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL8201F_ISR);

	return (err < 0) ? err : 0;
}

static int rtl821x_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read(phydev, RTL821x_INSR);

	return (err < 0) ? err : 0;
}
/*
static int rtl8211f_ack_interrupt(struct phy_device *phydev)
{
	int err;

	err = phy_read_paged(phydev, 0xa43, RTL8211F_INSR);

	return (err < 0) ? err : 0;
}
*/
static int rtl8201_config_intr(struct phy_device *phydev)
{
	u16 val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		val = BIT(13) | BIT(12) | BIT(11);
	else
		val = 0;

	return phy_write_paged(phydev, 0x7, RTL8201F_IER, val);
}

static int rtl8211b_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211B_INER_INIT);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}

static int rtl8211e_config_intr(struct phy_device *phydev)
{
	int err;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		err = phy_write(phydev, RTL821x_INER,
				RTL8211E_INER_LINK_STATUS);
	else
		err = phy_write(phydev, RTL821x_INER, 0);

	return err;
}
/*
static int rtl8211f_config_intr(struct phy_device *phydev)
{
	u16 val;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		val = RTL8211F_INER_LINK_STATUS;
	else
		val = 0;

	return phy_write_paged(phydev, 0xa42, RTL821x_INER, val);
}
*/
static int rtl8211_config_aneg(struct phy_device *phydev)
{
	int ret;

	ret = genphy_config_aneg(phydev);
	if (ret < 0)
		return ret;

	/* Quirk was copied from vendor driver. Unfortunately it includes no
	 * description of the magic numbers.
	 */
	if (phydev->speed == SPEED_100 && phydev->autoneg == AUTONEG_DISABLE) {
		phy_write(phydev, 0x17, 0x2138);
		phy_write(phydev, 0x0e, 0x0260);
	} else {
		phy_write(phydev, 0x17, 0x2108);
		phy_write(phydev, 0x0e, 0x0000);
	}

	return 0;
}

static int rtl8211c_config_init(struct phy_device *phydev)
{
	/* RTL8211C has an issue when operating in Gigabit slave mode */
	phy_set_bits(phydev, MII_CTRL1000,
		     CTL1000_ENABLE_MASTER | CTL1000_AS_MASTER);

	return genphy_config_init(phydev);
}

static int rtl8211f_config_init(struct phy_device *phydev)
{
	int ret;
	u16 val = 0;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	/* enable TX-delay for rgmii-id and rgmii-txid, otherwise disable it */
	if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
	    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
		val = RTL8211F_TX_DELAY;

	return phy_modify_paged(phydev, 0xd08, 0x11, RTL8211F_TX_DELAY, val);
}

static int rtl8211fd_config_init(struct phy_device *phydev)
{
	return 0;
}

int rtl8211fd_soft_reset(struct phy_device *phydev)
{
#if 1
    int value,i;

    printk("%s",__func__);
    mdelay(56);
    phy_write(phydev, 31, 0x0B82);
    phy_write(phydev, 16, 0x0010);
    phy_write(phydev, 31, 0x0B80);
    //Check the PHY is ready to patch, time out if not ready after 400ms.
    for(i = 0; i < 40;i++){
        mdelay(10);
        value = phy_read(phydev, 16);
        if(((value>>6)&0x1)== 1) break;
    }

    if(((value>>6)&0x1)== 0) {
        printk("%s step 1 err\n", __func__);
        goto step_err;
    }

    //step 2
    phy_write(phydev, 31, 0x0B82);
    phy_write(phydev, 16, 0x0010);

    phy_write(phydev, 27, 0x802B);
    phy_write(phydev, 28, 0x8700);
    phy_write(phydev, 27, 0xB82E);
    phy_write(phydev, 28, 0x0001);

    phy_write(phydev, 16, 0x0090);

    phy_write(phydev, 27, 0xA016);
    phy_write(phydev, 28, 0x0000);
    phy_write(phydev, 27, 0xA012);
    phy_write(phydev, 28, 0x0000);
    phy_write(phydev, 27, 0xA014);

    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x8010);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x801c);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x8022);
    phy_write(phydev, 28, 0x1800);

    phy_write(phydev, 28, 0x805d);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x8073);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x8077);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x80de);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x80ef);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x6060);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0d1a);
    phy_write(phydev, 28, 0x8602);
    phy_write(phydev, 28, 0xd710);

    phy_write(phydev, 28, 0x2e71);
    phy_write(phydev, 28, 0x0d1e);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x7f8c);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0bba);

    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x0722);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x07a1);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x00a6);
    phy_write(phydev, 28, 0xcc00);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x07c5);
    phy_write(phydev, 28, 0xd502);

    phy_write(phydev, 28, 0x8307);
    phy_write(phydev, 28, 0xd501);
    phy_write(phydev, 28, 0xc30f);
    phy_write(phydev, 28, 0xc2b0);
    phy_write(phydev, 28, 0xc117);
    phy_write(phydev, 28, 0xd500);
    phy_write(phydev, 28, 0xc20c);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x0659);
    phy_write(phydev, 28, 0xd502);
    phy_write(phydev, 28, 0xa080);
    phy_write(phydev, 28, 0xa308);
    phy_write(phydev, 28, 0xd500);
    phy_write(phydev, 28, 0xc430);
    phy_write(phydev, 28, 0x9af0);

    phy_write(phydev, 28, 0x0c0f);
    phy_write(phydev, 28, 0x0102);
    phy_write(phydev, 28, 0xc808);
    phy_write(phydev, 28, 0xd024);
    phy_write(phydev, 28, 0xd1db);
    phy_write(phydev, 28, 0xd06c);
    phy_write(phydev, 28, 0xd1c4);
    phy_write(phydev, 28, 0xd702);
    phy_write(phydev, 28, 0x9410);
    phy_write(phydev, 28, 0xc030);
    phy_write(phydev, 28, 0x401c);

    phy_write(phydev, 28, 0xa3b0);
    phy_write(phydev, 28, 0x8208);
    phy_write(phydev, 28, 0x40ea);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x0722);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x07a1);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x804b);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x07a7);
    phy_write(phydev, 28, 0xd702);
    phy_write(phydev, 28, 0x35a8);
    phy_write(phydev, 28, 0x0025);
    phy_write(phydev, 28, 0x6114);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x0579);
    phy_write(phydev, 28, 0x1000);

    phy_write(phydev, 28, 0x0662);
    phy_write(phydev, 28, 0x3188);
    phy_write(phydev, 28, 0x00b2);
    phy_write(phydev, 28, 0xd702);
    phy_write(phydev, 28, 0x36a3);
    phy_write(phydev, 28, 0x0078);
    phy_write(phydev, 28, 0x37a5);
    phy_write(phydev, 28, 0x0025);
    phy_write(phydev, 28, 0x5d00);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x03ae);
    phy_write(phydev, 28, 0xd700);
    phy_write(phydev, 28, 0x616c);
    phy_write(phydev, 28, 0xd603);
    phy_write(phydev, 28, 0xd60c);
    phy_write(phydev, 28, 0xd610);
    phy_write(phydev, 28, 0xd61c);

    phy_write(phydev, 28, 0xd624);
    phy_write(phydev, 28, 0xd628);
    phy_write(phydev, 28, 0xd630);
    phy_write(phydev, 28, 0xd638);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x072a);
    phy_write(phydev, 28, 0xd603);
    phy_write(phydev, 28, 0xd60c);
    phy_write(phydev, 28, 0xd610);
    phy_write(phydev, 28, 0xd61c);
    phy_write(phydev, 28, 0xd620);
    phy_write(phydev, 28, 0xd62c);
    phy_write(phydev, 28, 0xd630);
    phy_write(phydev, 28, 0xd638);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x072a);
    phy_write(phydev, 28, 0x8102);
    phy_write(phydev, 28, 0xa110);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x047e);
    phy_write(phydev, 28, 0x263c);
    phy_write(phydev, 28, 0x807a);
    phy_write(phydev, 28, 0xac20);
    phy_write(phydev, 28, 0x9f10);
    phy_write(phydev, 28, 0x2019);
    phy_write(phydev, 28, 0x0a6c);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x2d69);
    phy_write(phydev, 28, 0x0a6c);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x80d4);
    phy_write(phydev, 28, 0xa240);
    phy_write(phydev, 28, 0x8910);
    phy_write(phydev, 28, 0xce02);
    phy_write(phydev, 28, 0x0c70);

    phy_write(phydev, 28, 0x0f10);
    phy_write(phydev, 28, 0xaf01);
    phy_write(phydev, 28, 0x8f01);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x09ed);
    phy_write(phydev, 28, 0x8e02);
    phy_write(phydev, 28, 0xa808);
    phy_write(phydev, 28, 0xbf10);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x09ff);
    phy_write(phydev, 28, 0xd710);

    phy_write(phydev, 28, 0x60bc);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x7fa4);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0ba4);
    phy_write(phydev, 28, 0xce04);
    phy_write(phydev, 28, 0x0c70);
    phy_write(phydev, 28, 0x0f20);
    phy_write(phydev, 28, 0xaf01);
    phy_write(phydev, 28, 0x8f01);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x09ed);
    phy_write(phydev, 28, 0x8e04);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x6064);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0ba4);
    phy_write(phydev, 28, 0xa520);

    phy_write(phydev, 28, 0x0c0f);
    phy_write(phydev, 28, 0x0504);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x60b9);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x7fa4);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0ba4);
    phy_write(phydev, 28, 0xa13e);
    phy_write(phydev, 28, 0xc240);
    phy_write(phydev, 28, 0x84c0);
    phy_write(phydev, 28, 0xb104);
    phy_write(phydev, 28, 0xa704);
    phy_write(phydev, 28, 0xa701);
    phy_write(phydev, 28, 0xa510);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x4018);
    phy_write(phydev, 28, 0x9910);
    phy_write(phydev, 28, 0x9f10);
    phy_write(phydev, 28, 0x8510);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0xc001);
    phy_write(phydev, 28, 0x4000);
    phy_write(phydev, 28, 0x5a46);
    phy_write(phydev, 28, 0xa101);
    phy_write(phydev, 28, 0xa43f);
    phy_write(phydev, 28, 0xa660);
    phy_write(phydev, 28, 0x9104);

    phy_write(phydev, 28, 0xc001);
    phy_write(phydev, 28, 0x4000);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x60ea);
    phy_write(phydev, 28, 0x7fc6);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x2425);
    phy_write(phydev, 28, 0x0a43);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x80d0);
    phy_write(phydev, 28, 0xa420);
    phy_write(phydev, 28, 0xa680);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x2425);
    phy_write(phydev, 28, 0x0a43);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x7f86);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x0a1c);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x808c);
    phy_write(phydev, 28, 0xd711);
    phy_write(phydev, 28, 0x6081);
    phy_write(phydev, 28, 0xaa11);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x80db);
    phy_write(phydev, 28, 0xaa22);
    phy_write(phydev, 28, 0xa904);

    phy_write(phydev, 28, 0xa901);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x0800);
    phy_write(phydev, 28, 0x8520);
    phy_write(phydev, 28, 0xd71f);
    phy_write(phydev, 28, 0x3ce1);
    phy_write(phydev, 28, 0x80e4);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0c72);
    phy_write(phydev, 28, 0xa208);
    phy_write(phydev, 28, 0xc001);
    phy_write(phydev, 28, 0xd710);
    phy_write(phydev, 28, 0x60a0);
    phy_write(phydev, 28, 0xd71e);
    phy_write(phydev, 28, 0x7fac);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0bba);
    phy_write(phydev, 28, 0x8208);

    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x0bc1);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x0722);
    phy_write(phydev, 28, 0x1000);
    phy_write(phydev, 28, 0x07a1);
    phy_write(phydev, 28, 0x1800);
    phy_write(phydev, 28, 0x03d9);
    phy_write(phydev, 27, 0xa026);
    phy_write(phydev, 28, 0x03d7);
    phy_write(phydev, 27, 0xa024);
    phy_write(phydev, 28, 0x0c71);
    phy_write(phydev, 27, 0xa022);
    phy_write(phydev, 28, 0x0a69);
    phy_write(phydev, 27, 0xa020);
    phy_write(phydev, 28, 0x047d);
    phy_write(phydev, 27, 0xa006);
    phy_write(phydev, 28, 0x0722);
    phy_write(phydev, 27, 0xa004);
    phy_write(phydev, 28, 0x0375);
    phy_write(phydev, 27, 0xa002);
    phy_write(phydev, 28, 0x00a2);
    phy_write(phydev, 27, 0xa000);
    phy_write(phydev, 28, 0x0d18);
    phy_write(phydev, 27, 0xa008);
    phy_write(phydev, 28, 0xff00);

    phy_write(phydev, 31, 0x0b82);
    phy_write(phydev, 16, 0x0010);

    phy_write(phydev, 27, 0x8465);
    phy_write(phydev, 28, 0xaf84);
    phy_write(phydev, 28, 0x7daf);
    phy_write(phydev, 28, 0x84ed);
    phy_write(phydev, 28, 0xaf84);
    phy_write(phydev, 28, 0xf0af);
    phy_write(phydev, 28, 0x84f3);
    phy_write(phydev, 28, 0xaf85);
    phy_write(phydev, 28, 0x02af);
    phy_write(phydev, 28, 0x8509);
    phy_write(phydev, 28, 0xaf85);
    phy_write(phydev, 28, 0x14af);
    phy_write(phydev, 28, 0x8526);
    phy_write(phydev, 28, 0x0211);
    phy_write(phydev, 28, 0x8fbf);
    phy_write(phydev, 28, 0x5335);

    phy_write(phydev, 28, 0x0252);
    phy_write(phydev, 28, 0x00ef);
    phy_write(phydev, 28, 0x21bf);
    phy_write(phydev, 28, 0x533b);
    phy_write(phydev, 28, 0x0252);
    phy_write(phydev, 28, 0x001e);
    phy_write(phydev, 28, 0x21bf);
    phy_write(phydev, 28, 0x5338);
    phy_write(phydev, 28, 0x0252);
    phy_write(phydev, 28, 0x001e);
    phy_write(phydev, 28, 0x21ad);
    phy_write(phydev, 28, 0x3003);
    phy_write(phydev, 28, 0xaf10);
    phy_write(phydev, 28, 0x8402);
    phy_write(phydev, 28, 0x84a4);
    phy_write(phydev, 28, 0xaf10);
    phy_write(phydev, 28, 0x84f8);
    phy_write(phydev, 28, 0xfaef);
    phy_write(phydev, 28, 0x69fb);
    phy_write(phydev, 28, 0xcfbf);
    phy_write(phydev, 28, 0x87f7);
    phy_write(phydev, 28, 0xd819);
    phy_write(phydev, 28, 0xd907);
    phy_write(phydev, 28, 0xbf54);

    phy_write(phydev, 28, 0xf102);
    phy_write(phydev, 28, 0x51bc);
    phy_write(phydev, 28, 0x0719);
    phy_write(phydev, 28, 0xd819);
    phy_write(phydev, 28, 0xd907);
    phy_write(phydev, 28, 0xbf54);
    phy_write(phydev, 28, 0xfa02);
    phy_write(phydev, 28, 0x51bc);
    phy_write(phydev, 28, 0x0719);
    phy_write(phydev, 28, 0xd819);
    phy_write(phydev, 28, 0xd907);
    phy_write(phydev, 28, 0xbf53);
    phy_write(phydev, 28, 0x4d02);
    phy_write(phydev, 28, 0x51bc);
    phy_write(phydev, 28, 0x0719);
    phy_write(phydev, 28, 0xd000);
    phy_write(phydev, 28, 0xd907);
    phy_write(phydev, 28, 0xbf55);
    phy_write(phydev, 28, 0x2d02);
    phy_write(phydev, 28, 0x51bc);
    phy_write(phydev, 28, 0x0719);
    phy_write(phydev, 28, 0xd819);

    phy_write(phydev, 28, 0xd9bf);
    phy_write(phydev, 28, 0x54ee);
    phy_write(phydev, 28, 0x0251);
    phy_write(phydev, 28, 0xbcc7);
    phy_write(phydev, 28, 0xffef);
    phy_write(phydev, 28, 0x96fe);

    phy_write(phydev, 28, 0xfc04);
    phy_write(phydev, 28, 0xaf0f);
    phy_write(phydev, 28, 0xccaf);
    phy_write(phydev, 28, 0x0f1c);
    phy_write(phydev, 28, 0x0251);
    phy_write(phydev, 28, 0xbc7c);
    phy_write(phydev, 28, 0x0001);
    phy_write(phydev, 28, 0xbf53);
    phy_write(phydev, 28, 0xda02);
    phy_write(phydev, 28, 0x51bc);
    phy_write(phydev, 28, 0xaf2f);
    phy_write(phydev, 28, 0xc40c);
    phy_write(phydev, 28, 0x5469);
    phy_write(phydev, 28, 0x00af);
    phy_write(phydev, 28, 0x3447);
    phy_write(phydev, 28, 0xbf54);
    phy_write(phydev, 28, 0x19d1);
    phy_write(phydev, 28, 0x0202);
    phy_write(phydev, 28, 0x51bc);
    phy_write(phydev, 28, 0xaf36);
    phy_write(phydev, 28, 0x3fd1);
    phy_write(phydev, 28, 0x10bf);
    phy_write(phydev, 28, 0x565d);
    phy_write(phydev, 28, 0x0251);
    phy_write(phydev, 28, 0xbc02);
    phy_write(phydev, 28, 0x5985);
    phy_write(phydev, 28, 0xef67);
    phy_write(phydev, 28, 0xd100);

    phy_write(phydev, 28, 0xaf35);
    phy_write(phydev, 28, 0x1a00);
    phy_write(phydev, 27, 0xb818);
    phy_write(phydev, 28, 0x1081);
    phy_write(phydev, 27, 0xb81a);
    phy_write(phydev, 28, 0x0fc9);
    phy_write(phydev, 27, 0xb81c);
    phy_write(phydev, 28, 0x0f1a);
    phy_write(phydev, 27, 0xb81e);
    phy_write(phydev, 28, 0x2fc1);
    phy_write(phydev, 27, 0xb846);
    phy_write(phydev, 28, 0x3445);

    phy_write(phydev, 27, 0xb848);
    phy_write(phydev, 28, 0x3639);
    phy_write(phydev, 27, 0xb84a);
    phy_write(phydev, 28, 0x3508);

    phy_write(phydev, 27, 0xb84c);
    phy_write(phydev, 28, 0xffff);
    phy_write(phydev, 27, 0xb832);
    phy_write(phydev, 28, 0x0079);

    phy_write(phydev, 27, 0x801e);
    phy_write(phydev, 28, 0x0004);

    phy_write(phydev, 31, 0x0d08);
    phy_write(phydev, 21, 0x0819);

    phy_write(phydev, 31, 0x0d41);
    phy_write(phydev, 17, 0x1524);
    phy_write(phydev, 18, 0x0724);

    phy_write(phydev, 31, 0xd05);
    phy_write(phydev, 17, 0xc02);
    phy_write(phydev, 31, 0xd05);
    phy_write(phydev, 17, 0xe02);
    phy_write(phydev, 31, 0xbc4);
    phy_write(phydev, 23, 0xc022);
    phy_write(phydev, 31, 0xd05);
    phy_write(phydev, 17, 0xf02);

    phy_write(phydev, 31, 0xbc5);
    phy_write(phydev, 16, 0xb803);
    phy_write(phydev, 31, 0xbc4);
    phy_write(phydev, 22, 0x8100);
    phy_write(phydev, 31, 0xbc4);
    phy_write(phydev, 23, 0x8022);
    phy_write(phydev, 31, 0xbc4);
    phy_write(phydev, 22, 0x8100);

    phy_write(phydev, 31, 0x0a43);
    phy_write(phydev, 27, 0x0000);
    phy_write(phydev, 28, 0x0000);

    phy_write(phydev, 31, 0x0b82);
    phy_write(phydev, 23, 0x0000);

    phy_write(phydev, 27, 0x802b);
    phy_write(phydev, 28, 0x0000);

    phy_write(phydev, 31, 0x0b82);
    phy_write(phydev, 16, 0x0000);

    //read r 16 6 6 0x0 // Check the PHY patched
    mdelay(1);
    value = phy_read(phydev, 16);
    if(((value>>6)&0x1) == 0)
        printk("%s step 2 ok\n", __func__);
    else
        goto step_err;

    phy_write(phydev, 31, 0x0a46);
    phy_write(phydev, 21, 0x0302);

    phy_write(phydev, 27, 0x807d);
    phy_write(phydev, 28, 0xd06b);
    phy_write(phydev, 27, 0x807e);
    phy_write(phydev, 28, 0x6abf);
    phy_write(phydev, 27, 0x807f);
    phy_write(phydev, 28, 0xcf9b);
    phy_write(phydev, 27, 0x809a);
    phy_write(phydev, 28, 0xb294);
    phy_write(phydev, 27, 0x809b);
    phy_write(phydev, 28, 0x970b);
    phy_write(phydev, 27, 0x809c);
    phy_write(phydev, 28, 0xdb18);
    phy_write(phydev, 27, 0x809d);
    phy_write(phydev, 28, 0x04a0);
    phy_write(phydev, 27, 0x80d7);
    phy_write(phydev, 28, 0x5033);

    phy_write(phydev, 27, 0x80aa);
    phy_write(phydev, 28, 0xf06b);
    phy_write(phydev, 27, 0x80ab);
    phy_write(phydev, 28, 0x6abf);
    phy_write(phydev, 27, 0x80ac);
    phy_write(phydev, 28, 0xcf4f);
    phy_write(phydev, 27, 0x80c7);
    phy_write(phydev, 28, 0xb294);
    phy_write(phydev, 27, 0x80c8);
    phy_write(phydev, 28, 0x970b);
    phy_write(phydev, 27, 0x80c9);
    phy_write(phydev, 28, 0xdb18);
    phy_write(phydev, 27, 0x80ca);
    phy_write(phydev, 28, 0x04a0);
    phy_write(phydev, 27, 0x80db);

    phy_write(phydev, 28, 0x5033);
    phy_write(phydev, 27, 0x8011);
    phy_write(phydev, 28, 0x9737);

    phy_write(phydev, 27, 0x804f);
    phy_write(phydev, 28, 0x2422);
    phy_write(phydev, 27, 0x8050);
    phy_write(phydev, 28, 0x44e7);
    phy_write(phydev, 27, 0x8053);
    phy_write(phydev, 28, 0x3300);
    phy_write(phydev, 27, 0x805a);
    phy_write(phydev, 28, 0x3300);
    phy_write(phydev, 27, 0x8061);
    phy_write(phydev, 28, 0x3300);
    phy_write(phydev, 27, 0x81ab);
    phy_write(phydev, 28, 0x9069);

    phy_write(phydev, 31, 0x0a86);
    phy_write(phydev, 17, 0xffff);
    phy_write(phydev, 31, 0x0a80);
    phy_write(phydev, 22, 0x3f46);

    phy_write(phydev, 31, 0x0a81);
    phy_write(phydev, 22, 0x69a7);
    phy_write(phydev, 31, 0x0bc0);
    phy_write(phydev, 20, 0x3461);

    phy_write(phydev, 31, 0x0a92);
    phy_write(phydev, 16, 0x0fff);

    phy_write(phydev, 31, 0xa43);
    phy_write(phydev, 4, 0x1e1);
    phy_write(phydev, 27, 0x87f7);
    phy_write(phydev, 28, 0x700);
    phy_write(phydev, 28, 0x1f8);
    phy_write(phydev, 28, 0x7aff);

    phy_write(phydev, 27, 0x87fd);
    phy_write(phydev, 28, 0xa00);
    phy_write(phydev, 27, 0x87fe);
    phy_write(phydev, 28, 0x08d3);

    phy_write(phydev, 31, 0xa87);
    phy_write(phydev, 20, 0x0);

    phy_write(phydev, 31, 0xa92);
    phy_write(phydev, 16, 0x0001);

    phy_write(phydev, 31, 0xa86);
    phy_write(phydev, 16, 0xc3c0);

    phy_write(phydev, 31, 0xa92);
    phy_write(phydev, 17, 0xc000);

    phy_write(phydev, 31, 0xa93);
    phy_write(phydev, 21, 0x350b);

    phy_write(phydev, 31, 0xa90);
    phy_write(phydev, 17, 0x81a);

    phy_write(phydev, 31, 0xa5d);
    phy_write(phydev, 16, 0x0);

    phy_write(phydev, 27, 0x8165);
    phy_write(phydev, 28, 0xb702);

    phy_write(phydev, 31, 0x0a46);
    phy_write(phydev, 21, 0x0300);

    //step 3
    phy_write(phydev, 27, 0x801e);
    // Check the value is 0x0004
    mdelay(1);
    value = phy_read(phydev, 28);
    if(value == 0x0004)
        printk("%s step 3 ok\n", __func__);
    else
        goto step_err;

    //Restore 1000M
    phy_write(phydev, 31, 0x0d08);
    phy_write(phydev, 21, 0x0811);

step_err:

#endif
    return 0;

}
EXPORT_SYMBOL(rtl8211fd_soft_reset);

static int rtl8211b_suspend(struct phy_device *phydev)
{
	phy_write(phydev, MII_MMD_DATA, BIT(9));

	return genphy_suspend(phydev);
}

static int rtl8211b_resume(struct phy_device *phydev)
{
	phy_write(phydev, MII_MMD_DATA, 0);

	return genphy_resume(phydev);
}

static int rtl8366rb_config_init(struct phy_device *phydev)
{
	int ret;

	ret = genphy_config_init(phydev);
	if (ret < 0)
		return ret;

	ret = phy_set_bits(phydev, RTL8366RB_POWER_SAVE,
			   RTL8366RB_POWER_SAVE_ON);
	if (ret) {
		dev_err(&phydev->mdio.dev,
			"error enabling power management\n");
	}

	return ret;
}

static struct phy_driver realtek_drvs[] = {
	{
		.phy_id         = 0x00008201,
		.name           = "RTL8201CP Ethernet",
		.phy_id_mask    = 0x0000ffff,
		.features       = PHY_BASIC_FEATURES,
		.flags          = PHY_HAS_INTERRUPT,
	}, {
		.phy_id		= 0x001cc816,
		.name		= "RTL8201F Fast Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_BASIC_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= &rtl8201_ack_interrupt,
		.config_intr	= &rtl8201_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id		= 0x001cc910,
		.name		= "RTL8211 Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.config_aneg	= rtl8211_config_aneg,
		.read_mmd	= &genphy_read_mmd_unsupported,
		.write_mmd	= &genphy_write_mmd_unsupported,
	}, {
		.phy_id		= 0x001cc912,
		.name		= "RTL8211B Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211b_config_intr,
		.read_mmd	= &genphy_read_mmd_unsupported,
		.write_mmd	= &genphy_write_mmd_unsupported,
		.suspend	= rtl8211b_suspend,
		.resume		= rtl8211b_resume,
	}, {
		.phy_id		= 0x001cc913,
		.name		= "RTL8211C Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.config_init	= rtl8211c_config_init,
		.read_mmd	= &genphy_read_mmd_unsupported,
		.write_mmd	= &genphy_write_mmd_unsupported,
	}, {
		.phy_id		= 0x001cc914,
		.name		= "RTL8211DN Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= rtl821x_ack_interrupt,
		.config_intr	= rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, {
/*		.phy_id		= 0x001cc915,
		.name		= "RTL8211E Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.ack_interrupt	= &rtl821x_ack_interrupt,
		.config_intr	= &rtl8211e_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	}, { */
		.phy_id		= 0x001cc916,
		.name		= "RTL8211F Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_init	= &rtl8211f_config_init,
//		.ack_interrupt	= &rtl8211f_ack_interrupt,
//		.config_intr	= &rtl8211f_config_intr,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
		.read_page	= rtl821x_read_page,
		.write_page	= rtl821x_write_page,
	}, {
		.phy_id         = 0x001cc859,
		.name           = "RTL8211FD-VX Ethernet",
		.phy_id_mask    = 0x001fffff,
		.features   = PHY_GBIT_FEATURES,
		.flags      = PHY_HAS_INTERRUPT,
		//.soft_reset = rtl8211fd_soft_reset,
		.config_aneg    = &genphy_config_aneg,
		.config_init    = &rtl8211fd_config_init,
		.read_status    = &genphy_read_status,
		.suspend    = genphy_suspend,
		.resume     = genphy_resume,
	}, {
		.phy_id		= 0x001cc961,
		.name		= "RTL8366RB Gigabit Ethernet",
		.phy_id_mask	= 0x001fffff,
		.features	= PHY_GBIT_FEATURES,
		.flags		= PHY_HAS_INTERRUPT,
		.config_init	= &rtl8366rb_config_init,
		.suspend	= genphy_suspend,
		.resume		= genphy_resume,
	},
};

module_phy_driver(realtek_drvs);

static struct mdio_device_id __maybe_unused realtek_tbl[] = {
	{ 0x001cc816, 0x001fffff },
	{ 0x001cc910, 0x001fffff },
	{ 0x001cc912, 0x001fffff },
	{ 0x001cc913, 0x001fffff },
	{ 0x001cc914, 0x001fffff },
//	{ 0x001cc915, 0x001fffff },
//	{ 0x001cc916, 0x001fffff },
	{ 0x001cc961, 0x001fffff },
	{ 0x001cc859, 0x001fffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, realtek_tbl);
