/*****************************************************************************
 *
 * Monitoring negate plugin
 *
 * License: GPL
 * Copyright (c) 2002-2024 Monitoring Plugins Development Team
 *
 * Description:
 *
 * This file contains the negate plugin
 *
 * Negates the status of a plugin (returns OK for CRITICAL, and vice-versa).
 * Can also perform custom state switching.
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

const char *progname = "negate";
const char *copyright = "2002-2024";
const char *email = "devel@monitoring-plugins.org";

#define DEFAULT_TIMEOUT 11

#include "common.h"
#include "utils.h"
#include "utils_cmd.h"
#include "negate.d/config.h"
#include "../lib/states.h"

typedef struct {
	int errorcode;
	negate_config config;
} negate_config_wrapper;
static negate_config_wrapper process_arguments(int /*argc*/, char ** /*argv*/);
static negate_config_wrapper validate_arguments(negate_config_wrapper /*config_wrapper*/);

static void print_help(void);
void print_usage(void);

int main(int argc, char **argv) {
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	timeout_interval = DEFAULT_TIMEOUT;

	negate_config_wrapper tmp_config = process_arguments(argc, argv);

	if (tmp_config.errorcode == ERROR) {
		die(STATE_UNKNOWN, _("negate: Failed to parse input"));
	}

	negate_config config = tmp_config.config;

	char **command_line = config.command_line;

	/* Set signal handling and alarm */
	if (signal(SIGALRM, timeout_alarm_handler) == SIG_ERR) {
		die(STATE_UNKNOWN, _("Cannot catch SIGALRM"));
	}

	(void)alarm(timeout_interval);

	mp_state_enum result = STATE_UNKNOWN;
	output chld_out;
	output chld_err;

	/* catch when the command is quoted */
	if (command_line[1] == NULL) {
		result = cmd_run(command_line[0], &chld_out, &chld_err, 0);
	} else {
		result = cmd_run_array(command_line, &chld_out, &chld_err, 0);
	}
	if (chld_err.lines > 0) {
		for (size_t i = 0; i < chld_err.lines; i++) {
			fprintf(stderr, "%s\n", chld_err.line[i]);
		}
	}

	/* Return UNKNOWN or worse if no output is returned */
	if (chld_out.lines == 0) {
		die(max_state_alt(result, STATE_UNKNOWN), _("No data returned from command\n"));
	}

	char *sub;
	for (size_t i = 0; i < chld_out.lines; i++) {
		if (config.subst_text && result >= 0 && result <= 4 && result != config.state[result]) {
			/* Loop over each match found */
			while ((sub = strstr(chld_out.line[i], state_text(result)))) {
				/* Terminate the first part and skip over the string we'll substitute */
				*sub = '\0';
				sub += strlen(state_text(result));
				/* then put everything back together */
				xasprintf(&chld_out.line[i], "%s%s%s", chld_out.line[i], state_text(config.state[result]), sub);
			}
		}
		printf("%s\n", chld_out.line[i]);
	}

	if (result >= 0 && result <= 4) {
		exit(config.state[result]);
	} else {
		exit(result);
	}
}

/* process command-line arguments */
static negate_config_wrapper process_arguments(int argc, char **argv) {
	static struct option longopts[] = {{"help", no_argument, 0, 'h'},           {"version", no_argument, 0, 'V'},
									   {"timeout", required_argument, 0, 't'},  {"timeout-result", required_argument, 0, 'T'},
									   {"ok", required_argument, 0, 'o'},       {"warning", required_argument, 0, 'w'},
									   {"critical", required_argument, 0, 'c'}, {"unknown", required_argument, 0, 'u'},
									   {"substitute", no_argument, 0, 's'},     {0, 0, 0, 0}};

	negate_config_wrapper result = {
		.errorcode = OK,
		.config = negate_config_init(),
	};
	bool permute = true;
	while (true) {
		int option = 0;
		int option_char = getopt_long(argc, argv, "+hVt:T:o:w:c:u:s", longopts, &option);

		if (option_char == -1 || option_char == EOF) {
			break;
		}

		switch (option_char) {
		case '?': /* help */
			usage5();
			break;
		case 'h': /* help */
			print_help();
			exit(STATE_UNKNOWN);
			break;
		case 'V': /* version */
			print_revision(progname, NP_VERSION);
			exit(STATE_UNKNOWN);
		case 't': /* timeout period */
			if (!is_integer(optarg)) {
				usage2(_("Timeout interval must be a positive integer"), optarg);
			} else {
				timeout_interval = atoi(optarg);
			}
			break;
		case 'T': /* Result to return on timeouts */
			if ((timeout_state = mp_translate_state(optarg)) == ERROR) {
				usage4(_("Timeout result must be a valid state name (OK, WARNING, CRITICAL, UNKNOWN) or integer (0-3)."));
			}
			break;
		case 'o': /* replacement for OK */
			if ((result.config.state[STATE_OK] = mp_translate_state(optarg)) == ERROR) {
				usage4(_("Ok must be a valid state name (OK, WARNING, CRITICAL, UNKNOWN) or integer (0-3)."));
			}
			permute = false;
			break;

		case 'w': /* replacement for WARNING */
			if ((result.config.state[STATE_WARNING] = mp_translate_state(optarg)) == ERROR) {
				usage4(_("Warning must be a valid state name (OK, WARNING, CRITICAL, UNKNOWN) or integer (0-3)."));
			}
			permute = false;
			break;
		case 'c': /* replacement for CRITICAL */
			if ((result.config.state[STATE_CRITICAL] = mp_translate_state(optarg)) == ERROR) {
				usage4(_("Critical must be a valid state name (OK, WARNING, CRITICAL, UNKNOWN) or integer (0-3)."));
			}
			permute = false;
			break;
		case 'u': /* replacement for UNKNOWN */
			if ((result.config.state[STATE_UNKNOWN] = mp_translate_state(optarg)) == ERROR) {
				usage4(_("Unknown must be a valid state name (OK, WARNING, CRITICAL, UNKNOWN) or integer (0-3)."));
			}
			permute = false;
			break;
		case 's': /* Substitute status text */
			result.config.subst_text = true;
			break;
		}
	}

	if (permute) { /* No [owcu] switch specified, default to this */
		result.config.state[STATE_OK] = STATE_CRITICAL;
		result.config.state[STATE_CRITICAL] = STATE_OK;
	}

	result.config.command_line = &argv[optind];

	return validate_arguments(result);
}

