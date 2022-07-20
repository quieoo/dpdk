#include <stdint.h>
#include <signal.h>

#include "simple_fwd.h"
#include "simple_fwd_port.h"
#include "simple_fwd_vnf_core.h"

#include "doca_argp.h"
#include "dpdk_utils.h"

#define DEFAULT_NB_METERS (1 << 13)

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("``DEBUG: Signal %d received, preparing to exit...", signum);
		simple_fwd_process_pkts_stop();
	}
}

int
main(int argc, char **argv)
{
    printf("``this is dpdk_simple_fwd_vnf application\n");

    uint16_t port_id;
	struct simple_fwd_port_cfg port_cfg = {0};
	struct application_dpdk_config dpdk_config = {
		.port_config.nb_ports = 2,
		.port_config.nb_queues = 4,
		.port_config.nb_hairpin_q = 0,
		.sft_config = {0},
		.reserve_main_thread = 1,
	};

    struct simple_fwd_config app_cfg = {
		.dpdk_cfg = &dpdk_config,
		.rx_only = 0,
		.hw_offload = 1,
		.stats_timer = 100000,
		.age_thread = false,
	};

    struct app_vnf *vnf;
	struct simple_fwd_process_pkts_params process_pkts_params = {.cfg = &app_cfg};

    /*
        doca_argp.h: 
            doca_argp_init: null
            register_simple_fwd_params: null
            doca_argp_start: rte_eal_init(argc, argv)
    */
    struct doca_argp_program_general_config *doca_general_config;
	struct doca_argp_program_type_config type_config = {
		.is_dpdk = true,
		.is_grpc = false,
	};
    doca_argp_init("simple_forward_vnf", &type_config, &app_cfg);
	register_simple_fwd_params();
	doca_argp_start(argc, argv, &doca_general_config);

    /*
        doca_log.h:
            doca_log_create_syslog_backend: null
    */
    doca_log_create_syslog_backend("doca_core");

	/*
		dpdk_init: dpdk
	*/
    dpdk_init(&dpdk_config);

	/*
		termination handle
	*/
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* convert to number of cycles */
	app_cfg.stats_timer *= rte_get_timer_hz();

	vnf = simple_fwd_get_vnf();
	port_cfg.nb_queues = dpdk_config.port_config.nb_queues;
	port_cfg.is_hairpin = !!dpdk_config.port_config.nb_hairpin_q;
	port_cfg.nb_meters = DEFAULT_NB_METERS;
	port_cfg.nb_counters = (1 << 13);

	if (vnf->vnf_init(&port_cfg) != 0) {
		printf("``DEBUG: vnf application init error");
		goto exit_app;
	}

exit_app:


}