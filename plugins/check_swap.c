/*****************************************************************************
 *
 * Monitoring check_swap plugin
 *
 * License: GPL
 * Copyright (c) 2000 Karl DeBisschop (kdebisschop@users.sourceforge.net)
 * Copyright (c) 2000-2024 Monitoring Plugins Development Team
 *
 * Description:
 *
 * This file contains the check_swap plugin
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *****************************************************************************/

#include "common.h"
#include "output.h"
#include "states.h"
#include <limits.h>
#ifdef HAVE_DECL_SWAPCTL
#	ifdef HAVE_SYS_PARAM_H
#		include <sys/param.h>
#	endif
#	ifdef HAVE_SYS_SWAP_H
#		include <sys/swap.h>
#	endif
#	ifdef HAVE_SYS_STAT_H
#		include <sys/stat.h>
#	endif
#endif

#include <stdint.h>
#include "./check_swap.d/check_swap.h"
#include "./utils.h"

typedef struct {
	int errorcode;
	swap_config config;
} swap_config_wrapper;

static swap_config_wrapper process_arguments(int argc, char **argv);
void print_usage(void);
static void print_help(swap_config /*config*/);

int verbose;

#define HUNDRED_PERCENT 100

#define BYTES_TO_KiB(number) (number / 1024)
#define BYTES_TO_MiB(number) (BYTES_TO_KiB(number) / 1024)

const char *progname = "check_swap";
const char *copyright = "2000-2024";
const char *email = "devel@monitoring-plugins.org";

int main(int argc, char **argv) {
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* Parse extra opts if any */
	argv = np_extra_opts(&argc, argv, progname);

	swap_config_wrapper tmp = process_arguments(argc, argv);

	if (tmp.errorcode != OK) {
		usage4(_("Could not parse arguments"));
	}

	swap_config config = tmp.config;

	swap_result data = get_swap_data(config);

	if (data.errorcode != STATE_OK) {
		puts("SWAP UNKNOWN - Failed to retrieve Swap usage");
		exit(STATE_UNKNOWN);
	}

	if (verbose) {
		printf("Swap retrieval result:\n"
			   "\tFree: %llu\n"
			   "\tUsed: %llu\n"
			   "\tTotal: %llu\n",
			   data.metrics.free, data.metrics.used, data.metrics.total);
	}

	double percent_used;
	mp_check overall = mp_check_init();
	if (config.output_format_is_set) {
		mp_set_format(config.output_format);
	}
	mp_subcheck sc1 = mp_subcheck_init();
	sc1 = mp_set_subcheck_default_state(sc1, STATE_OK);

	/* if total_swap_mb == 0, let's not divide by 0 */
	if (data.metrics.total != 0) {
		percent_used = HUNDRED_PERCENT * ((double)data.metrics.used) / ((double)data.metrics.total);
	} else {
		sc1 = mp_set_subcheck_state(sc1, config.no_swap_state);
		sc1.output = (char *)_("Swap is either disabled, not present, or of zero size.");

		mp_add_subcheck_to_check(&overall, sc1);
		mp_exit(overall);
	}

	if (verbose) {
		printf("Computed usage percentage: %g\n", percent_used);
	}

	mp_perfdata pd = perfdata_init();
	pd.label = "swap";
	pd = mp_set_pd_value(pd, data.metrics.free);
	pd.uom = "B";

	if (config.warn_is_set) {
		uint64_t warn_print = config.warn.value;
		if (config.warn.is_percentage) {
			warn_print = config.warn.value * (data.metrics.total / HUNDRED_PERCENT);
		}

		mp_perfdata_value warn_pd = mp_create_pd_value(warn_print);

		mp_range warn_range = mp_range_init();
		warn_range.end_infinity = false;
		warn_range.end = warn_pd;

		pd.warn = warn_range;
		pd.warn_present = true;
	}

	if (config.crit_is_set) {
		uint64_t crit_print = config.crit.value;
		if (config.crit.is_percentage) {
			crit_print = config.crit.value * (data.metrics.total / HUNDRED_PERCENT);
		}

		mp_perfdata_value crit_pd = mp_create_pd_value(crit_print);

		mp_range crit_range = mp_range_init();
		crit_range.end_infinity = false;
		crit_range.end = crit_pd;

		pd.crit = crit_range;
		pd.crit_present = true;
	}

	mp_perfdata_value max = mp_create_pd_value(data.metrics.total);
	pd.max = max;
	pd.max_present = true;

	mp_perfdata_value min = mp_create_pd_value(0);
	pd.min = min;
	pd.min_present = true;

	mp_add_perfdata_to_subcheck(&sc1, pd);
	if (verbose > 1) {
		printf("Warn threshold value: %" PRIu64 "\n", config.warn.value);
	}

	if (config.warn_is_set) {
		if ((config.warn.is_percentage && (percent_used >= (100 - (double)config.warn.value))) || config.warn.value >= data.metrics.free) {
			sc1 = mp_set_subcheck_state(sc1, STATE_WARNING);
		}
	}

	if (verbose > 1) {
		printf("Crit threshold value: %" PRIu64 "\n", config.crit.value);
	}

	if (config.crit_is_set) {
		if ((config.crit.is_percentage && (percent_used >= (100 - (double)config.crit.value))) || config.crit.value >= data.metrics.free) {
			sc1 = mp_set_subcheck_state(sc1, STATE_CRITICAL);
		}
	}

	xasprintf(&sc1.output, _("%g%% free (%lluMiB out of %lluMiB)"), (100 - percent_used), data.metrics.free >> 20,
			  data.metrics.total >> 20);

	overall.summary = "Swap";
	mp_add_subcheck_to_check(&overall, sc1);

	mp_exit(overall);
}

