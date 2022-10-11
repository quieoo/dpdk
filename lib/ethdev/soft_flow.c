#include "soft_flow.h"
#include "rte_ethdev_core.h"
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_jhash.h>

#include "simple_lru_hash_table.h"
//TODO
// int direct_port_queue[MAX_PORT_QUEUE]; 

static struct rte_flow *flow_table[MAX_FLOW_RULE];
static int num_flows=0;

struct hash *match_table_out_src_mac;
struct hash *match_table_out_dst_mac;
struct hash * match_table_out_src_ip;
struct hash * match_table_out_dst_ip;


void soft_flow_create_table(){
	if(!is_soft_flow_enabled())
		return;

	if(!match_table_out_dst_ip){
		match_table_out_dst_ip=hash_create(sizeof(rte_be32_t));
		RTE_LOG(INFO, TABLE, "Create match table for out dst ip\n");
	}
	if(!match_table_out_src_ip){
		match_table_out_src_ip=hash_create(sizeof(rte_be32_t));
		RTE_LOG(INFO, TABLE, "Create match table for out src ip\n");
	}
	if(!match_table_out_dst_mac){
		match_table_out_dst_mac=hash_create(6);
		RTE_LOG(INFO, TABLE, "Create match table for out dst mac\n");
	}
	if(!match_table_out_src_mac){
		match_table_out_src_mac=hash_create(6);
		RTE_LOG(INFO, TABLE, "Create match table for out src mac\n");
	}

}

static void soft_flow_destroy_flow_table(){
	for(int i=0;i<num_flows;i++){
		if(flow_table[i])
			free(flow_table[i]);
	}
	num_flows=0;
}

void soft_flow_destroy_all_table(){
	if(!is_soft_flow_enabled())
		return;
	
	if(match_table_out_dst_ip){
		hash_destroy(match_table_out_dst_ip);
		RTE_LOG(INFO, TABLE, "Destroy match table for out dst ip\n");
	}
	if(match_table_out_src_ip){
		hash_destroy(match_table_out_src_ip);
		RTE_LOG(INFO, TABLE, "Destroy match table for out src ip\n");
	}

	if(match_table_out_dst_mac){
		hash_destroy(match_table_out_dst_mac);
		RTE_LOG(INFO, TABLE, "Destroy match table for out dst mac\n");
	}
	if(match_table_out_src_mac){
		hash_destroy(match_table_out_src_mac);
		RTE_LOG(INFO, TABLE, "Destroy match table for out src mac\n");
	}

	if(num_flows)
		soft_flow_destroy_flow_table();

}




