#include "hxptr.h"
#include <stdio.h>

int main(void)
{
	HxptrResult_t res;
	hxptr_record_t record = {0};
	char *hex = ":0b0010006164647265737320676170a7";

	if ((res = hxptr_hex_to_record(hex, &record)) != HX_OK) {
		printf("%d\n", res);
		return -1;
	}

	record_to_str(&record);

	return 0;
}
