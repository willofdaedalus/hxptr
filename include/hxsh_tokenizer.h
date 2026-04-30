#ifndef HXSH_TOKENIZER_H
#define HXSH_TOKENIZER_H

#include "hxsh.h"

HxshTokenType_t classify_token(const char *tok, token_map_t *inputs, int inputs_len);
int resolve_base(hxsh_t *sh, HxshTokenType_t type, const char *raw, uint32_t *out);
int resolve_num(HxshTokenType_t type, const char *raw, uint32_t *out);
int get_token(char *input, char *out_tok, uint8_t toklen);


#endif
