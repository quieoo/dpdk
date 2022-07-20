#include "simple_fwd_vnf_core.h"


static volatile bool force_quit;

void register_simple_fwd_params(void){
    printf("``register_simple_fwd_params called \n");
}


void
simple_fwd_process_pkts_stop()
{
	force_quit = true;
}