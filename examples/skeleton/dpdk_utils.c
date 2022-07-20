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
setup_hairpin_queues(uint16_t port_id, uint16_t peer_port_id, uint16_t *reserved_hairpin_q_list, int hairpin_queue_len)
{
	/* Port:
	 *	0. RX queue
	 *	1. RX hairpin queue rte_eth_rx_hairpin_queue_setup
	 *	2. TX hairpin queue rte_eth_tx_hairpin_queue_setup
	 */

	int ret = 0, hairpin_q;
	uint16_t nb_tx_rx_desc = 2048;
	uint32_t manual = 1;
	uint32_t tx_exp = 1;
	struct rte_eth_hairpin_conf hairpin_conf = {
	    .peer_count = 1,
	    .manual_bind = !!manual,
	    .tx_explicit = !!tx_exp,
	    .peers[0] = {peer_port_id},
	};

	for (hairpin_q = 0; hairpin_q < hairpin_queue_len; hairpin_q++) {
		// TX
		hairpin_conf.peers[0].queue = reserved_hairpin_q_list[hairpin_q];
		ret = rte_eth_tx_hairpin_queue_setup(port_id, reserved_hairpin_q_list[hairpin_q], nb_tx_rx_desc,
						     &hairpin_conf);
		if (ret != 0)
			return ret;
		// RX
		hairpin_conf.peers[0].queue = reserved_hairpin_q_list[hairpin_q];
		ret = rte_eth_rx_hairpin_queue_setup(port_id, reserved_hairpin_q_list[hairpin_q], nb_tx_rx_desc,
						     &hairpin_conf);
		if (ret != 0)
			return ret;
	}
	return ret;
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

    /* Configure the Ethernet device */
	ret = rte_eth_dev_configure(port, rx_rings + nb_hairpin_queues, tx_rings + nb_hairpin_queues, &port_conf);
	if (ret != 0)
		return ret;
	if (port_conf_default.rx_adv_conf.rss_conf.rss_hf != port_conf.rx_adv_conf.rss_conf.rss_hf) {
        // DOCA_LOG_DBG -> printf
        printf("``DEBUG: Port %u modified RSS hash function based on hardware support, requested:%#" PRIx64
			     " configured:%#\n" PRIx64 "",
			     port, port_conf_default.rx_adv_conf.rss_conf.rss_hf,
			     port_conf.rx_adv_conf.rss_conf.rss_hf);
		
	}
    
    /* Enable RX in promiscuous mode for the Ethernet device */
	ret = rte_eth_promiscuous_enable(port);
	if (ret != 0)
		return ret;
    
    /* Allocate and set up RX queues according to number of cores per Ethernet port */
	for (q = 0; q < rx_rings; q++) {
		ret = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (ret < 0)
			return ret;
	}

    /* Allocate and set up TX queues according to number of cores per Ethernet port */
	for (q = 0; q < tx_rings; q++) {
		ret = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (ret < 0)
			return ret;
	}

    /* Enabled hairpin queue before port start */
	if (nb_hairpin_queues) {
		for (queue_index = 0; queue_index < nb_hairpin_queues; queue_index++)
			rss_queue_list[queue_index] = app_config->port_config.nb_queues + queue_index;
		ret = setup_hairpin_queues(port, port ^ 1, rss_queue_list, nb_hairpin_queues);
		if (ret != 0)
			APP_EXIT("Cannot hairpin port %" PRIu8 ", ret=%d", port, ret);
	}

	/* Start the Ethernet port */
	ret = rte_eth_dev_start(port);
	if (ret < 0)
		return ret;

	/* Display the port MAC address */
	rte_eth_macaddr_get(port, &addr);
	printf("``DEBUG: Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
		     (unsigned int)port, addr.addr_bytes[0], addr.addr_bytes[1], addr.addr_bytes[2], addr.addr_bytes[3],
		     addr.addr_bytes[4], addr.addr_bytes[5]);

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	if (rte_eth_dev_socket_id(port) > 0 && rte_eth_dev_socket_id(port) != (int)rte_socket_id()) {
		printf("``WARN: Port %u is on remote NUMA node to polling thread\n", port);
		printf("``WARN: \tPerformance will not be optimal.\n");
	}
	return ret;

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


static int
bind_hairpin_queues(uint16_t port_id)
{
	/* Configure the Rx and Tx hairpin queues for the selected port. */
	int ret = 0, peer_port, peer_ports_len;
	uint16_t peer_ports[RTE_MAX_ETHPORTS];

	/* bind current Tx to all peer Rx */
	peer_ports_len = rte_eth_hairpin_get_peer_ports(port_id, peer_ports, RTE_MAX_ETHPORTS, 1);
	if (peer_ports_len < 0)
		return peer_ports_len;
	for (peer_port = 0; peer_port < peer_ports_len; peer_port++) {
		ret = rte_eth_hairpin_bind(port_id, peer_ports[peer_port]);
		if (ret < 0)
			return ret;
	}
	/* bind all peer Tx to current Rx */
	peer_ports_len = rte_eth_hairpin_get_peer_ports(port_id, peer_ports, RTE_MAX_ETHPORTS, 0);
	if (peer_ports_len < 0)
		return peer_ports_len;
	for (peer_port = 0; peer_port < peer_ports_len; peer_port++) {
		ret = rte_eth_hairpin_bind(peer_ports[peer_port], port_id);
		if (ret < 0)
			return ret;
	}
	return ret;
}

static void
enable_hairpin_queues(uint8_t nb_ports)
{
	uint8_t port_id;

	for (port_id = 0; port_id < nb_ports; port_id++)
		if (bind_hairpin_queues(port_id) != 0)
			APP_EXIT("Hairpin bind failed on port=%u", port_id);
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

	/* Enable hairpin queues */
	if (app_dpdk_config->port_config.nb_hairpin_q > 0)
		enable_hairpin_queues(app_dpdk_config->port_config.nb_ports);

	/*	
	if (app_dpdk_config->sft_config.enable)
		dpdk_sft_init(app_dpdk_config);
	*/
}


