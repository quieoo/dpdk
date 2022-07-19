#include "dpdk_utils.h"

void dpdk_init(struct application_dpdk_config *app_dpdk_config){
    int ret = 0;

    ret = rte_eth_dev_count_avail();
	printf("``available devices: %d\n",ret);
    if (app_dpdk_config->port_config.nb_ports > 0 && ret != app_dpdk_config->port_config.nb_ports)
		APP_EXIT("Application will only function with %u ports, num_of_ports=%d",app_dpdk_config->port_config.nb_ports, ret);
    
}