negate_config_wrapper validate_arguments(negate_config_wrapper config_wrapper) {
	if (config_wrapper.config.command_line[0] == NULL) {
		usage4(_("Could not parse arguments"));
	}

	if (strncmp(config_wrapper.config.command_line[0], "/", 1) != 0 && strncmp(config_wrapper.config.command_line[0], "./", 2) != 0) {
		usage4(_("Require path to command"));
	}

	return config_wrapper;
}

void print_help(void) {
	print_revision(progname, NP_VERSION);

	printf(COPYRIGHT, copyright, email);

	printf("%s\n", _("Negates only the return code of a plugin (returns OK for CRITICAL and vice-versa) by default."));
	printf("%s\n", _("Additional switches can be used to control:\n"));
	printf("\t  - which state becomes what\n");
	printf("\t  - changing the plugin output text to match the return code");

	printf("\n\n");

	print_usage();

	printf(UT_HELP_VRSN);

	printf(UT_PLUG_TIMEOUT, timeout_interval);
	printf("    %s\n", _("Keep timeout longer than the plugin timeout to retain CRITICAL status."));
	printf(" -T, --timeout-result=STATUS\n");
	printf("    %s\n", _("Custom result on Negate timeouts; see below for STATUS definition\n"));

	printf(" -o, --ok=STATUS\n");
	printf(" -w, --warning=STATUS\n");
	printf(" -c, --critical=STATUS\n");
	printf(" -u, --unknown=STATUS\n");
	printf(_("    STATUS can be 'OK', 'WARNING', 'CRITICAL' or 'UNKNOWN' without single\n"));
	printf(_("    quotes. Numeric values are accepted. If nothing is specified, permutes\n"));
	printf(_("    OK and CRITICAL.\n"));
	printf(" -s, --substitute\n");
	printf(_("    Substitute output text as well. Will only substitute text in CAPITALS\n"));

	printf("\n");
	printf("%s\n", _("Examples:"));
	printf(" %s\n", "negate /usr/local/nagios/libexec/check_ping -H host");
	printf("    %s\n", _("Run check_ping and invert result. Must use full path to plugin"));
	printf(" %s\n", "negate -w OK -c UNKNOWN /usr/local/nagios/libexec/check_procs -a 'vi negate.c'");
	printf("    %s\n", _("This will return OK instead of WARNING and UNKNOWN instead of CRITICAL"));
	printf("\n");
	printf("%s\n", _("Notes:"));
	printf(" %s\n", _("This plugin is a wrapper to take the output of another plugin and invert it."));
	printf(" %s\n", _("The full path of the plugin must be provided."));
	printf(" %s\n", _("If the wrapped plugin returns OK, the wrapper will return CRITICAL."));
	printf(" %s\n", _("If the wrapped plugin returns CRITICAL, the wrapper will return OK."));
	printf(" %s\n", _("Otherwise, the output state of the wrapped plugin is unchanged."));
	printf("\n");
	printf(" %s\n", _("Using timeout-result, it is possible to override the timeout behaviour or a"));
	printf(" %s\n", _("plugin by setting the negate timeout a bit lower."));

	printf(UT_SUPPORT);
}

void print_usage(void) {
	printf("%s\n", _("Usage:"));
	printf("%s [-t timeout] [-Towcu STATE] [-s] <definition of wrapped plugin>\n", progname);
}
