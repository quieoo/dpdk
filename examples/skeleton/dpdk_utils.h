#ifndef COMMON_DPDK_UTILS_H_
#define COMMON_DPDK_UTILS_H_


#include "offload_rules.h"
#include "utils.h"
#include <rte_ethdev.h>


void dpdk_init(struct application_dpdk_config *app_dpdk_config);


#endif /* COMMON_DPDK_UTILS_H_ */