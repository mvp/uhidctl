/*
 * Copyright (c) 2017-2020 Vadim Mikhailov
 *
 * uhidctl - utility to control USB HID power relays.
 *
 * This file can be distributed under the terms and conditions of the
 * GNU General Public License version 2.
 *
 */

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <errno.h>
#include <ctype.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <process.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#endif

#if _POSIX_C_SOURCE >= 199309L
#include <time.h>   /* for nanosleep */
#endif

#include <hidapi/hidapi.h>


/* Max number of relay ports supported */

#define MAX_RELAY_PORTS  8
#define ALL_RELAY_PORTS  ((1 << MAX_RELAY_PORTS) - 1) /* bitmask */

#define POWER_KEEP       (-1)
#define POWER_OFF        0
#define POWER_ON         1
#define POWER_CYCLE      2

struct relay_info {
    char serial[16];
    int  nports;
    char path[256];
};


/* Array of all enumerated relays */
#define MAX_RELAYS 64
static struct relay_info relays[MAX_RELAYS];
static int relay_count = 0;


/* default options */
static char opt_relay[16] = "";          /* Serial number of relay to operate on */
static char opt_path[16] = "";           /* USB path of relay to operate on */
static int opt_ports  = ALL_RELAY_PORTS; /* Bitmask of relay ports to operate on */
static int opt_action = POWER_KEEP;      /* Power action */
static double opt_delay = 2;             /* Delay for power cycle */

static const struct option long_options[] = {
    { "relay" ,   required_argument, NULL, 'l' },
    { "path",     required_argument, NULL, 'u' },
    { "ports",    required_argument, NULL, 'p' },
    { "action",   required_argument, NULL, 'a' },
    { "delay",    required_argument, NULL, 'd' },
    { "version",  no_argument,       NULL, 'v' },
    { "help",     no_argument,       NULL, 'h' },
    { 0,          0,                 NULL, 0   },
};


int print_usage()
{
    printf(
        "uhidctl: control USB HID power relays.\n"
        "Usage: uhidctl [options]\n"
        "Without options, show status for all relays.\n"
        "\n"
        "Options [defaults in brackets]:\n"
        "--relay,    -l - specific relay (serial number) to operate on.\n"
        "--path,     -u - specific relay (usb path) to operator on.\n"
        "--ports,    -p - ports to operate on [all ports].\n"
        "--action,   -a - action to off/on/cycle (0/1/2) for affected ports.\n"
        "--delay,    -d - delay for power cycle [%g sec].\n"
        "--version,  -v - print program version.\n"
        "--help,     -h - print this text.\n"
        "\n"
        "Send bugs and requests to: https://github.com/mvp/uhidctl\n"
        "version: %s\n",
        opt_delay,
        PROGRAM_VERSION
    );
    return 0;
}

/* cross-platform sleep function */

void sleep_ms(int milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    usleep(milliseconds * 1000);
#endif
}

/*
 * Convert port list into bitmap.
 * Following port list specifications are equivalent:
 *   1,3,4,5,11,12,13
 *   1,3-5,11-13
 * Returns: bitmap of specified ports, max port is MAX_RELAY_PORTS.
 */

static int ports2bitmap(char* const portlist)
{
    int ports = 0;
    char* position = portlist;
    char* comma;
    char* dash;
    int len;
    int i;
    while (position) {
        char buf[8] = {0};
        comma = strchr(position, ',');
        len = sizeof(buf) - 1;
        if (comma) {
            if (len > comma - position)
                len = comma - position;
            strncpy(buf, position, len);
            position = comma + 1;
        } else {
            strncpy(buf, position, len);
            position = NULL;
        }
        /* Check if we have port range, e.g.: a-b */
        int a=0, b=0;
        a = atoi(buf);
        dash = strchr(buf, '-');
        if (dash) {
            b = atoi(dash+1);
        } else {
            b = a;
        }
        if (a > b) {
            fprintf(stderr, "Bad port spec %d-%d, first port must be less than last\n", a, b);
            exit(1);
        }
        if (a <= 0 || a > MAX_RELAY_PORTS || b <= 0 || b > MAX_RELAY_PORTS) {
            fprintf(stderr, "Bad port spec %d-%d, port numbers must be from 1 to %d\n", a, b, MAX_RELAY_PORTS);
            exit(1);
        }
        for (i=a; i<=b; i++) {
            ports |= (1 << (i-1));
        }
    }
    return ports;
}


