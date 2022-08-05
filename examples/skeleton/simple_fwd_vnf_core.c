#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_flow.h>


#include "simple_fwd_port.h"
#include "simple_fwd_vnf_core.h"
#include "simple_fwd_pkt.h"

#define VNF_RX_BURST_SIZE (32)
#define MAX_PATTERN_NUM		10
#define MAX_ACTION_NUM		10
#define SRC_IP ((0<<24) + (0<<16) + (0<<8) + 0) /* src ip = 0.0.0.0 */
#define DEST_IP ((192<<24) + (168<<16) + (1<<8) + 1) /* dest ip = 192.168.1.1 */
#define FULL_MASK 0xffffffff /* full mask */
#define EMPTY_MASK 0x0 /* empty mask */
static uint16_t port_id = 0;
static uint8_t selected_queue = 1;




static volatile bool force_quit;
struct vnf_per_core_params {
	int ports[NUM_OF_PORTS];
	int queues[NUM_OF_PORTS];
	bool used;
};
struct vnf_per_core_params core_params_arr[RTE_MAX_LCORE];


void register_simple_fwd_params(void){
    printf("``register_simple_fwd_params called \n");
}

void simple_fwd_map_queue(uint16_t nb_queues){
    int i, queue_idx = 0;

	memset(core_params_arr, 0, sizeof(core_params_arr));
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (!rte_lcore_is_enabled(i))
			continue;
		core_params_arr[i].ports[0] = 0;
		core_params_arr[i].ports[1] = 1;
		core_params_arr[i].queues[0] = queue_idx;
		core_params_arr[i].queues[1] = queue_idx;
		core_params_arr[i].used = true;
		queue_idx++;
		if (queue_idx >= nb_queues)
			break;
	}
}


void output_flow(uint16_t port_id, const struct rte_flow_attr *attr, const struct rte_flow_item *pattern, const struct rte_flow_action *actions, struct rte_flow_error *error){
	printf("{\n");
	
	printf("	port_id: %d\n",port_id);
	
	printf("	attr: \n");
	printf("		egress: %d\n",attr->egress);
	printf("		group: %d\n",attr->group);
	printf("		ingress: %d\n",attr->ingress);
	printf("		priority: %d\n",attr->priority);
	printf("		transfer: %d\n",attr->transfer);
	int i=0;
	
	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++){
		printf("	pattern-%d:\n",i++);
		printf("		type: ");
		switch (pattern->type)
		{
			case RTE_FLOW_ITEM_TYPE_VOID:
				printf("RTE_FLOW_ITEM_TYPE_VOID\n");
				break;
			case RTE_FLOW_ITEM_TYPE_ETH:
				printf("RTE_FLOW_ITEM_TYPE_ETH\n");
				break;
			case RTE_FLOW_ITEM_TYPE_IPV4:
				printf("RTE_FLOW_ITEM_TYPE_IPV4\n");
				const struct rte_flow_item_ipv4 *mask = pattern->mask;
				struct in_addr mask_dst,mask_src;
				mask_dst.s_addr=mask->hdr.dst_addr;
				mask_src.s_addr=mask->hdr.src_addr;
				printf("		mask.hdr:\n");
				printf("			src_addr:%s\n",inet_ntoa(mask_src));
				printf("			dst_addr: %s\n",inet_ntoa(mask_dst));
				
				const struct rte_flow_item_ipv4 *spec = pattern->spec;
				struct in_addr dst,src;
				dst.s_addr=spec->hdr.dst_addr;
				src.s_addr=spec->hdr.src_addr;

				printf("		spec.hdr:\n");
				printf("			src_addr:%s\n",inet_ntoa(src));
				printf("			dst_addr:%s\n",inet_ntoa(dst));
				
				break;
			default:
				printf("other type: %d\n",pattern->type);
		}

	}
	i=0;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		printf("	action-%d:\n",i++);
		printf("		type: ");

		switch (actions->type)
		{
			case RTE_FLOW_ACTION_TYPE_VOID:
				printf("RTE_FLOW_ACTION_TYPE_VOID\n");
				break;
			case RTE_FLOW_ACTION_TYPE_DROP:
				printf("RTE_FLOW_ACTION_TYPE_DROP\n");
				break;
			case RTE_FLOW_ACTION_TYPE_QUEUE:
				printf("RTE_FLOW_ACTION_TYPE_QUEUE\n");
				const struct rte_flow_action_queue *queue = actions->conf;
				printf("		index:%d\n",queue->index);
				break;
			default:
				printf("other type: %d\n",actions->type);
		}

	}
	
	printf("}\n");
	
}

