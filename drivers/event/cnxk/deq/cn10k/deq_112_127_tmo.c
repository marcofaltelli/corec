/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_worker.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

#define R(name, flags) SSO_DEQ_TMO(cn10k_sso_hws_deq_tmo_##name, flags)

NIX_RX_FASTPATH_MODES_112_127
#undef R
