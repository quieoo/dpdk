#include "dpdk_utils.h"

void dpdk_init(struct application_dpdk_config *app_dpdk_config){
    
    int ret = 0;

    /* Check that DPDK enabled the required ports to send/receive on */

    ret = rte_eth_dev_count_avail();
	if (app_dpdk_config->port_config.nb_ports > 0 && ret != app_dpdk_config->port_config.nb_ports)
		APP_EXIT("Application will only function with %u ports, num_of_ports=%d",app_dpdk_config->port_config.nb_ports, ret);
    
    /* Check for available logical cores */
    ret = rte_lcore_count();
    printf("ava cores %d\n",ret);
	if (app_dpdk_config->port_config.nb_queues > 0 && ret < app_dpdk_config->port_config.nb_queues)
		APP_EXIT("At least %d cores are needed for the application to run, available_cores=%d", app_dpdk_config->port_config.nb_queues, ret);
	else
		app_dpdk_config->port_config.nb_queues = ret;
    
    if (app_dpdk_config->reserve_main_thread)
		app_dpdk_config->port_config.nb_queues -= 1;


    


}

