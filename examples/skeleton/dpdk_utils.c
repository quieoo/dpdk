#include "dpdk_utils.h"
#include <rte_ethdev.h>

#define RSS_KEY_LEN 40


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
port_init(struct rte_mempool *mbuf_pool, uint8_t port, struct application_dpdk_config *app_config)
{
    int ret;
	int symmetric_hash_key_length = RSS_KEY_LEN;
	const uint16_t nb_hairpin_queues = app_config->port_config.nb_hairpin_q;
	const uint16_t rx_rings = app_config->port_config.nb_queues;
	const uint16_t tx_rings = app_config->port_config.nb_queues;
	uint16_t q, queue_index;
	uint16_t rss_queue_list[nb_hairpin_queues];
	struct rte_ether_addr addr;
	struct rte_eth_dev_info dev_info;
	uint8_t symmetric_hash_key[RSS_KEY_LEN] = {
	    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
	};
	const struct rte_eth_conf port_conf_default = {
	    .rxmode = {
		    .mtu = RTE_ETHER_MAX_LEN,
		},
	    .rx_adv_conf = {
		    .rss_conf = {
			    .rss_key_len = symmetric_hash_key_length,
			    .rss_key = symmetric_hash_key,
			    .rss_hf = ETH_RSS_PROTO_MASK,
			},
		},
	};
	struct rte_eth_conf port_conf = port_conf_default;


    if (!rte_eth_dev_is_valid_port(port))
		APP_EXIT("Invalid port");
	ret = rte_eth_dev_info_get(port, &dev_info);
	if (ret != 0)
		APP_EXIT("Failed getting device (port %u) info, error=%s", port, strerror(-ret));
	port_conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;

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

    ret = rte_flow_dynf_metadata_register();
	if (ret < 0)
		APP_EXIT("Metadata register failed");
    for (port_id = 0; port_id < nb_ports; port_id++)
		if (port_init(mbuf_pool, port_id, app_config) != 0)
			APP_EXIT("Cannot init port %" PRIu8, port_id);
	return ret;
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


