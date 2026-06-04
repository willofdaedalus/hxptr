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


static int hxsh_readline_raw(char *buf, size_t buf_size)
{
	size_t len = 0;
	printf(PROMPT);
	fflush(stdout);

	while (1) {
		char c;
		ssize_t n = read(STDIN_FILENO, &c, 1);
		if (n <= 0) {
			continue;
		}

		if (c == 0x04) {
			// ctrl+d
			return 1;
		} else if (c == 0x0c) {
			// ctrl+l
			printf("\033[H\033[2J");
			printf(PROMPT "%.*s", (int) len, buf);
			fflush(stdout);
		} else if (c == 0x7f || c == 0x08) {
			// backspace
			if (len > 0) {
				len--;
				printf("\b \b");
				fflush(stdout);
			}
		} else if (c == '\n' || c == '\r') {
			// enter
			buf[len] = '\0';
			printf("\n");
			return 0;
		} else if (c == 0x1b) {
			// escape sequence (arrow keys)
			char seq[2];
			if (read(STDIN_FILENO, &seq[0], 1) == 0) {
				continue;
			}
			if (read(STDIN_FILENO, &seq[1], 1) == 0) {
				continue;
			}

			if (seq[0] == '[') {
				switch (seq[1]) {
					case 'A':
						break; // TODO: history prev
					case 'B':
						break; // TODO: history next
					case 'C':
						break; // TODO: move cursor
					case 'D':
						break; // TODO: move cursor
				}
			}
		} else {
			if (len < buf_size - 1) {
				buf[len++] = c;
				write(STDOUT_FILENO, &c, 1);
			}
		}
	}
}

hxptr_error_t hxsh_repl(hxsh_t *sh)
{
	hxsh_req_t req = { 0 };
	HxshTokenType_t op = TOK_UNKNOWN;
	hxptr_error_t err = { 0 };
	hxql_result_t result = { 0 };

	int eof = hxsh_readline_raw(sh->cmd, sizeof(sh->cmd));
	if (eof) {
		sh->running = 0;
		return hxptr_ok();
	}

	if (sh->cmd[0] == '\0') {
		return hxptr_ok();
	}

	char *p = sh->cmd;
	char cmd[32];
	int len = get_token(p, cmd, sizeof(cmd));
	p += len;

	for (size_t i = 0; i < map_count; i++) {
		if (strcmp(cmd, token_map[i].name) == 0) {
			op = token_map[i].type;
			break;
		}
	}

	if (op == TOK_UNKNOWN) {
		return hxptr_err(
			HX_ERR_UNKNOWN_CMD, "unknown command '%s'. try 'help'", cmd);
	}

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
		case TOK_LOAD:
			err = load_handler(sh, p, NULL);
			goto early_exit;
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

static int make_raw(hxsh_t *hx)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &hx->orig_termios) == -1) {
		return -1;
	}

	raw = hx->orig_termios;

	raw.c_lflag &= ~(ICANON | ECHO | IEXTEN); // char mode, no echo
	// ISIG is left set to allow Ctrl+C/Ctrl+Z

	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	// raw.c_oflag &= ~(OPOST);

	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1) {
		return -1;
	}

	return 0;
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

	// set terminal to cbreak mode and back up original settings
	if (make_raw(hx) != 0) {
		free(hx);
		return NULL;
	}

	printf("\033[2J");
	printf("\033[H");
	// printf("\033[?1049h");
	fflush(stdout);

	return hx;
}

void hxsh_free(hxsh_t *hx)
{
	if (!hx) {
		return;
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &hx->orig_termios);
	// printf("\033[?1049l");
	printf("\033[2J");
	printf("\033[H");
	free(hx);
}
