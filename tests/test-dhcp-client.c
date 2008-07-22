/* Copyright (c) 2008 The Board of Trustees of The Leland Stanford
 * Junior University
 *
 * We are making the OpenFlow specification and associated documentation
 * (Software) available for public use and benefit with the expectation
 * that others will use, modify and enhance the Software and contribute
 * those enhancements back to the community. However, since we would
 * like to make the Software available for broadest use, with as few
 * restrictions as possible permission is hereby granted, free of
 * charge, to any person obtaining a copy of this Software to deal in
 * the Software under the copyrights without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * The name and trademarks of copyright holder(s) may NOT be used in
 * advertising or publicity pertaining to the Software or any
 * derivatives without specific, written prior permission.
 */

#include <config.h>
#include "dhcp-client.h"
#include <arpa/inet.h>
#include <getopt.h>
#include <stdlib.h>
#include <limits.h>
#include "command-line.h"
#include "dhcp.h"
#include "fatal-signal.h"
#include "fault.h"
#include "poll-loop.h"
#include "util.h"
#include "vlog.h"

/* --request-ip: IP address to request from server.  If zero, then do not
 * request a specific IP address. */
static struct in_addr request_ip;

/* --vendor-class: Vendor class string to include in request.  If null, no
 * vendor class string is included. */
static const char *vendor_class;

static void parse_options(int argc, char *argv[]);
static void usage(void);
static void release(void *cli_);
static void modify_dhcp_request(struct dhcp_msg *, void *aux);

int
main(int argc, char *argv[])
{
    struct dhclient *cli;
    int error;

    set_program_name(argv[0]);
    register_fault_handlers();
    vlog_init();
    parse_options(argc, argv);

    argc -= optind;
    argv += optind;
    if (argc != 1) {
        fatal(0, "exactly one non-option argument required; "
              "use --help for help");
    }

    error = dhclient_create(argv[0], modify_dhcp_request, NULL, NULL, &cli);
    if (error) {
        fatal(error, "dhclient_create failed");
    }
    dhclient_init(cli, request_ip.s_addr);
    fatal_signal_add_hook(release, cli);

    for (;;) {
        fatal_signal_block();
        dhclient_run(cli);
        if (dhclient_changed(cli)) {
            dhclient_configure_netdev(cli);
        }
        dhclient_wait(cli);
        fatal_signal_unblock();
        poll_block();
    }
}

static void
release(void *cli_)
{
    struct dhclient *cli = cli_;
    dhclient_release(cli);
    if (dhclient_changed(cli)) {
        dhclient_configure_netdev(cli);
    }
}

static void
modify_dhcp_request(struct dhcp_msg *msg, void *aux UNUSED)
{
    if (vendor_class) {
        dhcp_msg_put_string(msg, DHCP_CODE_VENDOR_CLASS, vendor_class);
    }
}

static void
parse_options(int argc, char *argv[])
{
    enum {
        OPT_REQUEST_IP = UCHAR_MAX + 1,
        OPT_VENDOR_CLASS
    };
    static struct option long_options[] = {
        {"request-ip",  required_argument, 0, OPT_REQUEST_IP },
        {"vendor-class", required_argument, 0, OPT_VENDOR_CLASS },
        {"verbose",     optional_argument, 0, 'v'},
        {"help",        no_argument, 0, 'h'},
        {"version",     no_argument, 0, 'V'},
        {0, 0, 0, 0},
    };
    char *short_options = long_options_to_short_options(long_options);

    for (;;) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {
        case OPT_REQUEST_IP:
            if (!inet_aton(optarg, &request_ip)) {
                fatal(0, "--request-ip argument is not a valid IP address");
            }
            break;

        case OPT_VENDOR_CLASS:
            vendor_class = optarg;
            break;

        case 'h':
            usage();

        case 'V':
            printf("%s "VERSION" compiled "__DATE__" "__TIME__"\n", argv[0]);
            exit(EXIT_SUCCESS);

        case 'v':
            vlog_set_verbosity(optarg);
            break;

        case '?':
            exit(EXIT_FAILURE);

        default:
            abort();
        }
    }
    free(short_options);
}

static void
usage(void)
{
    printf("%s: standalone program for testing OpenFlow DHCP client.\n"
           "usage: %s [OPTIONS] NETDEV\n"
           "where NETDEV is a network device (e.g. eth0).\n"
           "\nDHCP options:\n"
           "  --request-ip=IP         request specified IP address (default:\n"
           "                          do not request a specific IP)\n"
           "  --vendor-class=STRING   use STRING as vendor class (default:\n"
           "                          none); use OpenFlow to imitate secchan\n"
           "\nOther options:\n"
           "  -v, --verbose=MODULE[:FACILITY[:LEVEL]]  set logging levels\n"
           "  -v, --verbose           set maximum verbosity level\n"
           "  -h, --help              display this help message\n"
           "  -V, --version           display version information\n",
           program_name, program_name);
    exit(EXIT_SUCCESS);
}