/* Function responsible for creating the flow rule. 8< */
struct rte_flow *
generate_ipv4_flow(uint16_t port_id, uint16_t rx_q,
		uint32_t src_ip, uint32_t src_mask,
		uint32_t dest_ip, uint32_t dest_mask,
		struct rte_flow_error *error)
{

	/* Declaring structs being used. 8< */
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_ACTION_NUM];
	struct rte_flow *flow = NULL;
	struct rte_flow_action_queue queue = { .index = rx_q };
	struct rte_flow_item_ipv4 ip_spec;
	struct rte_flow_item_ipv4 ip_mask;
	/* >8 End of declaring structs being used. */
	int res;

	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));

	/* Set the rule attribute, only ingress packets will be checked. 8< */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;
	/* >8 End of setting the rule attribute. */

	/*
	 * create the action sequence.
	 * one action only,  move packet to queue
	 */
	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
	action[0].conf = &queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;

	/*
	 * set the first level of the pattern (ETH).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */

	/* Set this level to allow all. 8< */
	pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	/* >8 End of setting the first level of the pattern. */

	/*
	 * setting the second level of the pattern (IP).
	 * in this example this is the level we care about
	 * so we set it according to the parameters.
	 */

	/* Setting the second level of the pattern. 8< */
	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
	memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
	ip_spec.hdr.dst_addr = htonl(dest_ip);
	ip_mask.hdr.dst_addr = dest_mask;
	ip_spec.hdr.src_addr = htonl(src_ip);
	ip_mask.hdr.src_addr = src_mask;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[1].spec = &ip_spec;
	pattern[1].mask = &ip_mask;
	/* >8 End of setting the second level of the pattern. */

	/* The final level must be always type end. 8< */
	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
	/* >8 End of final level must be always type end. */

	/* Validate the rule and create it. 8< */
	res = rte_flow_validate(port_id, &attr, pattern, action, error);
	if (!res)
		flow = rte_flow_create(port_id, &attr, pattern, action, error);
	/* >8 End of validation the rule and create it. */

	//output_flow(port_id, &attr, pattern, action, error);
	return flow;
}

static inline void
print_ether_addr(const char *what, struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", what, buf);
}

