
#include <stdbool.h>

#define MAX_SERVER_ADDRESS 24
typedef void (*callback_func)(void *, void *);


struct doca_argp_program_general_config {
	int log_level;					/**< The log level as provided by the user */
	char grpc_address[MAX_SERVER_ADDRESS];		/**< The gRPC server address as provided by the user */
};


struct doca_argp_program_type_config {
	bool is_dpdk;					/**< Is the program based on DPDK API */
	bool is_grpc;					/**< Is the program based on gRPC API */
};


enum doca_argp_type {
	DOCA_ARGP_TYPE_STRING = 0,			/**< Input type is a string */
	DOCA_ARGP_TYPE_INT,				/**< Input type is an integer */
	DOCA_ARGP_TYPE_BOOLEAN,				/**< Input type is a boolean */
	DOCA_ARGP_TYPE_JSON_OBJ,			/**< DPDK Param input type is a json object,
							  * only for json mode */
};
struct doca_argp_param {
	char *short_flag;				/**< Flag long name */
	char *long_flag;				/**< Flag short name */
	char *arguments;				/**< Flag expected arguments */
	char *description;				/**< Flag description */
	callback_func callback;				/**< Flag program callback */
	enum doca_argp_type arg_type;			/**< Flag argument type */
	bool is_mandatory;				/**< Is flag mandatory for the program */
	bool is_cli_only;				/**< Is flag supported only in cli mode */
};

void doca_argp_init(const char *program_name, struct doca_argp_program_type_config *type_config, void *program_config);


void doca_argp_register_param(struct doca_argp_param *input_param);



void doca_argp_start(int argc, char **argv, struct doca_argp_program_general_config **general_config);