/*
 *  Find all USB relays that we are going to work with and fill relays[] array.
 *  This applies possible constraints like serial number.
 *  Returns count of found relays or negative error code.
 */

static int find_relays()
{
    struct hid_device_info *devs, *cur_dev;
    hid_device *handle;
    char serial[9];
    int nports;
    int rc;
    int perm_ok = 1;

    relay_count = 0;
    devs = hid_enumerate(0, 0);

    for (cur_dev = devs; cur_dev; cur_dev = cur_dev->next) {
        if (cur_dev->product_string == NULL)
            continue;
        if (wcslen(cur_dev->product_string) < 8)
            continue;
        if (wcsncmp(cur_dev->product_string, L"USBRelay", 7))
            continue;

        if (strlen(opt_path)>0 && strcasecmp(cur_dev->path, opt_path))
            continue;

        handle = hid_open_path(cur_dev->path);
        if (!handle) {
            fprintf(stderr, "Unable to open relay at [%s]", cur_dev->path);
            perm_ok = 0; /* Permission issue? */
            continue;
        }

        serial[0] = 1;
        rc = hid_get_feature_report(handle, (unsigned char*)serial, sizeof(serial));
        if (rc == -1) {
            fprintf(stderr, "Can't get serial number for relay at [%s]\n", cur_dev->path);
            continue;
        }
        if (strlen(opt_relay)>0 && strcasecmp(serial, opt_relay))
            continue;
        nports = wcstol(cur_dev->product_string+8, 0, 0);
        if (nports <= 0)
            continue;
        if (relay_count < MAX_RELAYS) {
            strncpy(relays[relay_count].serial, serial, sizeof(relays[relay_count].serial));
            relays[relay_count].nports = nports;
            strncpy(relays[relay_count].path, cur_dev->path, sizeof(relays[relay_count].path));
            relay_count++;
        } else {
            fprintf(stderr, "Too many relays!\n");
            exit(1);
        }
        hid_close(handle);
    }
    hid_free_enumeration(devs);

#ifdef __gnu_linux__
    if (!perm_ok) {
        fprintf(stderr,
            "There were permission problems while accessing USB.\n"
            "To fix this, run this tool as root using 'sudo uhidctl',\n"
            "or add one or more udev rules like below\n"
            "to file '/etc/udev/rules.d/52-usb.rules':\n"
            "SUBSYSTEM==\"usb\", ATTR{idVendor}==\"16c0\", MODE=\"0666\"\n"
            "then run 'sudo udevadm trigger --attr-match=subsystem=usb'\n"
        );
    }
#endif
    return relay_count;
}


/*
 * Get relay port state.
 * Return value: 0 = OFF, 1 = ON, -1 = error occured.
 */

static int get_port_state(struct relay_info* info, int port)
{
    int rc;
    hid_device *handle;
    unsigned char buf[9] = { 1 };

    if (info == NULL)
        return -1;
    if (port < 1 || port > info->nports)
        return -1;
    handle = hid_open_path(info->path);
    if (handle == NULL)
        return -1;

    rc = hid_get_feature_report(handle, buf, sizeof(buf));
    hid_close(handle);
    if (rc < 0)
        return -1;

    rc = (buf[7] & (1 << (port - 1))) ? 1 : 0;
    return rc;
}


/*
 * Set relay port state.
 * Returns new port state: 0 = OFF, 1 = ON, -1 = error occured.
 */

static int set_port_state(struct relay_info* info, int port, int state)
{
    int rc = -1;
    hid_device *handle;
    unsigned char buf[9] = {0, state ? 0xFF : 0xFD, port};

    if (!info)
        return -1;
    if (port < 1 || port > info->nports)
        return -1;
    handle = hid_open_path(info->path);
    if (!handle)
        return -1;
    rc = hid_write(handle, buf, sizeof(buf));
    hid_close(handle);
    return rc;
}


/*
 * Print status for relay port(s).
 * If portmask is 0, show all ports.
 */

