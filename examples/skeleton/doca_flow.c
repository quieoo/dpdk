#include "doca_flow.h"
#include <rte_flow.h>
#include <rte_ethdev.h>

void doca_flow_destroy_port(port_id){
    struct rte_flow_error error;
    int ret;

    rte_flow_flush(port_id, &error);
	ret = rte_eth_dev_stop(port_id);
	if (ret < 0)
		printf("Failed to stop port %u: %s",
		       port_id, rte_strerror(-ret));
	rte_eth_dev_close(port_id);
}