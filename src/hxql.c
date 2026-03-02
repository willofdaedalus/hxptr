#include "hxql.h"
#include <ctype.h>
#include <string.h>

static int get_token(const char *str, char *out_tok, uint8_t outlen)
{
	int i = 0;
	int j = 0;

	if (!str || !out_tok) {
		return -1;
	}

	// skip whitespace
	while (str[i] && isspace((unsigned char) str[i])) {
		i++;
	}

	if (!str[i]) {
		return 0;
	}

	// token copy
	while (str[i] && !isspace((unsigned char) str[i])) {
		if (j < outlen - 1) {
			out_tok[j++] = str[i++];
			continue;
		}
		return -1;
	}
	out_tok[j] = '\0';

	return i;
}


HxptrResult_t hxql_query(hxptr_t *core, const char *query)
{
	if (!core || !query) {
		return HX_ERR_NULL;
	}

	if (strlen(query) == 0) {
		return HX_OK;
	}

	return HX_OK;
}
