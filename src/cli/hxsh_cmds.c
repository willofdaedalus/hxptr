#include "hxsh_cmds.h"
#include "hxsh_handlers.h"

const hxsh_cmd_t cmd_table[] = {
	{
     .name = "list",
     .type = TOK_LIST,
     .shell_only = 0,
     .handler = list_handler,
     .short_help = "list memory entries from current position",
     .long_help =
			"list [addr | addr : addr]\n"
			"\n"
			"  lists memory entries starting at addr.\n"
			"  with no args, lists from current position and advances the cursor.\n"
			"\n"
			"  address expressions:\n"
			"    .          current position\n"
			"    start      first entry in the hex map\n"
			"    end        last entry in the hex map\n"
			"    0x...      explicit hex address\n"
			"    region <n> start of named region (requires profile)\n"
			"    mark <n>   address of named bookmark\n"
			"    . +/- n    offset from current position\n"
			"\n"
			"  examples:\n"
			"    list\n"
			"    list 0x08000000\n"
			"    list start : end\n"
			"    list . : . + 10", },
	{
     .name = "goto",
     .type = TOK_GOTO,
     .shell_only = 0,
     .handler = goto_handler,
     .short_help = "move current position to an address",
     .long_help = "goto <addr>\n"
					 "\n"
					 "  moves the current position to the entry at addr.\n"
					 "  ranges are not valid; pass a single address.\n"
					 "\n"
					 "  address expressions:\n"
					 "    .          current position (no-op)\n"
					 "    start      first entry in the hex map\n"
					 "    end        last entry in the hex map\n"
					 "    0x...      explicit hex address\n"
					 "    region <n> start of named region (requires profile)\n"
					 "    mark <n>   address of named bookmark\n"
					 "    . +/- n    offset from current position\n"
					 "\n"
					 "  examples:\n"
					 "    goto 0x08000000\n"
					 "    goto start\n"
					 "    goto region bootloader\n"
					 "    goto mark reset\n"
					 "    goto . + 5", },
	{
     .name = "find",
     .type = TOK_FIND,
     .shell_only = 0,
     .handler = find_handler,
     .short_help = "search for a byte pattern across memory entries",
     .long_help =
			"find <pattern> [addr | addr : addr]\n"
			"find next\n"
			"find prev\n"
			"\n"
			"  searches for a hex byte pattern in the hex map.\n"
			"  an optional address expression constrains the search range.\n"
			"  with no range, searches the entire map from current position.\n"
			"\n"
			"  'find next' and 'find prev' continue an active search forward\n"
			"  or backward. search wraps around the range boundary.\n"
			"\n"
			"  pattern format: hex digits, with or without 0x prefix.\n"
			"    must be an even number of hex digits.\n"
			"\n"
			"  examples:\n"
			"    find 0xdeadbeef\n"
			"    find b508\n"
			"    find b508 0x08000000 : 0x08010000\n"
			"    find next\n"
			"    find prev", },
	{
     .name = "info",
     .type = TOK_INFO,
     .shell_only = 0,
     .handler = info_handler,
     .short_help = "display file, profile, and diagnostic information",
     .long_help =
			"info [stats] [profile] [regions] [memory] [diag]\n"
			"\n"
			"  displays information about the loaded hex file and profile.\n"
			"  with no args, displays everything available.\n"
			"\n"
			"  subcommands:\n"
			"    stats    total bytes, records, gaps, overlaps, address range\n"
			"    profile  loaded mcu profile name and architecture\n"
			"    regions  named regions declared in the profile\n"
			"    memory   memory blocks declared in the profile\n"
			"    diag     gaps and overlaps detected during analysis\n"
			"\n"
			"  examples:\n"
			"    info\n"
			"    info stats\n"
			"    info regions diag", },
	{
     .name = "which",
     .type = TOK_WHICH,
     .shell_only = 0,
     .handler = which_handler,
     .short_help = "show which region or memory block contains current address",
     .long_help = "which [addr] [region] [memory]\n"
					 "\n"
					 "  reports which profile region and/or memory block contains\n"
					 "  the current cursor address. requires a loaded profile.\n"
					 "\n"
					 "  subcommands:\n"
					 "    addr    show the current address\n"
					 "    region  show which named region contains the address\n"
					 "    memory  show which memory block contains the address\n"
					 "\n"
					 "  with no args, reports all three.\n"
					 "\n"
					 "  examples:\n"
					 "    which\n"
					 "    which region\n"
					 "    which addr memory", },
	{
     .name = "mark",
     .type = TOK_MARK,
     .shell_only = 1,
     .handler = mark_handler,
     .short_help = "manage named address bookmarks",
     .long_help =
			"mark add <name> [addr]\n"
			"mark rm <name>\n"
			"mark list\n"
			"\n"
			"  bookmarks store a named address for quick navigation.\n"
			"  once set, marks can be used as address expressions in\n"
			"  other commands via 'mark <name>'.\n"
			"\n"
			"  subcommands:\n"
			"    add <name> [addr]  bookmark addr (default: current position)\n"
			"    rm <name>          remove a bookmark by name\n"
			"    list               list all bookmarks\n"
			"\n"
			"  limits: up to 32 bookmarks per session.\n"
			"\n"
			"  examples:\n"
			"    mark add reset 0x08000000\n"
			"    mark add here\n"
			"    mark rm reset\n"
			"    mark list\n"
			"    goto mark reset", },
	{
     .name = "load",
     .type = TOK_LOAD,
     .shell_only = 1,
     .handler = load_handler,
     .short_help = "load a hex file or profile at runtime",
     .long_help = "load file <path>\n"
					 "load profile <path>\n"
					 "\n"
					 "  dynamically loads a new hex file or mcu profile without\n"
					 "  restarting hxptr. on success, shell state is reset.\n"
					 "\n"
					 "  load file    replaces the current hex map. cursor, search\n"
					 "               state, and marks are reset.\n"
					 "  load profile replaces the current profile. the existing\n"
					 "               profile is preserved if the new path fails.\n"
					 "\n"
					 "  examples:\n"
					 "    load file firmware_v2.hex\n"
					 "    load profile stm32f411.prof", },
	{
     .name = "help",
     .type = TOK_HELP,
     .shell_only = 1,
     .handler = help_handler,
     .short_help = "show help for commands",
     .long_help =
			"help [command]\n"
			"\n"
			"  with no args, lists all commands with a short description.\n"
			"  with a command name, shows detailed usage for that command.\n"
			"\n"
			"  examples:\n"
			"    help\n"
			"    help find\n"
			"    help mark", },
	{
     .name = "quit",
     .type = TOK_QUIT,
     .shell_only = 1,
     .handler = NULL,
     .short_help = "exit hxptr",
     .long_help = "quit\n"
					 "\n"
					 "  exits hxptr and restores the terminal.\n"
					 "  ctrl+d has the same effect.", },
};

const size_t cmd_table_count = sizeof(cmd_table) / sizeof(cmd_table[0]);