static int print_relay_status(struct relay_info* info, int portmask)
{
    int port;
    int state;
    if (!info)
        return -1;
    printf("Status for relay %s at [%s], %d ports:\n", info->serial, info->path, info->nports);
    for (port = 1; port <= info->nports; port++) {
        if (portmask > 0 && (portmask & (1 << (port-1))) == 0)
            continue;
        state = get_port_state(info, port);
        printf("  Port %d: %d %s\n", port, state, state ? "ON" : "OFF");
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int rc = 0;
    int c = 0;
    int option_index = 0;
    int i;

    for (;;) {
        c = getopt_long(argc, argv, "a:d:p:l:hv", long_options, &option_index);
        if (c == -1)
            break;  /* no more options left */
        switch (c) {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
                break;
            printf("option %s", long_options[option_index].name);
            if (optarg)
                printf(" with arg %s", optarg);
            printf("\n");
            break;
        case 'l':
            strncpy(opt_relay, optarg, sizeof(opt_relay));
            break;
        case 'u':
            strncpy(opt_path, optarg, sizeof(opt_path));
            break;
        case 'p':
            if (!strcasecmp(optarg, "all")) { /* all ports is the default */
                break;
            }
            if (strlen(optarg)) {
                /* parse port list */
                opt_ports = ports2bitmap(optarg);
            }
            break;
        case 'a':
            if (!strcasecmp(optarg, "off")          || !strcasecmp(optarg, "0")) {
                opt_action = POWER_OFF;
            } else if (!strcasecmp(optarg, "on")    || !strcasecmp(optarg, "1")) {
                opt_action = POWER_ON;
            } else if (!strcasecmp(optarg, "cycle") || !strcasecmp(optarg, "2")) {
                opt_action = POWER_CYCLE;
            } else {
                fprintf(stderr, "Invalid power action: %s. Run with -h to get usage info.\n", optarg);
                exit(1);
            }
            break;
        case 'd':
            opt_delay = atof(optarg);
            break;
        case 'v':
            printf("%s\n", PROGRAM_VERSION);
            exit(0);
            break;
        case 'h':
            print_usage();
            exit(1);
            break;
        case '?':
            /* getopt_long has already printed an error message here */
            fprintf(stderr, "Run with -h to get usage info.\n");
            exit(1);
            break;
        default:
            abort();
        }
    }
    if (optind < argc) {
        /* non-option parameters are found? */
        fprintf(stderr, "Invalid command line syntax!\n");
        fprintf(stderr, "Run with -h to get usage info.\n");
        exit(1);
    }

    rc = hid_init();
    if (rc < 0) {
        fprintf(stderr, "Error initializing hidapi!\n");
        exit(1);
    }

    rc = find_relays();

    if (rc <= 0) {
        fprintf(stderr,
            "No compatible relays detected!\n"
            "Run with -h to get usage info.\n"
        );
        rc = 1;
        goto cleanup;
    }

    if (opt_action == POWER_KEEP) {
        for (i = 0; i < relay_count; i++) {
            print_relay_status(relays+i, opt_ports);
        }
        rc = 0;
        goto cleanup;
    }

    if (relay_count > 1) {
        fprintf(stderr, "More than 1 relay found, choose one to operate with -l RELAY\n");
        for (i = 0; i < relay_count; i++) {
            fprintf(stderr, "%s\n", relays[i].serial);
        }
        rc = 1;
    } else {
        int port;
        int k; /* k=0 for power OFF, k=1 for power ON */
        for (k=0; k<2; k++) { /* up to 2 power actions - OFF/ON */
            int state;
            if (k == 0 && opt_action == POWER_ON )
                continue;
            if (k == 1 && opt_action == POWER_OFF)
                continue;
            state = k;
            for (port=1; port <= relays[0].nports; port++) {
                if ((1 << (port-1)) & opt_ports) {
                    rc = set_port_state(&relays[0], port, state);
                    if (rc < 0) {
                        fprintf(stderr, "Cannot set new port state!\n");
                        exit(1);
                    }
                }
            }
            print_relay_status(&relays[0], opt_ports);
            if (k==0 && opt_action == POWER_CYCLE) {
                sleep_ms(opt_delay * 1000);
            }
        }
        rc = 0;
    }

cleanup:
    hid_exit();
    return rc;
}