static void generate_new_flow(struct rte_mbuf *mbuf){
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_ACTION_NUM];
	struct rte_flow *flow = NULL;
	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));
	memset(&attr, 0, sizeof(struct rte_flow_attr));

	/*
		attr
	*/
	//attr.egress=1;
	attr.ingress=1;

	/*
		pattern: match all new packet
	*/
	//mac
	pattern[0].type=RTE_FLOW_ITEM_TYPE_ETH;
	struct rte_flow_item_eth mac_spec;
	memset(&mac_spec,0,sizeof(struct rte_flow_item_eth));

	struct rte_ether_hdr *eth_hdr=rte_pktmbuf_mtod(mbuf, struct rte_ether_dhr*);
	print_ether_addr("MAC: src=",&eth_hdr->src_addr);
	print_ether_addr(" -dst=",&eth_hdr->dst_addr);
	printf("\n");

	mac_spec.hdr.src_addr=eth_hdr->src_addr;
	mac_spec.hdr.dst_addr=eth_hdr->dst_addr;
	pattern[0].spec=&mac_spec;

	//ip
	pattern[1].type=RTE_FLOW_ITEM_TYPE_IPV4;
	struct rte_flow_item_ipv4 ip_spec;
	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));

	struct rte_ipv4_hdr *ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
	struct in_addr addr;
	addr.s_addr=ipv4_hdr->src_addr;
	printf("IP: src=%s ",inet_ntoa(addr));
	addr.s_addr=ipv4_hdr->dst_addr;
	printf(" -dst=%s\n",inet_ntoa(addr));

	ip_spec.hdr.src_addr=ipv4_hdr->src_addr;
	ip_spec.hdr.dst_addr=ipv4_hdr->dst_addr;
	pattern[1].spec=&ip_spec;

	//TCP-UDP
	switch (ipv4_hdr->next_proto_id)
	{
		case IPPROTO_TCP:
		{
			pattern[2].type=RTE_FLOW_ITEM_TYPE_TCP;
			struct rte_flow_item_tcp tcp_spec;
			memset(&tcp_spec,0,sizeof(struct rte_flow_item_tcp));

			struct rte_tcp_hdr *tcp;
			tcp=(struct rte_tcp_hdr *)((unsigned char *)ipv4_hdr +sizeof(struct rte_ipv4_hdr));
			printf("TCP: src=%d -dst=%d\n",tcp->src_port,tcp->dst_port);

			tcp_spec.hdr.src_port=tcp->src_port;
			tcp_spec.hdr.dst_port=tcp->dst_port;
			pattern[2].spec=&tcp_spec;

			break;
		}
		case IPPROTO_UDP:
		{
			pattern[2].type=RTE_FLOW_ITEM_TYPE_UDP;
			struct rte_flow_item_udp udp_spec;
			memset(&udp_spec,0,sizeof(struct rte_flow_item_udp));

			struct rte_udp_hdr *udp;
			udp=(struct rte_udp_hdr *)((unsigned char *)ipv4_hdr+sizeof(struct rte_ipv4_hdr));
			printf("UDP: src=%d -dst=%d\n",udp->src_port,udp->dst_port);

			udp_spec.hdr.src_port=udp->src_port;
			udp_spec.hdr.dst_port=udp->dst_port;
			pattern[2].spec=&udp_spec;

			break;
		}
		default:
			printf("OTHER: typeid- %d\n",ipv4_hdr->next_proto_id);
	}

	printf("----------------------------------------\n");
	int j=0;
	pattern[2].type=RTE_FLOW_ITEM_TYPE_END;
	
	/*
		action: redirect all packet to 
			dst_mac: 0x0c, 0x42, 0xa1, 0x4b, 0xc5, 0x8c
			dst_ip: 18.18.18.18
			dst_port: 55555
	*/
	
	action[0].type=RTE_FLOW_ACTION_TYPE_SET_MAC_DST;
	struct rte_flow_action_set_mac dst_mac;
	// memset(&dst_mac,0,sizeof(struct rte_flow_action_set_mac));
	dst_mac.mac_addr[0]=0x0c;
	dst_mac.mac_addr[1]=0x42;
	dst_mac.mac_addr[2]=0xa1;
	dst_mac.mac_addr[3]=0x4b;
	dst_mac.mac_addr[4]=0xc5;
	dst_mac.mac_addr[5]=0x8c;
	action[0].conf=&dst_mac;
	
	action[1].type=RTE_FLOW_ACTION_TYPE_SET_IPV4_DST;
	struct rte_flow_action_set_ipv4 set_ipv4;
	set_ipv4.ipv4_addr=RTE_BE32( 18<<24 + 18<<16 + 18<<8 + 18);
	action[1].conf=&set_ipv4;

	action[2].type=RTE_FLOW_ACTION_TYPE_SET_TP_DST;
	struct rte_flow_action_set_tp set_tp;
	set_tp.port=RTE_BE16(55555);
	action[2].conf=&set_tp;

	action[3].type=RTE_FLOW_ACTION_TYPE_END;

	struct rte_flow_error error;
	int res=rte_flow_validate(port_id, &attr,pattern,action,&error);
	if(!res){
		flow = rte_flow_create(port_id, &attr, pattern, action, &error);
		if (!flow) {
			printf("Flow can't be created %d message: %s\n",
				error.type,
				error.message ? error.message : "(no stated reason)");
			rte_exit(EXIT_FAILURE, "error in creating flow");
		}
		printf("create flow\n");
		output_flow(port_id, &attr, pattern, action, &error);
	}else{
		printf("ERROR while validate flow: %d\n",res);
		printf("%s\n",error.message);
	}


}

