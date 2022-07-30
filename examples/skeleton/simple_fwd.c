#include "simple_fwd.h"
#include "app_vnf.h"




static int
simple_fwd_init(void *p)
{
	printf("``simple_fwd_init\n");
	return 0;

}

static int
simple_fwd_handle_packet(struct simple_fwd_pkt_info *pinfo)
{

}

static int
simple_fwd_destroy(void)
{


}

static void
simple_fwd_handle_aging(uint32_t port_id, uint16_t queue)
{


}

struct app_vnf simple_fwd_vnf = {
	.vnf_init = &simple_fwd_init,
	.vnf_process_pkt = &simple_fwd_handle_packet,
	.vnf_destroy = &simple_fwd_destroy,
	.vnf_flow_age = &simple_fwd_handle_aging,
};

struct app_vnf*
simple_fwd_get_vnf(void)
{
	return &simple_fwd_vnf;
}
