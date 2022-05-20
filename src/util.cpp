/** @file util.cpp
 *  @copyright 2016 Peter Watkins. All rights reserved.
 */

#include "util.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "common.h"

// TODO: This looks like it was required before. Perhaps Sierra now includes it.
//#ifdef __MACH__
//#include <sys/time.h>
//clock_gettime is not implemented on OSX.
// TODO: Look into this implementation. Is this the proper thing to use
// for Linux and OS X?
// int clock_gettime(int /*clk_id*/, struct timespec* t) {
//     struct timeval now;
//     int rv = gettimeofday(&now, NULL);
//     if (rv) return rv;
//     t->tv_sec  = now.tv_sec;
//     t->tv_nsec = now.tv_usec * 1000;
//     return 0;
// }
//#endif

/* SizeStr2Number: Convert size string to number */
size_t SizeStr2Number(const char *str)
{
        size_t len = strnlen(str, OPTION_MAX);
        size_t unit, size = 0, magnitude = 1;
        const char *sp = str + (len - 1);
        /* ignore spaces and get the unit character */
        while (*sp == ' ') {
                --sp;
        }

        char uc = *sp;
        if (uc >= '0' && uc <= '9') {
                unit = 1;
                ++sp;
        } else if (uc == 'k' || uc == 'K') {
                unit = 1024UL;
        } else if (uc == 'm' || uc == 'M') {
                unit = 1048576UL;
        } else if (uc == 'g' || uc == 'G') {
                unit = 1048576UL * 1024UL;
        } else if (uc == 't' || uc == 'T') {
                unit = 1048576UL * 1048576UL;
        } else if (uc == 'p' || uc == 'P') {
                unit = 1048576UL * 1048576UL * 1024;
        } else if (uc == 'e' || uc == 'E') {
                unit = 1048576UL * 1048576UL * 1048576UL;
        } else {
                /* Unsupported unit */
                return 0;
        }
        --sp;
        /* Gather the number value */
        for (; sp >= str; --sp, magnitude *= 10) {
                char cur = *sp;
                if (cur < '0' || cur > '9') {
                        /* Encountered non-number, quit */
                        return 0;
                }
                size += magnitude * (cur - '0');
        }
        return size * unit;
}

/* ramfs_parse_options: Parse option string provided by -o argument
 *
 * @param[in] optstr:   Option string 
 * @param[in] opt:      The fuse_ramfs_options structure for config values
 * 
 * The option string should be in the following form:
 * "[-.*o]key1=value1,key2=value2,key3=value3,switch1,switch2,..."
 * Currently this function parses the following options:
 *   - size     Capacity of the RAM file system. Supports unit suffix
 *              including k,m,g,t,p,e.
 *   - inodes   Inode slots of the file system. Also supports unit suffix.
 *   - subtype  Subtype name to be displayed in mount list.
 * 
 * @return: The new string buffer containing the original option string
 *   with the parsed options excluded.
 */
char *ramfs_parse_options(char *optstr, struct fuse_ramfs_options &opt) {
    char *ctx1, *kvtoken; 
    char *newopt = new char[strnlen(optstr, OPTION_MAX) + 1];
    char *ptr = newopt;
    printf("opt string: %s\n", optstr);
    /* bypass if there is prefix switches '-.*o' */
    if (*optstr == '-') {
        while (*optstr != 'o') {
            *(ptr++) = *(optstr++);
        }
        *(ptr++) = *(optstr++);
    }
    kvtoken = strtok_r(optstr, ",", &ctx1);
    while (kvtoken) {
        char *ctx2;
        char *key = strtok_r(kvtoken, "=", &ctx2);
        char *value = strtok_r(nullptr, "=", &ctx2);
        printf("kvtoken: %s, key=%s, value=%s\n", kvtoken, key, (value) ? value : "<null>");
        if (key && strncmp(key, "size", OPTION_MAX) == 0) {
            if (value) {
                opt.capacity = SizeStr2Number(value);
                printf("Custom capacity: %zu bytes\n", opt.capacity);
            }
        } else if (key && strncmp(key, "inodes", OPTION_MAX) == 0) {
            if (value) {
                opt.inodes = SizeStr2Number(value);
                printf("Custom inode slots: %zu\n", opt.inodes);
            }
        } else if (key && strncmp(key, "subtype", OPTION_MAX) == 0) {
            if (value) {
                opt.subtype = value;
                printf("Custom subtype: %s\n", value);
            }
        } else {
            if (key == nullptr) {
                continue;
            }
            /* For options not recognized by this function, let's copy
             * them into the newopt buffer for future use (e.g. fuse_mount)
             */
            size_t keylen = strnlen(key, OPTION_MAX);
            /* If the cursor of newopt buffer is not at the beginning,
             * add comma */
            if (ptr > newopt)
              *(ptr++) = ',';
            strncpy(ptr, key, keylen);
            ptr += keylen;
            if (value) {
                *(ptr++) = '=';
                size_t valen = strnlen(value, OPTION_MAX);
                strncpy(ptr, value, valen);
                ptr += valen;
            }
        }
        kvtoken = strtok_r(nullptr, ",", &ctx1);
    }
    /* Verify options */
    if (opt.capacity > 0 && opt.capacity < 1024) {
        printf("fuse-cpp-ramfs requires at least 1KB capacity.\n");
        exit(1);
    }
    if (opt.inodes > 0 && opt.inodes < 2) {
        printf("fuse-cpp-ramfs requires at least 2 inode slots\n");
        exit(1);
    }

    /* Erase '-o' if no option string is left */
    if (*(--ptr) == 'o') {
        *ptr = 0;
        if (*(--ptr) == '-') {
            *ptr = 0;
        }
    }

    return newopt;
}