int check_swap(float free_swap_mb, float total_swap_mb, swap_config config) {
	if (total_swap_mb == 0) {
		return config.no_swap_state;
	}

	uint64_t free_swap = (uint64_t)(free_swap_mb * (1024 * 1024)); /* Convert back to bytes as warn and crit specified in bytes */

	if (!config.crit.is_percentage && config.crit.value >= free_swap) {
		return STATE_CRITICAL;
	}
	if (!config.warn.is_percentage && config.warn.value >= free_swap) {
		return STATE_WARNING;
	}

	uint64_t usage_percentage = (uint64_t)((total_swap_mb - free_swap_mb) / total_swap_mb) * HUNDRED_PERCENT;

	if (config.crit.is_percentage && config.crit.value != 0 && usage_percentage >= (HUNDRED_PERCENT - config.crit.value)) {
		return STATE_CRITICAL;
	}

	if (config.warn.is_percentage && config.warn.value != 0 && usage_percentage >= (HUNDRED_PERCENT - config.warn.value)) {
		return STATE_WARNING;
	}

	return STATE_OK;
}

#define output_format_index CHAR_MAX + 1

/* process command-line arguments */
swap_config_wrapper process_arguments(int argc, char **argv) {
	swap_config_wrapper conf_wrapper = {.errorcode = OK};
	conf_wrapper.config = swap_config_init();

	static struct option longopts[] = {{"warning", required_argument, 0, 'w'},
									   {"critical", required_argument, 0, 'c'},
									   {"allswaps", no_argument, 0, 'a'},
									   {"no-swap", required_argument, 0, 'n'},
									   {"verbose", no_argument, 0, 'v'},
									   {"version", no_argument, 0, 'V'},
									   {"help", no_argument, 0, 'h'},
									   {"output-format", required_argument, 0, output_format_index},
									   {0, 0, 0, 0}};

	while (true) {
		int option = 0;
		int option_char = getopt_long(argc, argv, "+?Vvhac:w:n:", longopts, &option);

		if (option_char == -1 || option_char == EOF) {
			break;
		}

		switch (option_char) {
		case 'w': /* warning size threshold */
		{
			/*
			 * We expect either a positive integer value without a unit, which
			 * means the unit is Bytes or a positive integer value and a
			 * percentage sign (%), which means the value must be with 0 and 100
			 * and is relative to the total swap
			 */
			size_t length;
			length = strlen(optarg);
			conf_wrapper.config.warn_is_set = true;

			if (optarg[length - 1] == '%') {
				/* It's percentage */
				conf_wrapper.config.warn.is_percentage = true;
				optarg[length - 1] = '\0';
				if (is_uint64(optarg, &conf_wrapper.config.warn.value)) {
					if (conf_wrapper.config.warn.value > HUNDRED_PERCENT) {
						usage4(_("Warning threshold percentage must be <= 100!"));
					}
				}
				break;
			} /* It's Bytes */
			conf_wrapper.config.warn.is_percentage = false;
			if (is_uint64(optarg, &conf_wrapper.config.warn.value)) {
				break;
			}
			usage4(_("Warning threshold be positive integer or "
					 "percentage!"));
		}
		case 'c': /* critical size threshold */
		{
			/*
			 * We expect either a positive integer value without a unit, which
			 * means the unit is Bytes or a positive integer value and a
			 * percentage sign (%), which means the value must be with 0 and 100
			 * and is relative to the total swap
			 */
			size_t length;
			length = strlen(optarg);
			conf_wrapper.config.crit_is_set = true;

			if (optarg[length - 1] == '%') {
				/* It's percentage */
				conf_wrapper.config.crit.is_percentage = true;
				optarg[length - 1] = '\0';
				if (is_uint64(optarg, &conf_wrapper.config.crit.value)) {
					if (conf_wrapper.config.crit.value > HUNDRED_PERCENT) {
						usage4(_("Critical threshold percentage must be <= 100!"));
					}
				}
				break;
			} /* It's Bytes */
			conf_wrapper.config.crit.is_percentage = false;
			if (is_uint64(optarg, &conf_wrapper.config.crit.value)) {
				break;
			}
			usage4(_("Critical threshold be positive integer or "
					 "percentage!"));
		}
		case 'a': /* all swap */
			conf_wrapper.config.allswaps = true;
			break;
		case 'n':
			if ((conf_wrapper.config.no_swap_state = mp_translate_state(optarg)) == ERROR) {
				usage4(_("no-swap result must be a valid state name (OK, "
						 "WARNING, CRITICAL, UNKNOWN) or integer (0-3)."));
			}
			break;
		case 'v': /* verbose */
			verbose++;
			break;
		case output_format_index: {
			parsed_output_format parser = mp_parse_output_format(optarg);
			if (!parser.parsing_success) {
				// TODO List all available formats here, maybe add anothoer usage function
				printf("Invalid output format: %s\n", optarg);
				exit(STATE_UNKNOWN);
			}

			conf_wrapper.config.output_format_is_set = true;
			conf_wrapper.config.output_format = parser.output_format;
			break;
		}
		case 'V': /* version */
			print_revision(progname, NP_VERSION);
			exit(STATE_UNKNOWN);
		case 'h': /* help */
			print_help(conf_wrapper.config);
			exit(STATE_UNKNOWN);
		case '?': /* error */
			usage5();
		}
	}

	if ((conf_wrapper.config.warn.is_percentage == conf_wrapper.config.crit.is_percentage) &&
		(conf_wrapper.config.warn.value < conf_wrapper.config.crit.value)) {
		/* This is NOT triggered if warn and crit are different units, e.g warn
		 * is percentage and crit is absolute. We cannot determine the condition
		 * at this point since we dont know the value of total swap yet
		 */
		usage4(_("Warning should be more than critical"));
	}

	return conf_wrapper;
}

