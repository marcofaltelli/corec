/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_yt.h"

#define YT_PHY_RST_WAIT_PERIOD		5

s32 ngbe_read_phy_reg_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	ngbe_mdi_map_register(&reg, &reg22);

	/* Read MII reg according to media type */
	if (hw->phy.media_type == ngbe_media_type_fiber) {
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, YT_SMI_PHY_SDS);
		ngbe_read_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, 0);
	} else {
		ngbe_read_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
	}

	return 0;
}

s32 ngbe_write_phy_reg_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	ngbe_mdi_map_register(&reg, &reg22);

	/* Write MII reg according to media type */
	if (hw->phy.media_type == ngbe_media_type_fiber) {
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, YT_SMI_PHY_SDS);
		ngbe_write_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, 0);
	} else {
		ngbe_write_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
	}

	return 0;
}

s32 ngbe_read_phy_reg_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	ngbe_write_phy_reg_mdi(hw, 0x1E, device_type, reg_addr);
	ngbe_read_phy_reg_mdi(hw, 0x1F, device_type, phy_data);

	return 0;
}

s32 ngbe_write_phy_reg_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	ngbe_write_phy_reg_mdi(hw, 0x1E, device_type, reg_addr);
	ngbe_write_phy_reg_mdi(hw, 0x1F, device_type, phy_data);

	return 0;
}

s32 ngbe_read_phy_reg_sds_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, device_type, YT_SMI_PHY_SDS);
	ngbe_read_phy_reg_ext_yt(hw, reg_addr, device_type, phy_data);
	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, device_type, 0);

	return 0;
}

s32 ngbe_write_phy_reg_sds_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, device_type, YT_SMI_PHY_SDS);
	ngbe_write_phy_reg_ext_yt(hw, reg_addr, device_type, phy_data);
	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, device_type, 0);

	return 0;
}

s32 ngbe_init_phy_yt(struct ngbe_hw *hw)
{
	u16 value = 0;

	DEBUGFUNC("ngbe_init_phy_yt");

	/* close sds area register */
	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, 0, 0);
	/* enable interrupts */
	ngbe_write_phy_reg_mdi(hw, YT_INTR, 0,
				YT_INTR_ENA_MASK | YT_SDS_INTR_ENA_MASK);

	/* power down in fiber mode */
	hw->phy.read_reg(hw, YT_BCR, 0, &value);
	value |= YT_BCR_PWDN;
	hw->phy.write_reg(hw, YT_BCR, 0, value);

	/* power down in UTP mode */
	ngbe_read_phy_reg_mdi(hw, YT_BCR, 0, &value);
	value |= YT_BCR_PWDN;
	ngbe_write_phy_reg_mdi(hw, YT_BCR, 0, value);

	return 0;
}

