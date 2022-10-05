#ifndef _SOFT_FLOW_H_
#define _SOFT_FLOW_H_

#include "rte_flow.h"

#define MAX_PORT_QUEUE 32*1024 //RTE_MAX_ETHPORTS * RTE_MAX_QUEUES_PER_PORT
#define MAX_PATTERN_LENGTH 10
#define MAX_ACTION_LENGTH 10
#define MAX_FLOW_RULE 1<<20  //1M flows maxminum



typedef struct rte_flow{
	struct rte_flow_attr attr;
    struct rte_flow_item pattern[MAX_PATTERN_LENGTH];
    struct rte_flow_action actions[MAX_ACTION_LENGTH];
};




int soft_flow_validate(uint16_t port_id,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error);

struct rte_flow *
soft_flow_create(uint16_t port_id,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);

int flow_process(uint16_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

#endif