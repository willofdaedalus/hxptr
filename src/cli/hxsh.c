#include "hxsh.h"
#include "hxql.h"
#include "hxsh_handlers.h"
#include "hxsh_render.h"
#include "hxsh_tokenizer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// out_tok map
static const token_map_t token_map[] = {
	{ "info",  TOK_INFO  },
	{ "find",  TOK_FIND  },
	{ "goto",  TOK_GOTO  },
	{ "which", TOK_WHICH },
	{ "list",  TOK_LIST  },
	{ "help",  TOK_HELP  },
	{ "quit",  TOK_QUIT  },
	{ "load",  TOK_LOAD  },
};
static const size_t map_count = sizeof(token_map) / sizeof(token_map[0]);


hxptr_error_t hxsh_repl(hxsh_t *sh, FILE *input)
{
	char cmd[32];
	int len = 0;
	hxsh_req_t req = { 0 };
	HxshTokenType_t op = TOK_UNKNOWN;
	hxptr_error_t err = { 0 };
	hxql_result_t result = { 0 };

	// TODO; remember to flush the stdin after every command runs
	printf(PROMPT);
	if (fgets(sh->cmd, sizeof(sh->cmd), input) == NULL) {
		sh->running = 0;
		return hxptr_ok();
	}
	sh->cmd[strcspn(sh->cmd, "\n")] = '\0';

	if (sh->cmd[0] == '\0') {
		return hxptr_ok();
	}

	char *p = sh->cmd;

	// get the initial token which is always considered the
	// command/operation
	len = get_token(p, cmd, sizeof(cmd));
	p += len;

	for (size_t i = 0; i < map_count; i++) {
		if (strcmp(cmd, token_map[i].name) == 0) {
			op = token_map[i].type;
		}
	}
	if (op == TOK_UNKNOWN) {
		return hxptr_err(
			HX_ERR_UNKNOWN_CMD, "unknown command '%s'. try 'help'", cmd);
	}

	// trim leading whitespace from remaining args
	while (isspace((unsigned char) *p)) {
		p++;
	}

	req.type = op;
	switch (req.type) {
		case TOK_INFO:
			err = info_handler(sh, p, &req.req_type.info_req);
			break;
		case TOK_LIST:
			err = list_handler(sh, p, &req.req_type.list_req);
			break;
		case TOK_WHICH:
			err = which_handler(sh, p, &req.req_type.which_req);
			break;
		case TOK_FIND:
			err = find_handler(sh, p, &req.req_type.search_req);
			break;
		case TOK_GOTO:
			err = goto_handler(sh, p, &req.req_type.goto_req);
			break;
		case TOK_QUIT:
			sh->running = 0;
			goto quit_handler;
			break;
		case TOK_LOAD:
			err = load_handler(sh, p, NULL);
			goto early_exit;
			break;
		case TOK_HELP:
			break;
	}
	if (err.code != HX_OK) {
		goto early_exit;
	}

	err = hxql_execute_cmd(sh, &req, &result);
	if (err.code != HX_OK) {
		return err;
	}

	render(&result, sh);

quit_handler:
	return hxptr_ok();

early_exit:
	return err;
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
	hx->cur_entry = 0;
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