s32 ngbe_setup_phy_link_yt(struct ngbe_hw *hw, u32 speed,
				bool autoneg_wait_to_complete)
{
	u16 value_r4 = 0;
	u16 value_r9 = 0;
	u16 value;

	DEBUGFUNC("ngbe_setup_phy_link_yt");
	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	hw->phy.autoneg_advertised = 0;

	/* check chip_mode first */
	ngbe_read_phy_reg_ext_yt(hw, YT_CHIP, 0, &value);
	if ((value & YT_CHIP_MODE_MASK) == YT_CHIP_MODE_SEL(0)) {
		/* UTP to rgmii */
		if (!hw->mac.autoneg) {
			switch (speed) {
			case NGBE_LINK_SPEED_1GB_FULL:
				value = YT_BCR_SPEED_SELECT1;
				break;
			case NGBE_LINK_SPEED_100M_FULL:
				value = YT_BCR_SPEED_SELECT0;
				break;
			case NGBE_LINK_SPEED_10M_FULL:
				value = 0;
				break;
			default:
				value = YT_BCR_SPEED_SELECT0 |
					YT_BCR_SPEED_SELECT1;
				DEBUGOUT("unknown speed = 0x%x.\n",
					speed);
				break;
			}
			/* duplex full */
			value |= YT_BCR_DUPLEX | YT_BCR_RESET;
			hw->phy.write_reg(hw, YT_BCR, 0, value);

			goto skip_an;
		}

		/*disable 100/10base-T Self-negotiation ability*/
		hw->phy.read_reg(hw, YT_ANA, 0, &value);
		value &= ~(YT_ANA_100BASET_FULL | YT_ANA_100BASET_HALF |
			YT_ANA_10BASET_FULL | YT_ANA_10BASET_HALF);
		hw->phy.write_reg(hw, YT_ANA, 0, value);

		/*disable 1000base-T Self-negotiation ability*/
		hw->phy.read_reg(hw, YT_MS_CTRL, 0, &value);
		value &= ~(YT_MS_1000BASET_FULL | YT_MS_1000BASET_HALF);
		hw->phy.write_reg(hw, YT_MS_CTRL, 0, value);

		if (speed & NGBE_LINK_SPEED_1GB_FULL) {
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_1GB_FULL;
			value_r9 |= YT_MS_1000BASET_FULL;
		}
		if (speed & NGBE_LINK_SPEED_100M_FULL) {
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_100M_FULL;
			value_r4 |= YT_ANA_100BASET_FULL;
		}
		if (speed & NGBE_LINK_SPEED_10M_FULL) {
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_10M_FULL;
			value_r4 |= YT_ANA_10BASET_FULL;
		}

		/* enable 1000base-T Self-negotiation ability */
		hw->phy.read_reg(hw, YT_MS_CTRL, 0, &value);
		value |= value_r9;
		hw->phy.write_reg(hw, YT_MS_CTRL, 0, value);

		/* enable 100/10base-T Self-negotiation ability */
		hw->phy.read_reg(hw, YT_ANA, 0, &value);
		value |= value_r4;
		hw->phy.write_reg(hw, YT_ANA, 0, value);

		/* software reset to make the above configuration take effect*/
		hw->phy.read_reg(hw, YT_BCR, 0, &value);
		value |= YT_BCR_RESET | YT_BCR_ANE | YT_BCR_RESTART_AN;
		hw->phy.write_reg(hw, YT_BCR, 0, value);
skip_an:
		/* power on in UTP mode */
		ngbe_read_phy_reg_mdi(hw, YT_BCR, 0, &value);
		value &= ~YT_BCR_PWDN;
		ngbe_write_phy_reg_mdi(hw, YT_BCR, 0, value);
	} else if ((value & YT_CHIP_MODE_MASK) == YT_CHIP_MODE_SEL(1)) {
		/* fiber to rgmii */
		hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_1GB_FULL;

		/* RGMII_Config1 : Config rx and tx training delay */
		value = YT_RGMII_CONF1_RXDELAY |
			YT_RGMII_CONF1_TXDELAY_FE |
			YT_RGMII_CONF1_TXDELAY;
		ngbe_write_phy_reg_ext_yt(hw, YT_RGMII_CONF1, 0, value);
		value = YT_CHIP_MODE_SEL(1) |
			YT_CHIP_SW_LDO_EN |
			YT_CHIP_SW_RST;
		ngbe_write_phy_reg_ext_yt(hw, YT_CHIP, 0, value);

		/* software reset */
		ngbe_write_phy_reg_sds_ext_yt(hw, 0x0, 0, 0x9140);

		/* power on phy */
		hw->phy.read_reg(hw, YT_BCR, 0, &value);
		value &= ~YT_BCR_PWDN;
		hw->phy.write_reg(hw, YT_BCR, 0, value);
	} else if ((value & YT_CHIP_MODE_MASK) == YT_CHIP_MODE_SEL(2)) {
		/* power on in UTP mode */
		ngbe_read_phy_reg_mdi(hw, YT_BCR, 0, &value);
		value &= ~YT_BCR_PWDN;
		ngbe_write_phy_reg_mdi(hw, YT_BCR, 0, value);
		/* power down in fiber mode */
		hw->phy.read_reg(hw, YT_BCR, 0, &value);
		value &= ~YT_BCR_PWDN;
		hw->phy.write_reg(hw, YT_BCR, 0, value);

		hw->phy.read_reg(hw, YT_SPST, 0, &value);
		if (value & YT_SPST_LINK) {
			/* fiber up */
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_1GB_FULL;
		} else {
			/* utp up */
			/*disable 100/10base-T Self-negotiation ability*/
			hw->phy.read_reg(hw, YT_ANA, 0, &value);
			value &= ~(YT_ANA_100BASET_FULL | YT_ANA_100BASET_HALF |
				YT_ANA_10BASET_FULL | YT_ANA_10BASET_HALF);
			hw->phy.write_reg(hw, YT_ANA, 0, value);

			/*disable 1000base-T Self-negotiation ability*/
			hw->phy.read_reg(hw, YT_MS_CTRL, 0, &value);
			value &= ~(YT_MS_1000BASET_FULL | YT_MS_1000BASET_HALF);
			hw->phy.write_reg(hw, YT_MS_CTRL, 0, value);

			if (speed & NGBE_LINK_SPEED_1GB_FULL) {
				hw->phy.autoneg_advertised |=
						NGBE_LINK_SPEED_1GB_FULL;
				value_r9 |= YT_MS_1000BASET_FULL;
			}
			if (speed & NGBE_LINK_SPEED_100M_FULL) {
				hw->phy.autoneg_advertised |=
						NGBE_LINK_SPEED_100M_FULL;
				value_r4 |= YT_ANA_100BASET_FULL;
			}
			if (speed & NGBE_LINK_SPEED_10M_FULL) {
				hw->phy.autoneg_advertised |=
						NGBE_LINK_SPEED_10M_FULL;
				value_r4 |= YT_ANA_10BASET_FULL;
			}

			/* enable 1000base-T Self-negotiation ability */
			hw->phy.read_reg(hw, YT_MS_CTRL, 0, &value);
			value |= value_r9;
			hw->phy.write_reg(hw, YT_MS_CTRL, 0, value);

			/* enable 100/10base-T Self-negotiation ability */
			hw->phy.read_reg(hw, YT_ANA, 0, &value);
			value |= value_r4;
			hw->phy.write_reg(hw, YT_ANA, 0, value);

			/* software reset to make the above configuration
			 * take effect
			 */
			hw->phy.read_reg(hw, YT_BCR, 0, &value);
			value |= YT_BCR_RESET;
			hw->phy.write_reg(hw, YT_BCR, 0, value);
		}
	} else if ((value & YT_CHIP_MODE_MASK) == YT_CHIP_MODE_SEL(4)) {
		hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_1GB_FULL;

		ngbe_read_phy_reg_ext_yt(hw, YT_RGMII_CONF1, 0, &value);
		value |= YT_RGMII_CONF1_MODE;
		ngbe_write_phy_reg_ext_yt(hw, YT_RGMII_CONF1, 0, value);

		ngbe_read_phy_reg_ext_yt(hw, YT_RGMII_CONF2, 0, &value);
		value &= ~(YT_RGMII_CONF2_SPEED_MASK | YT_RGMII_CONF2_DUPLEX |
			YT_RGMII_CONF2_LINKUP);
		value |= YT_RGMII_CONF2_SPEED(2) | YT_RGMII_CONF2_DUPLEX |
			YT_RGMII_CONF2_LINKUP;
		ngbe_write_phy_reg_ext_yt(hw, YT_RGMII_CONF2, 0, value);

		ngbe_read_phy_reg_ext_yt(hw, YT_CHIP, 0, &value);
		value &= ~YT_SMI_PHY_SW_RST;
		ngbe_write_phy_reg_ext_yt(hw, YT_CHIP, 0, value);

		/* power on phy */
		hw->phy.read_reg(hw, YT_BCR, 0, &value);
		value &= ~YT_BCR_PWDN;
		hw->phy.write_reg(hw, YT_BCR, 0, value);
	}

	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, 0, 0);
	ngbe_read_phy_reg_mdi(hw, YT_INTR_STATUS, 0, &value);

	return 0;
}

