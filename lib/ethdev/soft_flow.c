#include "soft_flow.h"
#include "rte_ethdev_core.h"
#include <rte_eal.h>
#include <rte_table_hash.h>
#include <rte_lru.h>
#include <rte_malloc.h>
#include <rte_jhash.h>

#include "simple_lru_hash_table.h"
//TODO
// int direct_port_queue[MAX_PORT_QUEUE]; 

static struct rte_flow flow_table[MAX_FLOW_RULE];

struct hash * match_table_src_ip;
struct hash * match_table_dst_ip;


void soft_flow_create_table(){
	if(!is_soft_flow_enabled())
		return;

	if(!match_table_dst_ip){
		match_table_dst_ip=hash_create();
		RTE_LOG(INFO, TABLE, "Create match table for dst ip\n");
	}
	if(!match_table_src_ip){
		match_table_src_ip=hash_create();
		RTE_LOG(INFO, TABLE, "Create match table for src ip\n");
	}
}


void soft_flow_destroy_all_table(){
	if(!is_soft_flow_enabled())
		return;
	
	if(match_table_dst_ip){
		hash_destroy(match_table_dst_ip);
		RTE_LOG(INFO, TABLE, "Destroy match table for dst ip\n");
	}
	if(match_table_src_ip){
		hash_destroy(match_table_src_ip);
		RTE_LOG(INFO, TABLE, "Destroy match table for src ip\n");
	}
}




int soft_flow_validate(uint16_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	int pattern_number=1;
	const struct rte_flow_item *item = pattern;
	for(;item->type != RTE_FLOW_ITEM_TYPE_END; item++){
		pattern_number++;
	}
	if(pattern_number > MAX_PATTERN_LENGTH){
		rte_flow_error_set(error, EINVAL, 
		RTE_FLOW_ERROR_TYPE_ITEM, 
		item,
		"pattern list too long");

		return 1;
	}

	int action_number=1;
	const struct rte_flow_action *act = actions;
	for(;act->type != RTE_FLOW_ITEM_TYPE_END; act++){
		action_number++;
	}
	if(action_number > MAX_ACTION_LENGTH){
		rte_flow_error_set(error, EINVAL, 
		RTE_FLOW_ERROR_TYPE_ITEM, 
		item,
		"action list too long");

		return 1;
	}

	return 0;
}

struct rte_flow *
soft_flow_create(uint16_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{

}

int flow_process(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts){
	if(!is_soft_flow_enabled())
		return 0;
	//test_table();
	//rte_exit(0,"exit");
	
	
	//send burst
	/*
	struct rte_eth_fp_ops *p;
	void *qd;
	p = &rte_eth_fp_ops[port_id^1];
	qd = p->rxq.data[queue_id];
	p->tx_pkt_burst(qd, rx_pkts, nb_pkts);
	*/

	// printf("flow process\n");
	return 0;
}