/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation.
 */

#ifdef RTE_NET_AF_XDP_LIBXDP
#include <xdp/xsk.h>
#else
#include <bpf/xsk.h>
#endif
#include <linux/version.h>
#include <poll.h>

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE && \
	defined(RTE_NET_AF_XDP_SHARED_UMEM)
#define ETH_AF_XDP_SHARED_UMEM 1
#endif

#ifdef ETH_AF_XDP_SHARED_UMEM
static __rte_always_inline int
create_shared_socket(struct xsk_socket **xsk_ptr,
			  const char *ifname,
			  __u32 queue_id, struct xsk_umem *umem,
			  struct xsk_ring_cons *rx,
			  struct xsk_ring_prod *tx,
			  struct xsk_ring_prod *fill,
			  struct xsk_ring_cons *comp,
			  const struct xsk_socket_config *config)
{
	return xsk_socket__create_shared(xsk_ptr, ifname, queue_id, umem, rx,
						tx, fill, comp, config);
}
#else
static __rte_always_inline int
create_shared_socket(struct xsk_socket **xsk_ptr __rte_unused,
			  const char *ifname __rte_unused,
			  __u32 queue_id __rte_unused,
			  struct xsk_umem *umem __rte_unused,
			  struct xsk_ring_cons *rx __rte_unused,
			  struct xsk_ring_prod *tx __rte_unused,
			  struct xsk_ring_prod *fill __rte_unused,
			  struct xsk_ring_cons *comp __rte_unused,
			  const struct xsk_socket_config *config __rte_unused)
{
	return -1;
}
#endif

#ifdef XDP_USE_NEED_WAKEUP
static int
tx_syscall_needed(struct xsk_ring_prod *q)
{
	return xsk_ring_prod__needs_wakeup(q);
}
#else
static int
tx_syscall_needed(struct xsk_ring_prod *q __rte_unused)
{
	return 1;
}
#endif
