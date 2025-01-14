#ifndef SIMPLE_FWD_PORT_H_

#define SIMPLE_FWD_PORT_H_

#include <stdint.h>
#include <signal.h>
#include <stdbool.h>

#define NUM_OF_PORTS (2)

struct simple_fwd_port_cfg {
	uint16_t port_id;
	uint16_t nb_queues;
	uint32_t nb_meters;
	uint32_t nb_counters;
	uint16_t is_hairpin;
	bool age_thread;
};


void
simple_fwd_dump_port_stats(uint16_t port_id);

#endif /* SIMPLE_FWD_PORT_H_ */