s32 ngbe_reset_phy_yt(struct ngbe_hw *hw)
{
	u32 i;
	u16 ctrl = 0;
	s32 status = 0;

	DEBUGFUNC("ngbe_reset_phy_yt");

	if (hw->phy.type != ngbe_phy_yt8521s &&
		hw->phy.type != ngbe_phy_yt8521s_sfi)
		return NGBE_ERR_PHY_TYPE;

	/* check chip_mode first */
	ngbe_read_phy_reg_ext_yt(hw, YT_CHIP, 0, &ctrl);
	if (ctrl & YT_CHIP_MODE_MASK) {
		/* fiber to rgmii */
		status = hw->phy.read_reg(hw, YT_BCR, 0, &ctrl);
		/* sds software reset */
		ctrl |= YT_BCR_RESET;
		status = hw->phy.write_reg(hw, YT_BCR, 0, ctrl);

		for (i = 0; i < YT_PHY_RST_WAIT_PERIOD; i++) {
			status = hw->phy.read_reg(hw, YT_BCR, 0, &ctrl);
			if (!(ctrl & YT_BCR_RESET))
				break;
			msleep(1);
		}
	} else {
		/* UTP to rgmii */
		status = ngbe_read_phy_reg_mdi(hw, YT_BCR, 0, &ctrl);
		/* sds software reset */
		ctrl |= YT_BCR_RESET;
		status = ngbe_write_phy_reg_mdi(hw, YT_BCR, 0, ctrl);

		for (i = 0; i < YT_PHY_RST_WAIT_PERIOD; i++) {
			status = ngbe_read_phy_reg_mdi(hw, YT_BCR, 0, &ctrl);
			if (!(ctrl & YT_BCR_RESET))
				break;
			msleep(1);
		}
	}

	if (i == YT_PHY_RST_WAIT_PERIOD) {
		DEBUGOUT("PHY reset polling failed to complete.\n");
		return NGBE_ERR_RESET_FAILED;
	}

	return status;
}

