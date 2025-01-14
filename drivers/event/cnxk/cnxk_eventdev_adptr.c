/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"
#include "cnxk_eventdev.h"

void
cnxk_sso_updt_xae_cnt(struct cnxk_sso_evdev *dev, void *data,
		      uint32_t event_type)
{
	int i;

	switch (event_type) {
	case RTE_EVENT_TYPE_ETHDEV: {
		struct cnxk_eth_rxq_sp *rxq = data;
		uint64_t *old_ptr;

		for (i = 0; i < dev->rx_adptr_pool_cnt; i++) {
			if ((uint64_t)rxq->qconf.mp == dev->rx_adptr_pools[i])
				return;
		}

		dev->rx_adptr_pool_cnt++;
		old_ptr = dev->rx_adptr_pools;
		dev->rx_adptr_pools = rte_realloc(
			dev->rx_adptr_pools,
			sizeof(uint64_t) * dev->rx_adptr_pool_cnt, 0);
		if (dev->rx_adptr_pools == NULL) {
			dev->adptr_xae_cnt += rxq->qconf.mp->size;
			dev->rx_adptr_pools = old_ptr;
			dev->rx_adptr_pool_cnt--;
			return;
		}
		dev->rx_adptr_pools[dev->rx_adptr_pool_cnt - 1] =
			(uint64_t)rxq->qconf.mp;

		dev->adptr_xae_cnt += rxq->qconf.mp->size;
		break;
	}
	case RTE_EVENT_TYPE_ETHDEV_VECTOR: {
		struct rte_mempool *mp = data;
		uint64_t *old_ptr;

		for (i = 0; i < dev->vec_pool_cnt; i++) {
			if ((uint64_t)mp == dev->vec_pools[i])
				return;
		}

		dev->vec_pool_cnt++;
		old_ptr = dev->vec_pools;
		dev->vec_pools =
			rte_realloc(dev->vec_pools,
				    sizeof(uint64_t) * dev->vec_pool_cnt, 0);
		if (dev->vec_pools == NULL) {
			dev->adptr_xae_cnt += mp->size;
			dev->vec_pools = old_ptr;
			dev->vec_pool_cnt--;
			return;
		}
		dev->vec_pools[dev->vec_pool_cnt - 1] = (uint64_t)mp;

		dev->adptr_xae_cnt += mp->size;
		break;
	}
	case RTE_EVENT_TYPE_TIMER: {
		struct cnxk_tim_ring *timr = data;
		uint16_t *old_ring_ptr;
		uint64_t *old_sz_ptr;

		for (i = 0; i < dev->tim_adptr_ring_cnt; i++) {
			if (timr->ring_id != dev->timer_adptr_rings[i])
				continue;
			if (timr->nb_timers == dev->timer_adptr_sz[i])
				return;
			dev->adptr_xae_cnt -= dev->timer_adptr_sz[i];
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_sz[i] = timr->nb_timers;

			return;
		}

		dev->tim_adptr_ring_cnt++;
		old_ring_ptr = dev->timer_adptr_rings;
		old_sz_ptr = dev->timer_adptr_sz;

		dev->timer_adptr_rings = rte_realloc(
			dev->timer_adptr_rings,
			sizeof(uint16_t) * dev->tim_adptr_ring_cnt, 0);
		if (dev->timer_adptr_rings == NULL) {
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_rings = old_ring_ptr;
			dev->tim_adptr_ring_cnt--;
			return;
		}

		dev->timer_adptr_sz = rte_realloc(
			dev->timer_adptr_sz,
			sizeof(uint64_t) * dev->tim_adptr_ring_cnt, 0);

		if (dev->timer_adptr_sz == NULL) {
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_sz = old_sz_ptr;
			dev->tim_adptr_ring_cnt--;
			return;
		}

		dev->timer_adptr_rings[dev->tim_adptr_ring_cnt - 1] =
			timr->ring_id;
		dev->timer_adptr_sz[dev->tim_adptr_ring_cnt - 1] =
			timr->nb_timers;

		dev->adptr_xae_cnt += timr->nb_timers;
		break;
	}
	default:
		break;
	}
}

