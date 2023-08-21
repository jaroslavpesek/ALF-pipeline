#include <iostream>
#include <csignal>

#include <getopt.h>

#include <libtrap/trap.h>
#include <unirec/unirec.h>

#include "blacklist.h"
#include "fields.h"

UR_FIELDS ( 
    ipaddr DST_IP,
    uint16 DST_PORT,
)

#define MODULE_BASIC_INFO(BASIC) \
    BASIC("miner_filter", "Miner blacklist filter.\n", 1, 2)

#define MODULE_PARAMS(PARAM) \
    PARAM('b', "blacklist", "Blaclist file in format 'IP port\\n'.", required_argument, "filename") 

trap_module_info_t *module_info = NULL;
static volatile int stop = 0;

TRAP_DEFAULT_SIGNAL_HANDLER(stop = 1)

static int
do_mainloop(Blacklist& blacklist)
{
    int ret;
    uint16_t data_size;
    const void *data;
    ur_template_t *tmplt;

    tmplt = ur_create_input_template(0, "DST_IP,DST_PORT", NULL);
    if (tmplt == NULL) {
        std::cerr << "Error: Input template could not be created." << std::endl;
        return 1;
    }

    trap_set_required_fmt(0, TRAP_FMT_UNIREC, "");

    while (!stop) {
        ret = trap_recv(0, &data, &data_size);
        TRAP_DEFAULT_RECV_ERROR_HANDLING(ret, continue, break);
        if (data_size <= 1) {
            stop = 1;
            break;
        }

        if (ret == TRAP_E_FORMAT_CHANGED) {
            // Get the data format of senders output interface (the data format of the output interface it is connected to)
            const char *spec = NULL;
            uint8_t data_fmt = TRAP_FMT_UNKNOWN;
            if (trap_get_data_fmt(TRAPIFC_INPUT, 0, &data_fmt, &spec) != TRAP_E_OK) {
               std::cerr << "Data format was not loaded." << std::endl;
               return 1;
            }

            tmplt = ur_define_fields_and_update_template(spec, tmplt);

            // Set the same data format to repeaters output interface
            trap_set_data_fmt(0, TRAP_FMT_UNIREC, spec);
            trap_set_data_fmt(1, TRAP_FMT_UNIREC, spec);
        }

        struct filter_pair filter_pair(
            *static_cast<ip_addr_t*>(ur_get_ptr(tmplt, data, F_DST_IP)),
            *static_cast<uint16_t*>(ur_get_ptr(tmplt, data, F_DST_PORT)));

        if (blacklist.is_blacklisted(filter_pair) == true) {
            ret = trap_send(0, data, data_size);
            TRAP_DEFAULT_SEND_DATA_ERROR_HANDLING(ret, continue, break)
        } else {
            ret = trap_send(1, data, data_size);
            TRAP_DEFAULT_SEND_DATA_ERROR_HANDLING(ret, continue, break)
        }
    }

    ur_free_template(tmplt);
    return 0;
}

int
main(int argc, char **argv)
{
    Blacklist blacklist;
    char *blacklist_path = nullptr;
    char opt;

    // Macro allocates and initializes module_info structure according to MODULE_BASIC_INFO.
    INIT_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);

    // Let TRAP library parse program arguments, extract its parameters and initialize module interfaces
    TRAP_DEFAULT_INITIALIZATION(argc, argv, *module_info);

    TRAP_REGISTER_DEFAULT_SIGNAL_HANDLER();

    while ((opt = TRAP_GETOPT(argc, argv, module_getopt_string, long_options)) != -1) {
        switch (opt) {
        case 'b':
            blacklist_path = optarg;
            break;
        default:
            std::cerr << "Invalid argument " << opt << ", skipped..." << std::endl;
        }
    }

    if (!blacklist_path) {
        std::cerr << "Blacklist file is missing." << std::endl;
        goto failure;
    }


    if (blacklist.load_blacklist(blacklist_path)) {
        goto failure;
    }

    do_mainloop(blacklist);

    trap_terminate();
    FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);
    TRAP_DEFAULT_FINALIZATION();
    return 0;

failure:
    trap_terminate();
    FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS);
    TRAP_DEFAULT_FINALIZATION();
    return 1;
}
