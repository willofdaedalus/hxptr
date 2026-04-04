#include "hxsh.h"
#include "hxql.h"
#include "hxsh_table.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hxptr_error_t info_handler(const char *args, void *out_data);
static hxptr_error_t list_handler(const char *args, void *out_data);

// out_tok map
static const token_map_t token_map[] = {
	{ "info",    TOK_INFO    },
	{ "search",  TOK_SEARCH  },
	{ "goto",    TOK_GOTO    },
	{ "frame",   TOK_FRAME   },
	{ "list",    TOK_LIST    },
	{ "help",    TOK_HELP    },
	{ "quit",    TOK_QUIT    },
	// args
	{ "regions", TOK_REGIONS },
	{ "memory",  TOK_MEMORY  },
	{ "stats",   TOK_STATS   },
	{ "diag",    TOK_DIAG    },
	{ "profile", TOK_PROFILE },
};
static const size_t token_map_count = sizeof(token_map) / sizeof(token_map[0]);

static int classify_token(const char *tok, token_map_t *inputs, int inputs_len)
{
	for (uint8_t i = 0; i < inputs_len; i++) {
		if (strcmp(tok, inputs[i].name) == 0) {
			return inputs[i].type;
		}
	}

	if (strcmp(tok, ".") == 0) {
		return TOK_DOT;
	}

	if (isxdigit((unsigned char) tok[0])) {
		int colon = 0;
		int i = 0;

		while (tok[i] && (isxdigit((unsigned char) tok[i]) || tok[i] == ':')) {
			if (tok[i] == ':') {
				if (colon) {
					// this is an error
					return TOK_UNKNOWN;
				}
				colon = 1;
			}
			i++;
		}

		if (!isxdigit((unsigned char) tok[i])) {
			return TOK_UNKNOWN;
		}

		return colon ? TOK_RANGE : TOK_HEX;
	}

	if (tok[0] == '+' || tok[0] == '-') {
		return TOK_NUMBER;
	}

	return TOK_UNKNOWN;
}

static int get_token(char *input, char *out_tok, uint8_t toklen)
{
	uint8_t i = 0, j = 0;

	if (!input) {
		return -1;
	}

	while (input[i] && isspace((unsigned char) input[i])) {
		i++;
	}

	if (!input[i]) {
		return 0;
	}

	while (input[i] && !isspace((unsigned char) input[i])) {
		if (j < toklen - 1) {
			out_tok[j++] = input[i++];
		} else {
			return -1;
		}
	}

	out_tok[j] = '\0';
	return i;
}