void print_help(swap_config config) {
	print_revision(progname, NP_VERSION);

	printf(_(COPYRIGHT), copyright, email);

	printf("%s\n", _("Check swap space on local machine."));

	printf("\n\n");

	print_usage();

	printf(UT_HELP_VRSN);
	printf(UT_EXTRA_OPTS);

	printf(" %s\n", "-w, --warning=INTEGER");
	printf("    %s\n", _("Exit with WARNING status if less than INTEGER bytes "
						 "of swap space are free"));
	printf(" %s\n", "-w, --warning=PERCENT%");
	printf("    %s\n", _("Exit with WARNING status if less than PERCENT of "
						 "swap space is free"));
	printf(" %s\n", "-c, --critical=INTEGER");
	printf("    %s\n", _("Exit with CRITICAL status if less than INTEGER bytes "
						 "of swap space are free"));
	printf(" %s\n", "-c, --critical=PERCENT%");
	printf("    %s\n", _("Exit with CRITICAL status if less than PERCENT of "
						 "swap space is free"));
	printf(" %s\n", "-a, --allswaps");
	printf("    %s\n", _("Conduct comparisons for all swap partitions, one by one"));
	printf(" %s\n", "-n, --no-swap=<ok|warning|critical|unknown>");
	printf("    %s %s\n",
		   _("Resulting state when there is no swap regardless of thresholds. "
			 "Default:"),
		   state_text(config.no_swap_state));
	printf(UT_OUTPUT_FORMAT);
	printf(UT_VERBOSE);

	printf("\n");
	printf("%s\n", _("Notes:"));
	printf(" %s\n", _("Both INTEGER and PERCENT thresholds can be specified, "
					  "they are all checked."));
	printf(" %s\n", _("On AIX, if -a is specified, uses lsps -a, otherwise uses lsps -s."));

	printf(UT_SUPPORT);
}

void print_usage(void) {
	printf("%s\n", _("Usage:"));
	printf(" %s [-av] -w <percent_free>%% -c <percent_free>%%\n", progname);
	printf("  -w <bytes_free> -c <bytes_free> [-n <state>]\n");
}