static void
simple_fwd_process_offload(struct rte_mbuf *mbuf, uint16_t queue_id, struct app_vnf *vnf)
{
	
	generate_new_flow(mbuf);

    /*
        simple_fwd_process_offload -> generate_ipv4_flow
    */
   struct rte_flow_error error;
   struct rte_flow *flow;
   flow = generate_ipv4_flow(port_id, selected_queue,
				SRC_IP, EMPTY_MASK,
				DEST_IP, FULL_MASK, &error);
    
    if (!flow) {
		printf("Flow can't be created %d message: %s\n",
			error.type,
			error.message ? error.message : "(no stated reason)");
		rte_exit(EXIT_FAILURE, "error in creating flow");
	}
}

void
simple_fwd_process_pkts_stop()
{
	force_quit = true;
}

int simple_fwd_process_pkts(void *process_pkts_params){
    uint64_t cur_tsc, last_tsc;
	struct rte_mbuf *mbufs[VNF_RX_BURST_SIZE];
	uint16_t j, nb_rx, queue_id;
	uint32_t port_id = 0, core_id = rte_lcore_id();
	struct vnf_per_core_params *params = &core_params_arr[core_id];
	struct simple_fwd_config *app_config = ((struct simple_fwd_process_pkts_params *) process_pkts_params)->cfg;
	struct app_vnf *vnf = ((struct simple_fwd_process_pkts_params *) process_pkts_params)->vnf;

    if (!params->used) {
		printf("``DEBUG: core %u nothing need to do", core_id);
		return 0;
	}
    printf("``INFO: core %u process queue %u start\n", core_id, params->queues[0]);
    last_tsc = rte_rdtsc();

	struct rte_ether_hdr *eth_hdr;
    while (!force_quit) {
		if (core_id == rte_get_main_lcore()) {
			cur_tsc = rte_rdtsc();
			if (cur_tsc > last_tsc + app_config->stats_timer) {
				simple_fwd_dump_port_stats(0);
				last_tsc = cur_tsc;
			}
		}
		for (port_id = 0; port_id < NUM_OF_PORTS; port_id++) {
			queue_id = params->queues[port_id];
			nb_rx = rte_eth_rx_burst(port_id, queue_id, mbufs, VNF_RX_BURST_SIZE);
			/*
			if (nb_rx) {
				for (j = 0; j < nb_rx; j++) {
					struct rte_mbuf *m = mbufs[j];

					eth_hdr = rte_pktmbuf_mtod(m,
							struct rte_ether_hdr *);
					print_ether_addr("src=",
							&eth_hdr->src_addr);
					print_ether_addr(" - dst=",
							&eth_hdr->dst_addr);
					printf(" - port_id=0x%x",
							(unsigned int)port_id);
					printf("\n");				
					}
			}*/
			
			for (j = 0; j < nb_rx; j++) {
				if (app_config->hw_offload && core_id == rte_get_main_lcore())
					{
						// printf("core_id: %d\n",core_id);
						simple_fwd_process_offload(mbufs[j], queue_id, vnf);
					}
				if (app_config->rx_only)
					rte_pktmbuf_free(mbufs[j]);
				else
					rte_eth_tx_burst(port_id ^ 1, queue_id, &mbufs[j], 1);
			}
			if (!app_config->age_thread)
				vnf->vnf_flow_age(port_id, queue_id);
		}
	}
	return 0;
}