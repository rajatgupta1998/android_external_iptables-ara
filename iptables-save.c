/* Code to save the iptables state, in human readable-form. */
/* (C) 1999 by Paul 'Rusty' Russell <rusty@rustcorp.com.au> and
 * (C) 2000-2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This code is distributed under the terms of GNU GPL v2
 *
 */
#include <getopt.h>
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include "libiptc/libiptc.h"
#include "iptables.h"
#include "iptables-multi.h"

#ifndef NO_SHARED_LIBS
#include <dlfcn.h>
#endif

static int show_binary = 0, show_counters = 0;

static const struct option options[] = {
	{.name = "binary",   .has_arg = false, .val = 'b'},
	{.name = "counters", .has_arg = false, .val = 'c'},
	{.name = "dump",     .has_arg = false, .val = 'd'},
	{.name = "table",    .has_arg = true,  .val = 't'},
	{NULL},
};

/* Debugging prototype. */
static int for_each_table(int (*func)(const char *tablename))
{
	int ret = 1;
	FILE *procfile = NULL;
	char tablename[IPT_TABLE_MAXNAMELEN+1];

	procfile = fopen("/proc/net/ip_tables_names", "r");
	if (!procfile)
		exit_error(OTHER_PROBLEM,
			   "Unable to open /proc/net/ip_tables_names: %s\n",
			   strerror(errno));

	while (fgets(tablename, sizeof(tablename), procfile)) {
		if (tablename[strlen(tablename) - 1] != '\n')
			exit_error(OTHER_PROBLEM, 
				   "Badly formed tablename `%s'\n",
				   tablename);
		tablename[strlen(tablename) - 1] = '\0';
		ret &= func(tablename);
	}

	return ret;
}


static int do_output(const char *tablename)
{
	struct iptc_handle *h;
	const char *chain = NULL;

	if (!tablename)
		return for_each_table(&do_output);

	h = iptc_init(tablename);
	if (!h)
		exit_error(OTHER_PROBLEM, "Can't initialize: %s\n",
			   iptc_strerror(errno));

	if (!show_binary) {
		time_t now = time(NULL);

		printf("# Generated by iptables-save v%s on %s",
		       XTABLES_VERSION, ctime(&now));
		printf("*%s\n", tablename);

		/* Dump out chain names first,
		 * thereby preventing dependency conflicts */
		for (chain = iptc_first_chain(h);
		     chain;
		     chain = iptc_next_chain(h)) {

			printf(":%s ", chain);
			if (iptc_builtin(chain, h)) {
				struct ipt_counters count;
				printf("%s ",
				       iptc_get_policy(chain, &count, h));
				printf("[%llu:%llu]\n", (unsigned long long)count.pcnt, (unsigned long long)count.bcnt);
			} else {
				printf("- [0:0]\n");
			}
		}


		for (chain = iptc_first_chain(h);
		     chain;
		     chain = iptc_next_chain(h)) {
			const struct ipt_entry *e;

			/* Dump out rules */
			e = iptc_first_rule(chain, h);
			while(e) {
				print_rule(e, h, chain, show_counters);
				e = iptc_next_rule(e, h);
			}
		}

		now = time(NULL);
		printf("COMMIT\n");
		printf("# Completed on %s", ctime(&now));
	} else {
		/* Binary, huh?  OK. */
		exit_error(OTHER_PROBLEM, "Binary NYI\n");
	}

	iptc_free(h);

	return 1;
}

/* Format:
 * :Chain name POLICY packets bytes
 * rule
 */
#ifdef IPTABLES_MULTI
int
iptables_save_main(int argc, char *argv[])
#else
int
main(int argc, char *argv[])
#endif
{
	const char *tablename = NULL;
	int c;

	program_name = "iptables-save";
	program_version = XTABLES_VERSION;

	xtables_init();
#ifdef NO_SHARED_LIBS
	init_extensions();
#endif

	while ((c = getopt_long(argc, argv, "bcdt:", options, NULL)) != -1) {
		switch (c) {
		case 'b':
			show_binary = 1;
			break;

		case 'c':
			show_counters = 1;
			break;

		case 't':
			/* Select specific table. */
			tablename = optarg;
			break;
		case 'd':
			do_output(tablename);
			exit(0);
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Unknown arguments found on commandline\n");
		exit(1);
	}

	return !do_output(tablename);
}
