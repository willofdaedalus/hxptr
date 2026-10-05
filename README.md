# hxptr

An interactive Intel HEX inspector for embedded engineers.

hxptr loads a HEX file and maps it into a sparse memory model built from a plain-text
profile describing your MCU's memory layout. Instead of grepping through hex dumps or
writing one-off scripts, you get a persistent cursor, named region navigation, address
expression syntax, and diagnostics — all aware of what your actual target looks like.

Built to answer questions like:

- What's at the vector table in this image?
- Does this firmware write into protected flash regions?
- Is there a gap between the bootloader and application sections?
- Where does this byte sequence appear in the image?

---

## Build

Zero dependencies. Runs on Linux.

```sh
git clone https://git.sr.ht/~willofdaedalus/hxptr
cd hxptr
make
```

Binary gets built into `bin/`

---

## Usage

```sh
./bin/hxptr -f firmware.hex -p profiles/stm32f411ret.prof
```

`-f` is required. `-p` is optional but unlocks region/memory awareness; without it
hxptr assumes a base address of `0x0` and skips profile-dependent commands.

---

## Profile format

Profiles are plain text. No linker script syntax, no toolchain required; just describe
what your hardware has.

```
mcu stm32f411ret arm-cortex-m4
vendor STMicroelectronics

memory flash  0x08000000 524288
memory ram    0x20000000 131072

region vectors      0x08000000 0x080001ff protected
region bootloader   0x08000000 0x08003fff protected
region application  0x08004000 0x0807ffff
```

`memory` declares a block with a start address and size in bytes. `region` declares
a named range with an optional `protected` flag. Protected regions trigger a warning
if the loaded image writes into them.

---

## Shell commands

hxptr drops you into an interactive shell after loading. Commands operate on a cursor;
a position in the sparse memory map.

### `list`

Display entries from the current cursor position.

```
list                        current position, default count
list 0x08000000             from explicit address
list . + 5                  5 entries forward from current
list 0x08000000 : 0x08003fff  range
list region bootloader      named region
```

### `goto`

Move the cursor.

```
goto 0x08000000
goto start
goto end
goto region vectors
goto . + 10
```

### `find`

Search for a byte pattern. Does not move the cursor; prints the address of the match.
Use `goto` to navigate there.

Behaves like vim's `/`: starts from the current position, wraps around with a notice,
and remembers the last find for `next`/`prev`.

```
find deadbeef
find 0xdeadbeef
find 0xdeadbeef 0x08000000
find 0xdeadbeef 0x08000000 : 0x0807ffff
find next
find prev
```

### `which`

Identify what region and memory block the current cursor address falls within.

```
which
which addr
which region
which memory
```

### `info`

Display loaded file metadata, diagnostics, and profile data.

```
info
info stats
info regions
info memory
info diag
info profile
```

---

## Address expression syntax

Most commands that take an address accept a full expression:

| Syntax | Meaning |
|---|---|
| `0x08000000` | explicit address |
| `.` | current cursor position |
| `start` | lowest address in the image |
| `end` | highest address in the image |
| `region <name>` | start of a named region |
| `. + n` / `. - n` | relative offset by n entries |
| `<expr> : <expr>` | range between two expressions |

---

## Diagnostics

After loading, hxptr runs a pass over the sorted memory entries and flags:

- **Gaps** - address ranges with no data above a configurable threshold
- **Overlaps** - records that write to the same address
- **Protected region violations** - data landing in a region marked `protected`

Run `info diag` to see the full report.

---

## Roadmap

- Colour representation is a big priority right now
- `settings` command for runtime configuration
- Pattern find across record boundaries
- AVR support (requires profile format work for separate address spaces)

---

## License

This project is licensed under the **Apache License 2.0** (Apache-2.0).
See the `LICENSE` file for the full license text.
