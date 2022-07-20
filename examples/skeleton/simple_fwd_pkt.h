
#ifndef SIMPLE_FWD_PKT_H_
#define SIMPLE_FWD_PKT_H_

#include <stdint.h>
#include <stdbool.h>

#define IPV4 (4)
#define IPV6 (6)


struct simple_fwd_pkt_format {

	uint8_t *l2;
	uint8_t *l3;
	uint8_t *l4;

	uint8_t l3_type;
	uint8_t l4_type;

	/* if tunnel it is the internal, if no tunnel then outer*/
	uint8_t *l7;
};


#endif /* SIMPLE_FWD_PKT_H_ */
