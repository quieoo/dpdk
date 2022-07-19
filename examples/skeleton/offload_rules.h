#ifndef COMMON_OFFLOAD_RULES_H_
#define COMMON_OFFLOAD_RULES_H_

#include<stdbool.h>

struct application_port_config {
	int nb_ports;	  /* Set on init to 0 for don't care, required ports otherwise */
	int nb_queues;	  /* Set on init to 0 for don't care, required minimum cores otherwise */
	int nb_hairpin_q; /* Set on init to 0 to disable, hairpin queues otherwise */
};

struct application_sft_config {
	bool enable;	  /* Enable SFT */
	bool enable_ct;	  /* Enable connection tracking feature of SFT */
	bool enable_frag; /* Enable fragmentation feature of SFT  */
	bool enable_state_hairpin;
	bool enable_state_drop;
};

struct application_dpdk_config {
	struct application_port_config port_config;
	struct application_sft_config sft_config;
	bool reserve_main_thread;
#ifdef GPU_SUPPORT
	struct gpu_pipeline pipe;
#endif
};

#endif /* COMMON_OFFLOAD_RULES_H_ */