#include "soft_flow.h"
#include "rte_ethdev_core.h"
#include <rte_eal.h>
#include <rte_malloc.h>
#include <rte_jhash.h>

#include "simple_lru_hash_table.h"
// TODO
//  int direct_port_queue[MAX_PORT_QUEUE];

static struct rte_flow *flow_table[MAX_FLOW_RULE];
static int num_flows = 0;

typedef struct match_entry
{
	uint8_t out_src_mac[6];
	uint8_t out_dst_mac[6];
	uint32_t out_src_ip;
	uint32_t out_dst_ip;
	uint8_t l4_type;
	uint16_t out_src_port;
	uint16_t out_dst_port;
};
#define MATCH_ENTRY_LENGTH 25
struct hash *match_table;

static void print_eth(struct rte_ether_hdr *eth_hdr)
{
	printf("	dst %x-%x-%x-%x-%x-%x, src %x-%x-%x-%x-%x-%x\n",
		   eth_hdr->dst_addr.addr_bytes[0],
		   eth_hdr->dst_addr.addr_bytes[1],
		   eth_hdr->dst_addr.addr_bytes[2],
		   eth_hdr->dst_addr.addr_bytes[3],
		   eth_hdr->dst_addr.addr_bytes[4],
		   eth_hdr->dst_addr.addr_bytes[5],
		   eth_hdr->src_addr.addr_bytes[0],
		   eth_hdr->src_addr.addr_bytes[1],
		   eth_hdr->src_addr.addr_bytes[2],
		   eth_hdr->src_addr.addr_bytes[3],
		   eth_hdr->src_addr.addr_bytes[4],
		   eth_hdr->src_addr.addr_bytes[5]);
}

void soft_flow_create_table()
{
	if (!is_soft_flow_enabled())
		return;

	if (!match_table)
	{
		match_table = hash_create(MATCH_ENTRY_LENGTH);
		RTE_LOG(INFO, TABLE, "Create match entry table\n");
	}
}

static void soft_flow_destroy_flow_table()
{
	for (int i = 0; i < num_flows; i++)
	{
		if (flow_table[i])
			free(flow_table[i]);
	}
	num_flows = 0;
}

void soft_flow_destroy_all_table()
{
	if (!is_soft_flow_enabled())
		return;
	if (match_table)
	{
		hash_destroy(match_table);
		RTE_LOG(INFO, TABLE, "Destroy match entry table\n");
	}
	if (num_flows)
		soft_flow_destroy_flow_table();
}

int soft_flow_validate_flow(uint16_t port_id,
							const struct rte_flow_attr *attr,
							const struct rte_flow_item pattern[],
							const struct rte_flow_action actions[],
							struct rte_flow_error *error)
{
	if (!is_soft_flow_enabled())
		return;
	// RTE_LOG(INFO, TABLE, "validate flow\n");