int soft_flow_validate_flow(uint16_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	if(!is_soft_flow_enabled())
		return;
	// RTE_LOG(INFO, TABLE, "validate flow\n");

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
soft_flow_create_flow(uint16_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	if(!is_soft_flow_enabled())
		return;
	
	// RTE_LOG(INFO, TABLE, "create flow\n");

	// keep track of all flow
	struct rte_flow *new_flow=rte_zmalloc(NULL, sizeof(struct rte_flow), 0);
	if(!new_flow){
		RTE_LOG(ERR, TABLE, "failed to allocate memory for new flow rule\n");
		return NULL;
	}
	memcpy(&(new_flow->attr), attr, sizeof(struct rte_flow_attr));
	memcpy(new_flow->actions, actions, sizeof(actions));
	int act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS,NULL, 0, actions, error);
	act_len=rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, new_flow->actions, act_len, actions, error);
	if(act_len <= 0 ){
		RTE_LOG(ERR, TABLE, "rte_flow_conv error\n");
		return NULL;
	}



	//build match entry and link the entry to its flow
	const struct rte_flow_item *item = pattern;
	//printf("add flow rule:\n");
	for(;item->type != RTE_FLOW_ITEM_TYPE_END; item++){
		if(item->type == RTE_FLOW_ITEM_TYPE_ETH){
			if(!(item->spec)){
				continue;
			}
			const struct rte_flow_item_eth *spec=item->spec;
			if(!hash_add(match_table_out_dst_mac, spec->hdr.dst_addr.addr_bytes, &new_flow)){
				RTE_LOG(ERR, TABLE, "Error add entry to hash table");
				return NULL;
			}
			if(!hash_add(match_table_out_src_mac, spec->hdr.src_addr.addr_bytes, &new_flow)){
				RTE_LOG(ERR, TABLE, "Error add entry to hash table");
				return NULL;
			}
		}
		if(item->type==RTE_FLOW_ITEM_TYPE_IPV4){
			if(!(item->spec)){
				continue;
			}
			const struct rte_flow_item_ipv4 *spec = item->spec;
			if(spec->hdr.dst_addr==0 && spec->hdr.src_addr==0)
				continue;
			//printf("	dst-%x src-%x\n", spec->hdr.dst_addr, spec->hdr.src_addr);
			if(!hash_add(match_table_out_dst_ip, &(spec->hdr.dst_addr), &new_flow)){
				RTE_LOG(ERR, TABLE, "Error add entry to hash table");
				return NULL;
			}
			if(!hash_add(match_table_out_src_ip, &(spec->hdr.src_addr), &new_flow)){
				RTE_LOG(ERR, TABLE, "Error add entry to hash table");
				return NULL;
			}
		}
	}

	flow_table[num_flows++]=new_flow;

	return new_flow;
}
void print_eth(uint8_t* e){
	printf("%x-%x-%x-%x-%x-%x", e[0], e[1], e[2], e[3], e[4], e[5]);
}
int flow_process(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts){
	if(!is_soft_flow_enabled())
		return nb_pkts;
	if(nb_pkts==0)
		return nb_pkts;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ether_hdr *eth_hdr;
	bool hit[32]={0};
	int last_tx_send_position=0;

	struct rte_mbuf *tx_send[32];
	printf("flow prcess incoming packets: \n");
	for(int i=0; i<nb_pkts; i++){
		eth_hdr=rte_pktmbuf_mtod(rx_pkts[i], struct rte_ether_hdr *);
		ipv4_hdr = rte_pktmbuf_mtod_offset(rx_pkts[i], struct rte_ipv4_hdr *, 
			sizeof(struct rte_ether_hdr));
		printf("	dst-%x src-%x\n", ipv4_hdr->dst_addr, ipv4_hdr->src_addr);
		printf("	dst ");
		print_eth(eth_hdr->dst_addr.addr_bytes);
		printf(" src ");
		print_eth(eth_hdr->src_addr.addr_bytes);
		printf("\n");
		
		struct rte_flow *flow;
		if(hash_lookup(match_table_out_dst_mac, eth_hdr->dst_addr.addr_bytes, &flow))
			continue;
		if(hash_lookup(match_table_out_src_mac, eth_hdr->src_addr.addr_bytes, &flow))
			continue;
		if(hash_lookup(match_table_out_dst_ip, &(ipv4_hdr->dst_addr), &flow))
			continue;
		if(hash_lookup(match_table_out_src_ip, &(ipv4_hdr->src_addr), &flow))
			continue;
		
		hit[i]=1;
		tx_send[last_tx_send_position++]=rx_pkts[i];
	}
	/*
		process hit flow
	*/
	//printf("flow hit: \n");
	for(int i=0; i<last_tx_send_position; i++){
		ipv4_hdr = rte_pktmbuf_mtod_offset(tx_send[i], struct rte_ipv4_hdr *, 
		sizeof(struct rte_ether_hdr));
		// printf("	dst-%x src-%x\n", ipv4_hdr->dst_addr, ipv4_hdr->src_addr);
	}

	// send out hit flow
	void *qd;
	struct rte_eth_fp_ops *p = &rte_eth_fp_ops[port_id^1];
	qd = p->txq.data[queue_id];
	int send_pkts=p->tx_pkt_burst(qd, tx_send, last_tx_send_position);
	// rte_ethdev_trace_tx_burst(port_id^1, queue_id, (void **)tx_send, send_pkts);
	printf("fast path packets: %d : %d\n", send_pkts, nb_pkts);

	// left un-hit pkts	
	int last_not_hit_position=0;
	// printf("flow un-hit:\n");
	for(int i=0; i< nb_pkts; i++){
		if(!hit[i]){
			ipv4_hdr = rte_pktmbuf_mtod_offset(rx_pkts[i], struct rte_ipv4_hdr *, 
			sizeof(struct rte_ether_hdr));
			// printf("	dst-%x src-%x\n", ipv4_hdr->dst_addr, ipv4_hdr->src_addr);

			rx_pkts[last_not_hit_position++]=rx_pkts[i];
		}
	}
	
	return last_not_hit_position;
}