static int
cnxk_sso_rxq_enable(struct cnxk_eth_dev *cnxk_eth_dev, uint16_t rq_id,
		    uint16_t port_id, const struct rte_event *ev,
		    uint8_t custom_flowid)
{
	struct roc_nix *nix = &cnxk_eth_dev->nix;
	struct roc_nix_rq *rq;
	int rc;

	rq = &cnxk_eth_dev->rqs[rq_id];
	rq->sso_ena = 1;
	rq->tt = ev->sched_type;
	rq->hwgrp = ev->queue_id;
	rq->flow_tag_width = 20;
	rq->wqe_skip = 1;
	rq->tag_mask = (port_id & 0xF) << 20;
	rq->tag_mask |= (((port_id >> 4) & 0xF) | (RTE_EVENT_TYPE_ETHDEV << 4))
			<< 24;

	if (custom_flowid) {
		rq->flow_tag_width = 0;
		rq->tag_mask |= ev->flow_id;
	}

	rc = roc_nix_rq_modify(&cnxk_eth_dev->nix, rq, 0);
	if (rc)
		return rc;

	if (rq_id == 0 && roc_nix_inl_inb_is_enabled(nix)) {
		uint32_t sec_tag_const;

		/* IPSec tag const is 8-bit left shifted value of tag_mask
		 * as it applies to bit 32:8 of tag only.
		 */
		sec_tag_const = rq->tag_mask >> 8;
		rc = roc_nix_inl_inb_tag_update(nix, sec_tag_const,
						ev->sched_type);
		if (rc)
			plt_err("Failed to set tag conf for ipsec, rc=%d", rc);
	}

	return rc;
}

static int
cnxk_sso_rxq_disable(struct cnxk_eth_dev *cnxk_eth_dev, uint16_t rq_id)
{
	struct roc_nix_rq *rq;

	rq = &cnxk_eth_dev->rqs[rq_id];
	rq->sso_ena = 0;
	rq->flow_tag_width = 32;
	rq->tag_mask = 0;

	return roc_nix_rq_modify(&cnxk_eth_dev->nix, rq, 0);
}

static int
cnxk_sso_rx_adapter_vwqe_enable(struct cnxk_eth_dev *cnxk_eth_dev,
				uint16_t port_id, uint16_t rq_id, uint16_t sz,
				uint64_t tmo_ns, struct rte_mempool *vmp)
{
	struct roc_nix_rq *rq;

	rq = &cnxk_eth_dev->rqs[rq_id];

	if (!rq->sso_ena)
		return -EINVAL;
	if (rq->flow_tag_width == 0)
		return -EINVAL;

	rq->vwqe_ena = 1;
	rq->vwqe_first_skip = 0;
	rq->vwqe_aura_handle = roc_npa_aura_handle_to_aura(vmp->pool_id);
	rq->vwqe_max_sz_exp = rte_log2_u32(sz);
	rq->vwqe_wait_tmo =
		tmo_ns /
		((roc_nix_get_vwqe_interval(&cnxk_eth_dev->nix) + 1) * 100);
	rq->tag_mask = (port_id & 0xF) << 20;
	rq->tag_mask |=
		(((port_id >> 4) & 0xF) | (RTE_EVENT_TYPE_ETHDEV_VECTOR << 4))
		<< 24;

	return roc_nix_rq_modify(&cnxk_eth_dev->nix, rq, 0);
}