static void profile_render(mcu_profile_t *profile, HxshTokenType_t type)
{
	uint8_t i = 0;
	profile_region_t region;
	profile_memory_t mem;
	hxsh_table_t t;

	if (!profile) {
		// do something
		fprintf(stderr, "you shouldn't be here...\n");
		return;
	}

	table_init(&t);
	if (type == TOK_MEMORY) {
		char s_start[16], s_size[16];

		const char *headers[] = { "name", "start", "size" };
		table_set_headers(&t, headers, 3);

		for (uint8_t i = 0; i < profile->mem_count; i++) {
			mem = profile->mem_blocks[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", mem.start);
			snprintf(s_size, sizeof(s_size), "%u", mem.size);
			const char *row[] = { mem.type, s_start, s_size };
			table_add_row(&t, row, 3);
		}

		table_render(&t, "memory info");

	} else if (type == TOK_REGIONS) {
		char s_start[16], s_end[16];

		const char *headers[] = { "name", "start", "end", "protected" };
		table_set_headers(&t, headers, 4);

		for (uint8_t i = 0; i < profile->region_count; i++) {
			region = profile->regions[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", region.start);
			snprintf(s_end, sizeof(s_end), "0x%08x", region.end);
			const char *row[] = { region.name, s_start, s_end,
				region.protected ? "yes" : "no" };
			table_add_row(&t, row, 4);
		}

		table_render(&t, "regions info");
	} else if (type == TOK_PROFILE) {
		const char *headers[] = { "name", "architecture", "vendor" };
		const char *row[] = { profile->name, profile->arch, profile->vendor };

		table_set_headers(&t, headers, 3);
		table_add_row(&t, row, 4);
		table_render(&t, "profile info");
	} else {
		printf("you're not allowed here!\n");
	}
}

static void stats_render(hxql_result_t *result)
{
	hxptr_stats_t *s = result->data.info.stats;

	printf("\nstats\n");
	printf("  %-20s %u\n", "total bytes", s->total_bytes);
	printf("  %-20s %u\n", "total records", s->total_records);
	printf("  %-20s %u\n", "total overlaps", s->total_overlaps);
	printf("  %-20s %u\n", "total gaps", s->total_gaps);
	printf("  %-20s %u bytes\n", "largest gap", s->largest_gap);
	printf("  %-20s 0x%08x\n", "lowest address", s->addr_lowest);
	printf("  %-20s 0x%08x\n", "highest address", s->addr_highest);
}

void render(hxql_result_t *result)
{
	info_result_t res;
	if (!result) {
		return;
	}

	switch (result->type) {
		case HXQL_RESULT_INFO:
			res = result->data.info;

			if (res.args & INFO_PROF_MASK) {
				if (!res.profile) {
					fprintf(stderr,
						"no mcu profile loaded for arguments 'memory', "
						"'regions' and 'profile'\n");
				} else {
					if (res.args & TOK_MEMORY) {
						profile_render(res.profile, TOK_MEMORY);
					}
					if (res.args & TOK_REGIONS) {
						profile_render(res.profile, TOK_REGIONS);
					}
					if (res.args & TOK_PROFILE) {
						profile_render(res.profile, TOK_PROFILE);
					}
				}
			}

			if (res.args & TOK_STATS) {
				printf("\nstats info\n");
				stats_render(result);
			}

			break;

			// case HXQL_RESULT_ENTRIES: {
			// 	memory_entry_t *entries = result->data.list.entries;
			// 	uint32_t count = result->data.list.count;
			// 	for (uint32_t i = 0; i < count; i++) {
			// 		if (!entries[i].is_data_record) {
			// 			continue;
			// 		}
			// 		printf("[0x%08x] ", entries[i].address);
			// 		for (uint8_t j = 0; j < entries[i].byte_count; j++) {
			// 			printf("%02x ", entries[i].bytes[j]);
			// 		}
			// 		printf("\n");
			// 	}
			// 	break;
			// }

			// case HXQL_RESULT_STATUS:
			// 	if (result->data.status.msg[0]) {
			// 		printf("%s\n", result->data.status.msg);
			// 	}
			// 	break;

		default:
			fprintf(stderr, "nothing to see here... yet\n");
			break;
	}
	putchar('\n');
}

hxptr_error_t hxsh_repl(hxsh_t *sh)
{
	char cmd[32];
	int len = 0;
	hxsh_req_t req = { 0 };
	HxshTokenType_t op = TOK_UNKNOWN;
	hxptr_error_t err = { 0 };
	hxql_result_t result = { 0 };

	printf(PROMPT);
	if (fgets(sh->cmd, sizeof(sh->cmd), stdin) == NULL) {
		sh->running = 0;
		return hxptr_ok();
	}
	sh->cmd[strcspn(sh->cmd, "\n")] = '\0';

	if (sh->cmd[0] == '\0') {
		return hxptr_ok();
	}

	char *p = sh->cmd;

	// get the initial token which is always considered the command/operation
	len = get_token(p, cmd, sizeof(cmd));
	p += len;

	for (size_t i = 0; i < token_map_count; i++) {
		if (strcmp(cmd, token_map[i].name) == 0) {
			op = token_map[i].type;
		}
	}
	if (op == TOK_UNKNOWN) {
		return hxptr_err(
			HX_ERR_UNKNOWN_CMD, "unknown command '%s'. try 'help'", cmd);
	}

	req.type = op;
	switch (req.type) {
		case TOK_INFO:
			err = info_handler(p, &req.req.info_req);
			break;
		case TOK_LIST:
			err = list_handler(p, &req.req.list_req);
	}
	if (err.code != HX_OK) {
		return err;
	}

	err = hxql_execute_cmd(sh, &req, &result);
	if (err.code != HX_OK) {
		return err;
	}

	render(&result);

	return hxptr_ok();
}

// handles the info command
// args: the optional args passed via the shell for the command
// out_data: the data that will be passed to the executor
static hxptr_error_t info_handler(const char *args, void *out_data)
{
	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: info_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	char *list = (char *) args;
	int len;
	HxshTokenType_t tok_type;
	char arg[64];
	info_req_t req = { 0 };
	uint8_t count;

	token_map_t sub_cmds[] = {
		{ "regions", TOK_REGIONS },
		{ "memory",  TOK_MEMORY  },
		{ "stats",   TOK_STATS   },
		{ "diag",    TOK_DIAG    },
		{ "profile", TOK_PROFILE },
	};
	count = sizeof(sub_cmds) / sizeof(sub_cmds[0]);

	if (!args || !*args) {
		// no arguments: request everything
		req.args =
			TOK_REGIONS | TOK_MEMORY | TOK_STATS | TOK_DIAG | TOK_PROFILE;
	} else {
		while ((len = get_token(list, arg, sizeof(arg))) > 0) {
			tok_type = classify_token(arg, sub_cmds, count);

			switch (tok_type) {
				case TOK_REGIONS:
				case TOK_MEMORY:
				case TOK_STATS:
				case TOK_DIAG:
				case TOK_PROFILE:
					req.args |= tok_type;
					break;

				default:
					return hxptr_err(HX_ERR_UNKN_ARG,
						"unknown argument '%s' for info command", arg);
			}

			list += len;
		}

		// if get_token returned < 0, it's a parsing error
		if (len < 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "invalid argument for info command");
		}
	}

	// copy the data only if parsing succeeded
	memcpy(out_data, &req, sizeof(req));

	return hxptr_ok();
}

static hxptr_error_t list_handler(const char *args, void *out_data)
{
	if (!out_data) {
		return hxptr_err(
			HX_ERR_INVALID_ARG, "%s:%d: out_data is NULL", __FILE__, __LINE__);
	}

	if (!args) {
		// default behaviour
	}

	return hxptr_ok();
}

hxsh_t *hxsh_init(hxptr_t *core)
{
	hxsh_t *hx;
	if (!core) {
		return NULL;
	}

	hx = calloc(1, sizeof(hxsh_t));
	if (!hx) {
		return NULL;
	}

	hx->hx = core;
	hx->cur_idx = 0;
	hx->search_addr = 0;
	hx->running = 1;

	printf("\033[2J");
	printf("\033[H");
	printf("\033[?1049h");

	return hx;
}

void hxsh_free(hxsh_t *hx)
{
	if (!hx) {
		return;
	}

	printf("\033[?1049l");
	printf("\033[2J");
	printf("\033[H");
	free(hx);
}
