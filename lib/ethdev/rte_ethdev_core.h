/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_ETHDEV_CORE_H_
#define _RTE_ETHDEV_CORE_H_

#include <pthread.h>

/**
 * @file
 *
 * RTE Ethernet Device internal header.
 *
 * This header contains internal data types. But they are still part of the
 * public API because they are used by inline functions in the published API.
 *
 * Applications should not use these directly.
 *
 */

struct rte_eth_dev_callback;
/** @internal Structure to keep track of registered callbacks */
RTE_TAILQ_HEAD(rte_eth_dev_cb_list, rte_eth_dev_callback);

struct rte_eth_dev;

/**
 * @internal Retrieve input packets from a receive queue of an Ethernet device.
 */
typedef uint16_t (*eth_rx_burst_t)(void *rxq,
				   struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts);

/**
 * @internal Retrieve input packets (possibly in a multiple-consumer way) from a receive queue of an Ethernet device.
 */
typedef uint16_t (*eth_rx_burst_parallel_t)(void *rxq,
								   struct rte_mbuf **rx_pkts,
								   uint16_t nb_pkts);

/**
 * @internal Send output packets on a transmit queue of an Ethernet device.
 */
typedef uint16_t (*eth_tx_burst_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);

/**
 * @internal Prepare output packets on a transmit queue of an Ethernet device.
 */
typedef uint16_t (*eth_tx_prep_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);


/** @internal Get number of used descriptors on a receive queue. */
typedef uint32_t (*eth_rx_queue_count_t)(void *rxq);

/** @internal Extimate number of used descriptors on a receive queue. */
typedef uint16_t (*eth_rx_queue_extimate_t)(void *rxq);

/** @internal Check the status of a Rx descriptor */
typedef int (*eth_rx_descriptor_status_t)(void *rxq, uint16_t offset);

/** @internal Check the status of a Tx descriptor */
typedef int (*eth_tx_descriptor_status_t)(void *txq, uint16_t offset);

/**
 * @internal
 * Structure used to hold opaque pointers to internal ethdev Rx/Tx
 * queues data.
 * The main purpose to expose these pointers at all - allow compiler
 * to fetch this data for fast-path ethdev inline functions in advance.
 */
struct rte_ethdev_qdata {
	/** points to array of internal queue data pointers */
	void **data;
	/** points to array of queue callback data pointers */
	void **clbk;
};

/**
 * @internal
 * fast-path ethdev functions and related data are hold in a flat array.
 * One entry per ethdev.
 * On 64-bit systems contents of this structure occupy exactly two 64B lines.
 * On 32-bit systems contents of this structure fits into one 64B line.
 */
struct rte_eth_fp_ops {

	/**@{*/
	/**
	 * Rx fast-path functions and related data.
	 * 64-bit systems: occupies first 64B line
	 */
	/** PMD receive function. */
	eth_rx_burst_t rx_pkt_burst;
	/** PMD parallel receive function. */
	eth_rx_burst_parallel_t rx_pkt_burst_parallel;
	/** Get the number of used Rx descriptors. */
	eth_rx_queue_count_t rx_queue_count;
    /** Get the number of expected Rx descriptors. */
    eth_rx_queue_extimate_t rx_queue_extimate;
	/** Check the status of a Rx descriptor. */
	eth_rx_descriptor_status_t rx_descriptor_status;
	/** Rx queues data. */
	struct rte_ethdev_qdata rxq;
	uintptr_t reserved1[3];
	/**@}*/

	/**@{*/
	/**
	 * Tx fast-path functions and related data.
	 * 64-bit systems: occupies second 64B line
	 */
	/** PMD transmit function. */
	eth_tx_burst_t tx_pkt_burst;
	/** PMD transmit prepare function. */
	eth_tx_prep_t tx_pkt_prepare;
	/** Check the status of a Tx descriptor. */
	eth_tx_descriptor_status_t tx_descriptor_status;
	/** Tx queues data. */
	struct rte_ethdev_qdata txq;
	uintptr_t reserved2[3];
	/**@}*/

} __rte_cache_aligned;

extern struct rte_eth_fp_ops rte_eth_fp_ops[RTE_MAX_ETHPORTS];

#endif /* _RTE_ETHDEV_CORE_H_ */