int
cnxk_sso_rx_adapter_queue_add(
	const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
	int32_t rx_queue_id,
	const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t port = eth_dev->data->port_id;
	struct cnxk_eth_rxq_sp *rxq_sp;
	int i, rc = 0;

	if (rx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
			rc |= cnxk_sso_rx_adapter_queue_add(event_dev, eth_dev,
							    i, queue_conf);
	} else {
		rxq_sp = cnxk_eth_rxq_to_sp(
			eth_dev->data->rx_queues[rx_queue_id]);
		cnxk_sso_updt_xae_cnt(dev, rxq_sp, RTE_EVENT_TYPE_ETHDEV);
		rc = cnxk_sso_xae_reconfigure(
			(struct rte_eventdev *)(uintptr_t)event_dev);
		rc |= cnxk_sso_rxq_enable(
			cnxk_eth_dev, (uint16_t)rx_queue_id, port,
			&queue_conf->ev,
			!!(queue_conf->rx_queue_flags &
			   RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID));
		if (queue_conf->rx_queue_flags &
		    RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR) {
			cnxk_sso_updt_xae_cnt(dev, queue_conf->vector_mp,
					      RTE_EVENT_TYPE_ETHDEV_VECTOR);
			rc |= cnxk_sso_xae_reconfigure(
				(struct rte_eventdev *)(uintptr_t)event_dev);
			rc |= cnxk_sso_rx_adapter_vwqe_enable(
				cnxk_eth_dev, port, rx_queue_id,
				queue_conf->vector_sz,
				queue_conf->vector_timeout_ns,
				queue_conf->vector_mp);

			if (cnxk_eth_dev->vec_drop_re_dis)
				rc |= roc_nix_rx_drop_re_set(&cnxk_eth_dev->nix,
							     false);
		}
		rox_nix_fc_npa_bp_cfg(&cnxk_eth_dev->nix,
				      rxq_sp->qconf.mp->pool_id, true,
				      dev->force_ena_bp);
		cnxk_eth_dev->nb_rxq_sso++;
	}

	if (rc < 0) {
		plt_err("Failed to configure Rx adapter port=%d, q=%d", port,
			queue_conf->ev.queue_id);
		return rc;
	}

	dev->rx_offloads |= cnxk_eth_dev->rx_offload_flags;

	/* Switch to use PF/VF's NIX LF instead of inline device for inbound
	 * when all the RQ's are switched to event dev mode. We do this only
	 * when using inline device is not forced by dev args.
	 */
	if (!cnxk_eth_dev->inb.force_inl_dev &&
	    cnxk_eth_dev->nb_rxq_sso == cnxk_eth_dev->nb_rxq)
		cnxk_nix_inb_mode_set(cnxk_eth_dev, false);

	return 0;
}

int
cnxk_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t rx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cnxk_eth_rxq_sp *rxq_sp;
	int i, rc = 0;

	RTE_SET_USED(event_dev);
	if (rx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
			cnxk_sso_rx_adapter_queue_del(event_dev, eth_dev, i);
	} else {
		rxq_sp = cnxk_eth_rxq_to_sp(
			eth_dev->data->rx_queues[rx_queue_id]);
		rc = cnxk_sso_rxq_disable(cnxk_eth_dev, (uint16_t)rx_queue_id);
		rox_nix_fc_npa_bp_cfg(&cnxk_eth_dev->nix,
				      rxq_sp->qconf.mp->pool_id, false,
				      dev->force_ena_bp);
		cnxk_eth_dev->nb_rxq_sso--;

		/* Enable drop_re if it was disabled earlier */
		if (cnxk_eth_dev->vec_drop_re_dis && !cnxk_eth_dev->nb_rxq_sso)
			rc |= roc_nix_rx_drop_re_set(&cnxk_eth_dev->nix, true);
	}

	if (rc < 0)
		plt_err("Failed to clear Rx adapter config port=%d, q=%d",
			eth_dev->data->port_id, rx_queue_id);

	/* Removing RQ from Rx adapter implies need to use
	 * inline device for CQ/Poll mode.
	 */
	cnxk_nix_inb_mode_set(cnxk_eth_dev, true);

	return rc;
}