s32 ngbe_get_phy_advertised_pause_yt(struct ngbe_hw *hw, u8 *pause_bit)
{
	u16 value;
	s32 status = 0;

	DEBUGFUNC("ngbe_get_phy_advertised_pause_yt");

	status = hw->phy.read_reg(hw, YT_ANA, 0, &value);
	value &= YT_FANA_PAUSE_MASK;
	*pause_bit = (u8)(value >> 7);

	return status;
}

s32 ngbe_get_phy_lp_advertised_pause_yt(struct ngbe_hw *hw, u8 *pause_bit)
{
	u16 value;
	s32 status = 0;

	DEBUGFUNC("ngbe_get_phy_lp_advertised_pause_yt");

	status = hw->phy.read_reg(hw, YT_LPAR, 0, &value);
	value &= YT_FLPAR_PAUSE_MASK;
	*pause_bit = (u8)(value >> 7);

	return status;
}

s32 ngbe_set_phy_pause_adv_yt(struct ngbe_hw *hw, u16 pause_bit)
{
	u16 value;
	s32 status = 0;

	DEBUGFUNC("ngbe_set_phy_pause_adv_yt");


	status = hw->phy.read_reg(hw, YT_ANA, 0, &value);
	value &= ~YT_FANA_PAUSE_MASK;
	value |= pause_bit;
	status = hw->phy.write_reg(hw, YT_ANA, 0, value);

	return status;
}

s32 ngbe_check_phy_link_yt(struct ngbe_hw *hw,
		u32 *speed, bool *link_up)
{
	s32 status = 0;
	u16 phy_link = 0;
	u16 phy_speed = 0;
	u16 phy_data = 0;
	u16 insr = 0;

	DEBUGFUNC("ngbe_check_phy_link_yt");

	/* Initialize speed and link to default case */
	*link_up = false;
	*speed = NGBE_LINK_SPEED_UNKNOWN;

	ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY, 0, 0);
	ngbe_read_phy_reg_mdi(hw, YT_INTR_STATUS, 0, &insr);

	status = hw->phy.read_reg(hw, YT_SPST, 0, &phy_data);
	phy_link = phy_data & YT_SPST_LINK;
	phy_speed = phy_data & YT_SPST_SPEED_MASK;

	if (phy_link) {
		*link_up = true;

		if (phy_speed == YT_SPST_SPEED_1000M)
			*speed = NGBE_LINK_SPEED_1GB_FULL;
		else if (phy_speed == YT_SPST_SPEED_100M)
			*speed = NGBE_LINK_SPEED_100M_FULL;
		else if (phy_speed == YT_SPST_SPEED_10M)
			*speed = NGBE_LINK_SPEED_10M_FULL;
	}

	return status;
}

