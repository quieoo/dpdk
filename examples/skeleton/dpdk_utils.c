#include "dpdk_utils.h"



static struct rte_mempool *
allocate_mempool(const uint32_t total_nb_mbufs)
{
	struct rte_mempool *mbuf_pool;
	/* Creates a new mempool in memory to hold the mbufs */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", total_nb_mbufs, MBUF_CACHE_SIZE, 0,
					    RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		APP_EXIT("Cannot allocate mbuf pool");
	return mbuf_pool;
}

static int
dpdk_ports_init(struct application_dpdk_config *app_config)
{
    int ret;
	uint8_t port_id;
	const uint8_t nb_ports = app_config->port_config.nb_ports;
	const uint32_t total_nb_mbufs = app_config->port_config.nb_queues * nb_ports * NUM_MBUFS;
	struct rte_mempool *mbuf_pool;    

    /* Initialize mbufs mempool */
    mbuf_pool = allocate_mempool(total_nb_mbufs);

    printf("allocate memory size: %d\n",mbuf_pool->size);
}




void dpdk_init(struct application_dpdk_config *app_dpdk_config){
    
    int ret = 0;

    /* Check that DPDK enabled the required ports to send/receive on */

    ret = rte_eth_dev_count_avail();
	if (app_dpdk_config->port_config.nb_ports > 0 && ret != app_dpdk_config->port_config.nb_ports)
		APP_EXIT("Application will only function with %u ports, num_of_ports=%d",app_dpdk_config->port_config.nb_ports, ret);
    
    /* Check for available logical cores */
    ret = rte_lcore_count();
	if (app_dpdk_config->port_config.nb_queues > 0 && ret < app_dpdk_config->port_config.nb_queues)
		APP_EXIT("At least %d cores are needed for the application to run, available_cores=%d", app_dpdk_config->port_config.nb_queues, ret);
	else
		app_dpdk_config->port_config.nb_queues = ret;
    
    if (app_dpdk_config->reserve_main_thread)
		app_dpdk_config->port_config.nb_queues -= 1;

    if (app_dpdk_config->port_config.nb_ports > 0 && dpdk_ports_init(app_dpdk_config) != 0)
		APP_EXIT("Ports allocation failed");

}


