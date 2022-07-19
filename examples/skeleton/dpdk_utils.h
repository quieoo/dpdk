#ifndef COMMON_DPDK_UTILS_H_
#define COMMON_DPDK_UTILS_H_


#include "offload_rules.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_mbuf.h>
#include <rte_flow.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

void dpdk_init(struct application_dpdk_config *app_dpdk_config);


#endif /* COMMON_DPDK_UTILS_H_ */