void ramfs_set_subtype(struct fuse_args *args, const char *subtype) {
    /* Let it crash if std::bad_alloc occurred */
    size_t len = strnlen(subtype, OPTION_MAX) + 32;
    char *subtype_str = new char[len];
    if (subtype_str == nullptr) {
        printf("Cannot allocate memory.\n");
        exit(2);
    }
    snprintf(subtype_str, len, "-osubtype=%s", subtype);
    fuse_opt_add_arg(args, subtype_str);
    delete[] subtype_str;
}

void erase_arg(struct fuse_args &args, int index) {
    if (index >= args.argc) {
        return;
    }

    char *target = args.argv[index];
    for (int i = index; i < args.argc - 1; ++i) {
        args.argv[i] = args.argv[i+1];
    }
    args.argv[args.argc - 1] = nullptr;
    args.argc--;
    if (args.allocated) {
        free(target);
    }
}
/* ramfs_parse_cmdline: Parse command line arguments
 * 
 * @params[in,out] args: CMD arguments wrapped in fuse_args
 * @params[out]    options: RAM file system config structure
 * 
 * NOTE: After parsing this function will remove parsed arguments
 *       from args except '-o [options]'
 */
void ramfs_parse_cmdline(struct fuse_args &args, struct fuse_ramfs_options &options) {
    int opt, argidx = 0;
    size_t optlen; 
    char *optstr_buf;
    int o_idx1 = 0, argo_idx = 0;
    while ((opt = getopt(args.argc, args.argv, "o:b")) != -1) {
        switch (opt) {
            case 'o':
                /* There might be cases in which the option string is
                 * concantenated with the switch '-[...]o'.  We should
                 * pass the whole string (i.e. argv[optind - 1]) instead
                 * of merely the arg (i.e. optarg) into ramfs_parse_options.
                 */
                optlen = strnlen(args.argv[optind - 1], OPTION_MAX) + 1;
                optstr_buf = new char[optlen];
                strncpy(optstr_buf, args.argv[optind - 1], optlen);
                options._optstr = ramfs_parse_options(optstr_buf, options);
                delete[] optstr_buf;
                /* Mark for erase of -o arg if the output option str is empty */
                if (strnlen(options._optstr, OPTION_MAX) == 0) {
                    argo_idx = optind - 1;
                    /* NOTE: did not handle strings like "-abo k=1,v=2..." */
                    if (strncmp(args.argv[optind - 2], "-o", OPTION_MAX) == 0) {
                        o_idx1 = optind - 2;
                    }
                } else {
                    if (args.allocated) {
                        delete[] args.argv[optind - 1];
                    }
                    args.argv[optind - 1] = options._optstr;
                }
                break;

            case 'b':
                options.deamonize = true;
                printf("Elected to deamonize fuse-cpp-ramfs\n");
                erase_arg(args, optind - 1);
                optind = optind - 1;
                break;

            default:
                continue;
        }
    }
    if (optind >= args.argc) {
        printf("Missing mountpoint.\n");
        exit(1);
    } else if (optind == args.argc - 1) {
        size_t mplen = strnlen(args.argv[optind], OPTION_MAX) + 1;
        char *mp = new char[mplen];
        strncpy(mp, args.argv[optind], mplen);
        /* We need to remove the mountpoint from the arg vector as well */
        erase_arg(args, optind);
        options.mountpoint = mp;
        if (options.subtype == nullptr) {
            options.subtype = basename(args.argv[0]);
            ramfs_set_subtype(&args, options.subtype);
        }
    } else {
        printf("Too many arguments?\n");
        exit(1);
    }

    /* Remove '-o' arg if there is no remaining option */
    if (argo_idx) {
        erase_arg(args, argo_idx);
    }
    if (o_idx1) {
        erase_arg(args, o_idx1);
    }
    // These two fuse options are required to fix permission related issues.
    fuse_opt_add_arg(&args, "-odefault_permissions");
    fuse_opt_add_arg(&args, "-oallow_other");
    printf("Mount name: %s\n", options.subtype);
    printf("Mountpoint path: %s\n", options.mountpoint);

#ifndef NDEBUG
    for (int i = 0; i < args.argc; ++i) {
        printf("argv[%d]: %s\n", i, args.argv[i]);
    }
#endif
}