	int pattern_number = 1;
	const struct rte_flow_item *item = pattern;
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++)
	{
		pattern_number++;
	}
	if (pattern_number > MAX_PATTERN_LENGTH)
	{
		rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "pattern list too long");

		return 1;
	}

	int action_number = 1;
	const struct rte_flow_action *act = actions;
	for (; act->type != RTE_FLOW_ITEM_TYPE_END; act++)
	{
		action_number++;
	}
	if (action_number > MAX_ACTION_LENGTH)
	{
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
	if (!is_soft_flow_enabled())
		return;

	// RTE_LOG(INFO, TABLE, "create flow\n");

	// keep track of all flow
	struct rte_flow *new_flow;
	new_flow=rte_zmalloc(NULL, sizeof(struct rte_flow), RTE_CACHE_LINE_SIZE);
	if (!new_flow)
	{
		RTE_LOG(ERR, TABLE, "failed to allocate memory for new flow rule\n");
		return NULL;
	}
	memcpy(&(new_flow->attr), attr, sizeof(struct rte_flow_attr));
	memcpy(new_flow->actions, actions, sizeof(actions));
	int act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, NULL, 0, actions, error);
	act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, new_flow->actions, act_len, actions, error);
	if (act_len <= 0)
	{
		RTE_LOG(ERR, TABLE, "rte_flow_conv error\n");
		return NULL;
	}

	printf("build flow %d(%x-%x)\n", new_flow->actions[0].type, new_flow, &(new_flow->actions[0].type));

	// build match entry and link the entry to its flow
	const struct rte_flow_item *item = pattern;
	//printf("add flow rule:\n");

	struct match_entry e;
	memset(&e, 0x0, sizeof(struct match_entry));

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++)
	{
		if (!(item->spec))
			continue;
		switch (item->type){
		case RTE_FLOW_ITEM_TYPE_ETH:{
			const struct rte_flow_item_eth *spec = item->spec;
			//print_eth(&(spec->hdr));
			memcpy(e.out_dst_mac, spec->hdr.dst_addr.addr_bytes, 6);
			memcpy(e.out_src_mac, spec->hdr.src_addr.addr_bytes, 6);
			break;
		}
		case RTE_FLOW_ITEM_TYPE_IPV4:{
			const struct rte_flow_item_ipv4 *spec = item->spec;
			//printf("	dst-%x src-%x\n", spec->hdr.dst_addr, spec->hdr.src_addr);
			e.out_dst_ip = spec->hdr.dst_addr;
			e.out_src_ip = spec->hdr.src_addr;
			e.l4_type = spec->hdr.next_proto_id;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_TCP:{
			const struct rte_flow_item_tcp *spec = item->spec;
			e.out_dst_port = spec->hdr.dst_port;
			e.out_src_port = spec->hdr.src_port;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_UDP:{
			const struct rte_flow_item_udp *spec = item->spec;
			e.out_dst_port = spec->hdr.dst_port;
			e.out_src_port = spec->hdr.src_port;
			break;
		}
		default:
			printf("Unclassified flow item type: %d\n", item->type);
			break;
		}
	}
	hash_add(match_table, &e, &new_flow);
	flow_table[num_flows++] = new_flow;

	return new_flow;
}

int flow_process(uint16_t port_id, uint16_t queue_id, struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	if (!is_soft_flow_enabled())
		return nb_pkts;
	if (nb_pkts == 0)
		return nb_pkts;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ether_hdr *eth_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	struct rte_udp_hdr *udp_hdr;

	bool hit[32] = {0};
	int last_tx_send_position = 0;

	struct rte_mbuf *tx_send[32];
	printf("flow prcess incoming packets: \n");
	for (int i = 0; i < nb_pkts; i++)
	{
		eth_hdr = rte_pktmbuf_mtod(rx_pkts[i], struct rte_ether_hdr *);
		ipv4_hdr = rte_pktmbuf_mtod_offset(rx_pkts[i], struct rte_ipv4_hdr *,
										   sizeof(struct rte_ether_hdr));
		
		struct rte_flow *flow;
		struct match_entry e;
		memset(&e, 0x0, sizeof(struct match_entry));
		memcpy(e.out_dst_mac, eth_hdr->dst_addr.addr_bytes, 6);
		memcpy(e.out_src_mac, eth_hdr->src_addr.addr_bytes, 6);
		e.out_dst_ip=ipv4_hdr->dst_addr;
		e.out_src_ip=ipv4_hdr->src_addr;
		e.l4_type=ipv4_hdr->next_proto_id;
		switch (e.l4_type)
		{
		case IPPROTO_TCP:
			tcp_hdr=rte_pktmbuf_mtod_offset(rx_pkts[i], struct rte_tcp_hdr*, 
				sizeof(struct rte_ether_hdr)+
				sizeof(struct rte_ipv4_hdr));
			e.out_dst_port=tcp_hdr->dst_port;
			e.out_src_port=tcp_hdr->src_port;
			break;
		case IPPROTO_UDP:
			udp_hdr=rte_pktmbuf_mtod_offset(rx_pkts[i], struct rte_udp_hdr*, 
				sizeof(struct rte_ether_hdr)+
				sizeof(struct rte_ipv4_hdr));
			e.out_dst_port=udp_hdr->dst_port;
			e.out_src_port=udp_hdr->src_port;
			break;
		default:
			// printf("Unclassified IP Proto: %d\n", e.l4_type);
			break;
		}
		if(hash_lookup(match_table, &e, &flow))
			continue;
		hit[i] = 1;
		tx_send[last_tx_send_position++] = rx_pkts[i];
		print_eth(eth_hdr);
		printf("	dst-%x:%d src-%x:%d\n", ipv4_hdr->dst_addr,e.out_dst_port,ipv4_hdr->src_addr, e.out_src_port);
		for(int i=0;i<num_flows;i++){
			printf("%d ", i);
			printf("%d\n", flow_table[i]->actions[0].type);
		}
		
		printf("flow: %x-%x\n", flow, &(flow->actions[0].type));
		printf("%d\n", flow[0].actions[0].type);
		
	}
	/*
		process hit flow
	*/
		

	// printf("flow hit: \n");
	for (int i = 0; i < last_tx_send_position; i++)
	{
		ipv4_hdr = rte_pktmbuf_mtod_offset(tx_send[i], struct rte_ipv4_hdr *,
										   sizeof(struct rte_ether_hdr));
		// printf("	dst-%x src-%x\n", ipv4_hdr->dst_addr, ipv4_hdr->src_addr);
	}

	// send out hit flow
	void *qd;
	struct rte_eth_fp_ops *p = &rte_eth_fp_ops[port_id ^ 1];
	qd = p->txq.data[queue_id];
	int send_pkts = p->tx_pkt_burst(qd, tx_send, last_tx_send_position);
	// rte_ethdev_trace_tx_burst(port_id^1, queue_id, (void **)tx_send, send_pkts);
	printf("fast path packets: %d : %d\n", send_pkts, nb_pkts);

	// left un-hit pkts
	int last_not_hit_position = 0;
	// printf("flow un-hit:\n");
	for (int i = 0; i < nb_pkts; i++)
	{
		if (!hit[i])
		{
			ipv4_hdr = rte_pktmbuf_mtod_offset(rx_pkts[i], struct rte_ipv4_hdr *,
											   sizeof(struct rte_ether_hdr));
			// printf("	dst-%x src-%x\n", ipv4_hdr->dst_addr, ipv4_hdr->src_addr);

			rx_pkts[last_not_hit_position++] = rx_pkts[i];
		}
	}

	return last_not_hit_position;
}