int
cnxk_sso_rx_adapter_start(const struct rte_eventdev *event_dev,
			  const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

int
cnxk_sso_rx_adapter_stop(const struct rte_eventdev *event_dev,
			 const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

static int
cnxk_sso_sqb_aura_limit_edit(struct roc_nix_sq *sq, uint16_t nb_sqb_bufs)
{
	return roc_npa_aura_limit_modify(
		sq->aura_handle, RTE_MIN(nb_sqb_bufs, sq->aura_sqb_bufs));
}

static void
cnxk_sso_tx_queue_data_init(struct cnxk_sso_evdev *dev, uint64_t *txq_data,
			    uint16_t eth_port_id, uint16_t tx_queue_id)
{
	uint64_t offset = 0;
	int i;

	dev->max_queue_id[0] = RTE_MAX(dev->max_queue_id[0], eth_port_id);
	for (i = 1; i < eth_port_id; i++) {
		offset += (dev->max_queue_id[i - 1] + 1);
		txq_data[i] |= offset << 48;
	}
	dev->max_port_id = RTE_MAX(dev->max_port_id, eth_port_id);
	dev->max_queue_id[eth_port_id] =
		RTE_MAX(dev->max_queue_id[eth_port_id], tx_queue_id);
}

static void
cnxk_sso_tx_queue_data_cpy(struct cnxk_sso_evdev *dev, uint64_t *txq_data,
			   uint64_t *otxq_data, uint16_t eth_port_id)
{
	uint64_t offset = 0;
	int i, j;

	for (i = 1; i < eth_port_id; i++) {
		offset += (dev->max_queue_id[i - 1] + 1);
		txq_data[i] |= offset << 48;
		for (j = 0;
		     (i < dev->max_port_id) && (j < dev->max_queue_id[i] + 1);
		     j++)
			txq_data[offset + j] =
				otxq_data[(otxq_data[i] >> 48) + j];
	}
}

static void
cnxk_sso_tx_queue_data_cpy_max(struct cnxk_sso_evdev *dev, uint64_t *txq_data,
			       uint64_t *otxq_data, uint16_t eth_port_id,
			       uint16_t max_port_id, uint16_t max_queue_id)
{
	uint64_t offset = 0;
	int i, j;

	for (i = 1; i < max_port_id + 1; i++) {
		offset += (dev->max_queue_id[i - 1] + 1);
		txq_data[i] |= offset << 48;
		for (j = 0; j < dev->max_queue_id[i] + 1; j++) {
			if (i == eth_port_id && j > max_queue_id)
				continue;
			txq_data[offset + j] =
				otxq_data[(otxq_data[i] >> 48) + j];
		}
	}
}

static void
cnxk_sso_tx_queue_data_rewrite(struct cnxk_sso_evdev *dev, uint64_t *txq_data,
			       uint16_t eth_port_id, uint16_t tx_queue_id,
			       uint64_t *otxq_data, uint16_t max_port_id,
			       uint16_t max_queue_id)
{
	int i;

	for (i = 0; i < dev->max_queue_id[0] + 1; i++)
		txq_data[i] |= (otxq_data[i] & ~((BIT_ULL(16) - 1) << 48));

	if (eth_port_id > max_port_id) {
		dev->max_queue_id[0] =
			RTE_MAX(dev->max_queue_id[0], eth_port_id);
		dev->max_port_id = RTE_MAX(dev->max_port_id, eth_port_id);

		cnxk_sso_tx_queue_data_cpy(dev, txq_data, otxq_data,
					   eth_port_id);
		dev->max_queue_id[eth_port_id] =
			RTE_MAX(dev->max_queue_id[eth_port_id], tx_queue_id);
	} else if (tx_queue_id > max_queue_id) {
		dev->max_queue_id[eth_port_id] =
			RTE_MAX(dev->max_queue_id[eth_port_id], tx_queue_id);
		dev->max_port_id = RTE_MAX(max_port_id, eth_port_id);
		cnxk_sso_tx_queue_data_cpy_max(dev, txq_data, otxq_data,
					       eth_port_id, max_port_id,
					       max_queue_id);
	}
}

static void
cnxk_sso_tx_queue_data_sz(struct cnxk_sso_evdev *dev, uint16_t eth_port_id,
			  uint16_t tx_queue_id, uint16_t max_port_id,
			  uint16_t max_queue_id, uint64_t *r, size_t *sz)
{
	uint64_t row = 0;
	size_t size = 0;
	int i;

	if (dev->tx_adptr_data == NULL) {
		size = (eth_port_id + 1);
		size += (eth_port_id + tx_queue_id);
		row = 2 * eth_port_id;
		*r = row;
		*sz = size;
		return;
	}

	if (eth_port_id > max_port_id) {
		size = (RTE_MAX(eth_port_id, dev->max_queue_id[0]) + 1);
		for (i = 1; i < eth_port_id; i++)
			size += (dev->max_queue_id[i] + 1);
		row = size;
		size += (tx_queue_id + 1);
	} else if (tx_queue_id > max_queue_id) {
		size = !eth_port_id ?
			       tx_queue_id + 1 :
				     RTE_MAX(max_port_id, dev->max_queue_id[0]) + 1;
		for (i = 1; i < max_port_id + 1; i++) {
			if (i == eth_port_id) {
				row = size;
				size += tx_queue_id + 1;
			} else {
				size += dev->max_queue_id[i] + 1;
			}
		}
	}
	*r = row;
	*sz = size;
}

static int
cnxk_sso_updt_tx_queue_data(const struct rte_eventdev *event_dev,
			    uint16_t eth_port_id, uint16_t tx_queue_id,
			    void *txq)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t max_queue_id = dev->max_queue_id[eth_port_id];
	uint16_t max_port_id = dev->max_port_id;
	uint64_t *txq_data = NULL;
	uint64_t row = 0;
	size_t size = 0;

	if (((uint64_t)txq) & 0xFFFF000000000000)
		return -EINVAL;

	cnxk_sso_tx_queue_data_sz(dev, eth_port_id, tx_queue_id, max_port_id,
				  max_queue_id, &row, &size);

	size *= sizeof(uint64_t);

	if (size) {
		uint64_t *otxq_data = dev->tx_adptr_data;

		txq_data = malloc(size);
		if (txq_data == NULL)
			return -ENOMEM;
		memset(txq_data, 0, size);
		txq_data[eth_port_id] = ((uint64_t)row) << 48;
		txq_data[row + tx_queue_id] = (uint64_t)txq;

		if (otxq_data != NULL)
			cnxk_sso_tx_queue_data_rewrite(
				dev, txq_data, eth_port_id, tx_queue_id,
				otxq_data, max_port_id, max_queue_id);
		else
			cnxk_sso_tx_queue_data_init(dev, txq_data, eth_port_id,
						    tx_queue_id);
		dev->tx_adptr_data_sz = size;
		free(otxq_data);
		dev->tx_adptr_data = txq_data;
	} else {
		txq_data = dev->tx_adptr_data;
		row = txq_data[eth_port_id] >> 48;
		txq_data[row + tx_queue_id] &= ~(BIT_ULL(48) - 1);
		txq_data[row + tx_queue_id] |= (uint64_t)txq;
	}

	return 0;
}

int
cnxk_sso_tx_adapter_queue_add(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t tx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct roc_nix_sq *sq;
	int i, ret;
	void *txq;

	if (tx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
			cnxk_sso_tx_adapter_queue_add(event_dev, eth_dev, i);
	} else {
		txq = eth_dev->data->tx_queues[tx_queue_id];
		sq = &cnxk_eth_dev->sqs[tx_queue_id];
		cnxk_sso_sqb_aura_limit_edit(sq, CNXK_SSO_SQB_LIMIT);
		ret = cnxk_sso_updt_tx_queue_data(
			event_dev, eth_dev->data->port_id, tx_queue_id, txq);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int
cnxk_sso_tx_adapter_queue_del(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t tx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct roc_nix_sq *sq;
	int i, ret;

	RTE_SET_USED(event_dev);
	if (tx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
			cnxk_sso_tx_adapter_queue_del(event_dev, eth_dev, i);
	} else {
		sq = &cnxk_eth_dev->sqs[tx_queue_id];
		cnxk_sso_sqb_aura_limit_edit(sq, sq->nb_sqb_bufs);
		ret = cnxk_sso_updt_tx_queue_data(
			event_dev, eth_dev->data->port_id, tx_queue_id, NULL);
		if (ret < 0)
			return ret;
	}

	return 0;
}
