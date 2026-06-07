#include "hxsh.h"
#include "hxql.h"
#include "hxsh_cmds.h"
#include "hxsh_render.h"
#include "hxsh_table.h"
#include "hxsh_tokenizer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
			} else {
				// buffer full; drain remaining input on this line silently
				while (c != '\n' && c != '\r') {
					if (read(STDIN_FILENO, &c, 1) <= 0) {
						break;
					}
				}
				buf[len] = '\0';
				printf("\n");
				// return a distinct value so the repl can warn the user
				return 2;
			}
		}
	}
}

hxptr_error_t hxsh_repl(hxsh_t *sh)
{
	const hxsh_cmd_t *cmd = NULL;
	hxsh_req_t req = { 0 };
	hxptr_error_t err = { 0 };
	hxql_result_t result = { 0 };

	int eof = hxsh_readline_raw(sh->cmd, sizeof(sh->cmd));
	if (eof == 2) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "input too long; command truncated and ignored");
	}
	if (eof == 1) {
		sh->running = 0;
		return hxptr_ok();
	}
	if (sh->cmd[0] == '\0') {
		return hxptr_ok();
	}

	char *p = sh->cmd;
	char input[32];
	int len = get_token(p, input, sizeof(input));
	p += len;

	for (size_t i = 0; i < cmd_table_count; i++) {
		if (strcmp(input, cmd_table[i].name) == 0) {
			cmd = &cmd_table[i];
			break;
		}
	}

	if (!cmd) {
		return hxptr_err(
			HX_ERR_UNKNOWN_CMD, "unknown command '%s'. try 'help'", input);
	}

	while (isspace((unsigned char) *p)) {
		p++;
	}

	// quit is the only handler-less command
	if (cmd->type == TOK_QUIT) {
		sh->running = 0;
		return hxptr_ok();
	}

	req.type = cmd->type;
	err = cmd->handler(sh, p, cmd->shell_only ? NULL : &req.req_type);
	if (err.code != HX_OK) {
		return err;
	}

	if (cmd->shell_only) {
		return hxptr_ok();
	}

	err = hxql_execute_cmd(sh, &req, &result);
	if (err.code != HX_OK) {
		return err;
	}
	render(&result, sh);

	return hxptr_ok();
}

static int make_raw(hxsh_t *hx)
{
	struct termios raw;

	if (tcgetattr(STDIN_FILENO, &hx->orig_termios) == -1) {
		return -1;
	}

	raw = hx->orig_termios;

	raw.c_lflag &= ~(ICANON | ECHO | IEXTEN); // char mode, no echo
	// ISIG is left set to allow ctrl+c/ctrl+z

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

static inline int mark_alloc(hxsh_t *sh)
{
	// check if all bits set; essentially all spaces are occupied
	if (!sh || sh->mark_used == UINT32_MAX) {
		return -1;
	}

	for (uint32_t i = 0; i < HXSH_MARKS_MAX; i++) {
		if (!(sh->mark_used & (1U << i))) {
			sh->mark_used |= (1U << i);
			return (int) i;
		}
	}

	return -1;
}

hxptr_error_t hxsh_mark_add(hxsh_t *sh, const hxsh_mark_t *mark)
{
	if (!sh || !mark) {
		return hxptr_err(HX_ERR_NULL, "mark_add(): NULL argument");
	}

	// reject duplicate names
	uint32_t used = sh->mark_used;
	while (used) {
		uint32_t idx = __builtin_ctz(used);
		if (strcmp(sh->marks[idx].name, mark->name) == 0) {
			return hxptr_err(HX_ERR_MARKS_DUP,
				"'%s' already exists; remove it first or pick a different name",
				mark->name);
		}
		used &= used - 1;
	}

	int idx = mark_alloc(sh);
	if (idx < 0) {
		return hxptr_err(HX_ERR_MARKS_FULL,
			"mark: reached total marks tracked of %d; remove a mark and try again",
			HXSH_MARKS_MAX);
	}
	memcpy(&sh->marks[idx], mark, sizeof(hxsh_mark_t));
	printf("added new mark '%s' for address '%#08x'\n", sh->marks[idx].name,
		sh->marks[idx].addr);

	return hxptr_ok();
}

hxptr_error_t hxsh_mark_rm(hxsh_t *sh, const char *mark)
{
	if (!sh) {
		return hxptr_err(HX_ERR_NULL, "mark_rm: sh is NULL");
	}
	if (!mark) {
		return hxptr_err(HX_ERR_NULL, "mark_rm: mark is NULL");
	}

	uint32_t used = sh->mark_used;

	while (used) {
		uint32_t idx = __builtin_ctz(used);

		if (strcmp(sh->marks[idx].name, mark) == 0) {
			sh->mark_used &= ~(1U << idx);
			sh->marks[idx] = (hxsh_mark_t) { 0 };

			return hxptr_ok();
		}

		used &= used - 1;
	}

	return hxptr_err(HX_ERR_MARKS_404, "mark '%s' not found", mark);
}

hxptr_error_t hxsh_mark_find(hxsh_t *sh, const char *mark, uint32_t *out_addr)
{
	if (!sh) {
		return hxptr_err(HX_ERR_NULL, "mark_rm: sh is NULL");
	}
	if (!mark) {
		return hxptr_err(HX_ERR_NULL, "mark_rm: mark is NULL");
	}

	uint32_t used = sh->mark_used;

	while (used) {
		uint32_t idx = __builtin_ctz(used);
		if (strcmp(sh->marks[idx].name, mark) == 0) {
			*out_addr = sh->marks[idx].addr;
			return hxptr_ok();
		}
		used &= used - 1;
	}

	return hxptr_err(HX_ERR_MARKS_404, "mark '%s' not found", mark);
}

hxptr_error_t hxsh_mark_list(hxsh_t *sh)
{
	if (!sh) {
		return hxptr_err(HX_ERR_NULL, "mark_list: sh is NULL");
	}

	hxsh_table_t t;
	uint32_t used = sh->mark_used;
	if (!used) {
		printf("no marks set\n");
		return hxptr_ok();
	}

	table_init(&t);

	const char *headers[] = { "name", "address" };
	table_set_headers(&t, headers, 2);

	uint8_t name_w = strlen("name");
	for (uint8_t i = 0; i < HXSH_MARKS_MAX; i++) {
		if (!(sh->mark_used & (1U << i))) {
			continue;
		}

		uint8_t nw = strlen(sh->marks[i].name);
		if (nw > name_w) {
			name_w = nw;
		}
	}

	const uint8_t widths[] = { name_w, 10 };
	table_set_widths(&t, widths, 2);
	table_print_header(&t, "marks");

	while (used) {
		uint32_t idx = __builtin_ctz(used);
		char addr[12];
		snprintf(addr, sizeof(addr), "0x%08x", sh->marks[idx].addr);
		const char *row[] = { sh->marks[idx].name, addr };
		table_print_row(&t, row);
		used &= used - 1;
	}

	table_print_separator(&t);

	return hxptr_ok();
}
