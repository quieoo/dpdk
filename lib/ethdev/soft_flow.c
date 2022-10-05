#include "soft_flow.h"
#include "rte_ethdev_core.h"
#include <rte_eal.h>

int direct_port_queue[MAX_PORT_QUEUE];
bool IS_SOFT_FLOW_ON;
struct rte_flow *flow_table;

int soft_flow_init(int* pq_map, int table_size){

}
int soft_flow_start(){}
int soft_flow_stop(){}



int soft_flow_validate(uint16_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error){}

struct rte_flow *
soft_flow_create(uint16_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error){}

int flow_process(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts){
	if(!is_soft_flow_enabled())
		return 0;
	
	//send burst
	/*
	struct rte_eth_fp_ops *p;
	void *qd;
	p = &rte_eth_fp_ops[port_id^1];
	qd = p->rxq.data[queue_id];
	p->tx_pkt_burst(qd, rx_pkts, nb_pkts);
	*/

	printf("flow process\n");
	return 0;
}