<h1 align="center"> StickVM 🌿 </h1>

<p align="center"><img src="https://github.com/user-attachments/assets/978945d3-da0f-4224-ae93-c7c6cf880fce" width="250"></p>
<p align="center">
    <img alt="Version" src="https://img.shields.io/badge/version-0.0.1%20indev-blue.svg" />
    <img alt="C" src="https://img.shields.io/badge/c-c11-grey.svg" />
    <img alt="License" src="https://img.shields.io/badge/license-GPLv3-green.svg" />
</p>
<p align="center"><i>pokes with Stick</i> <b>cmonnnn do something.</b></p>
<p align="center"><em>A lightweight, register-based bytecode virtual machine written in C.</em></p>

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Quickstart (dev)](#quickstart-dev)
- [Architecture](#architecture)
- [](#instruction-set)
- [File Format](#file-format)
- [Development logs](#development-logs)
- [Roadmap](#roadmap)
- [Contributing](#contributing)

## Introduction

<h3 align="center">StickVM is a simple, stable and fast register VM, with tightly packed 32 bit instructions you make the most out of each operation. The design is was meant to be extremely compute light, optimizing for minimal cycle count and low memory footprint wherever possible. This makes it <b>perfect</b> for both embedding in larger projects, and using standalone for raw power. The goal is a clean, portable runtime that's easy to understand and extend.</h3>

## Specs

- **32 bit instruction wdth** — `[op:8][a:8][b:8][c:8]`, giving us room for plenty of information dense bytecode. (a million instructions is only 4mb itself)
- **Windowed register-based architecture** — The entire VM runs on 65536 global registers. Each frame carves out its own slice of the register file using a base offset, and on return everything in that scope is cleared and the pointer is pulled back. Simple.
- **Strong static typing, at both compile and runtime** — The goal is to make this as absolutely explicit as possible. Logical operations can only be done on booleans, each type has RTTI (full metadata WIP, but for right now at least the type)
- **Constant & global pools** — Constants are immutable and baked in at compile time. Globals are mutable and persist across the entire runtime. No need to construct values at runtime, they just get loaded in.
- **Other Stuff** — Quite honestly, I don't exactly know what to show off here. Will be updated as I write more.


## Quickstart

To build, I would reccomend gcc, and this is C99. However, you can you clang if you'd like with slightly different results, and MSVC is fine too, as that's what I wrote this on.

1) Clone the repo

```bash
git clone https://github.com/gamerjamer43/StickVM
cd StickVM
```

2) Build (mess w the Makefile if you want, -O3 is on with no -g)

```bash
make
```

3) This builds test files, so run the runner to check em out?

```bash
python runner.py
```

4) Whenever you want to run a compiled file, simply run
```bash
.\vm <filepath> <args> # .\vm.exe if windows
```

## A look under the hood

A lot of moving parts come together to make this so fast. A combination of multiple different types of data storage specifically made for their types of access, proper handling of ownership vs copying, and doing a lot of reading lead me to create a design that allows for some things to be mutably global, some things to be immutably global, and eventually by default I would like to make locals immutable by default (but you can do mut or immut globals).

```
┌─────────────────────────────────────────────────────┐
│                        VM                           │
├─────────────────────────────────────────────────────┤
│  istream[]     - instruction array                  │
│  consts[]      - constant pool (immutable)          │
│  globals[]     - global variables (mutable)         │
│  regs          - flat register file (types + vals)  │
│  frames[]      - call stack                         │
│  ip            - instruction pointer                │
│  panic_code    - error state                        │
└─────────────────────────────────────────────────────┘
```

**Registers:** One array alloced at the start of runtime, which is shared across all `Frame`s in scope. Each `Frame` has a `base` offset and `regc` count defining its window into the register file.
**Values:** 9-byte structs with 1-byte type tag + 8-byte payload. Registers store types and payloads separately for cache efficiency, as letting the 8 byte values fill out first leaves us just reading 1 byte values without worries about alignment.

## Currently Implemented (and passing tests)

### Control Flow
| Opcode | Args | Description |
|--------|------|-------------|
| `HALT` | — | Stop execution successfully |
| `PANIC` | a | Abort with error code `a` |
| `JMP` | imm24 | Jump relative by signed 24-bit offset |
| `JMPIF` | a, imm16 | Jump if `reg[a]` is truthy |
| `JMPIFZ` | a, imm16 | Jump if `reg[a]` is falsy |

### Data Movement
| Opcode | Args | Description |
|--------|------|-------------|
| `COPY` | a, b | `reg[a] = reg[b]` (preserve source) |
| `MOVE` | a, b | `reg[a] = reg[b]`, null source |
| `LOADI` | a, imm16 | Load signed 16-bit immediate into `reg[a]` |
| `LOADC` | a, b | Load constant `consts[b]` into `reg[a]` |
| `LOADG` | a, b | Load global `globals[b]` into `reg[a]` |
| `STOREG` | a, b | Store `reg[a]` into `globals[b]` |

### Arithmetic (all binary: `reg[a] = reg[b] OP reg[c]`)
| Opcode | Description |
|--------|-------------|
| `ADD` | Addition |
| `SUB` | Subtraction |
| `MUL` | Multiplication |
| `DIV` | Signed division |
| `MOD` | Signed modulo |
| `NEG` | Unary negation (in-place) |

### Comparisons (result is BOOL)
| Opcode | Description |
|--------|-------------|
| `EQ` | Equal |
| `NEQ` | Not equal |
| `GT` / `GE` | Greater than / greater or equal |
| `LT` / `LE` | Less than / less or equal |

### Bitwise
| Opcode | Description |
|--------|-------------|
| `AND` / `OR` / `XOR` | Bitwise ops |
| `SHL` / `SHR` | Shift left / right |
| `BNOT` | Bitwise NOT (unary) |
| `LNOT` | Logical NOT (BOOL only) |

### Functions (not quite but we getting there)
| Opcode | Args | Description |
|--------|------|-------------|
| `CALL` | a, b, c | Call `reg[a]` with `b` args, store result in `reg[c]` |
| `RET` | a | Return `reg[a]` to caller |


## File Format
`.stk` binary format (little-endian):
The header is 20 bytes containing pretty much all the info anyone needs to know
```
┌────────────────────────────────────┐
│ Header (20 bytes)                  │
├────────────────────────────────────┤
│ 4B  magic     "STIK"               │
│ 2B  version                        │
│ 2B  flags                          │
│ 4B  instruction count              │
│ 4B  constant count                 │
│ 4B  global count                   │
├────────────────────────────────────┤
│ Instructions (4B each)             │
├────────────────────────────────────┤
│ Constants (9B each: type + val)    │
├────────────────────────────────────┤
│ Globals (9B each: type + val)      │
└────────────────────────────────────┘
```

## Development logs

#### Recent Changes:
- Preallocated 65536 registers for frame handling (will potentially expand further but that would require 2 word instructions.)
- Added the actual stack, including push/pop of frames, CALL and RET, and the entry frame that gets pushed on main (as we should lol.)
- Added operation macros and ALL operations (BINOP, CMPOP, UNOP are all basically the same, so once one was done all the others were too.)
- Fixed LNOT to **only** work on BOOL types
- Fixed both Value struct padding and `memcpy` abuse.
- All 43 (with the addition of 35 new ones) passing.

#### Known Issues:
- CALL/RET not testable yet (need MKFUNC or function table)
- No frontend compiler yet

## Roadmap

### ✅ Done
- [x] VM core (init, load, run, free)
- [x] Binary file reader with header validation
- [x] Register file with type tags
- [x] Frame-based call stack
- [x] Control flow (HALT, PANIC, JMP, JMPIF, JMPIFZ)
- [x] Data movement (COPY, MOVE, LOADI, LOADC, LOADG, STOREG)
- [x] Arithmetic (ADD, SUB, MUL, DIV, MOD, NEG)
- [x] Comparisons (EQ, NEQ, GT, GE, LT, LE)
- [x] Bitwise (AND, OR, XOR, BNOT, LNOT, SHL, SHR)
- [x] CALL and RET implementation
- [x] Python test generator + runner (just a QOL thing, don't wanna manually run >5 tests)

### 🔨 Immediate
- [ ] Instruction to create callable from bytecode offset, as I haven't fully set up functions yet. (MAKEFN)
- [ ] CALL/RET tests (comes with the above)
- [ ] TCO and the TAILCALL opcode
- [ ] Unsigned ops (DIVU, MODU, GTU, GEU, LTU, LEU)
- [ ] SAR (arithmetic shift right)

### 🎯 High Priority
- [ ] Frontend compiler (Stick language → bytecode, handwriting a parser rn)
- [ ] Native function registration API (relatively simple)
- [ ] Float/double arithmetic support (fixed at u64 and i64 just for rn)
- [ ] Type conversion ops (I2D, I2F, D2I, F2I)

### 🧱 Core Features
- [ ] Heap allocation (NEWARR, NEWTABLE, NEWOBJ)
- [ ] Object access (GETELEM, SETELEM, ARRGET, ARRSET, ARRLEN)
- [ ] String operations (CONCAT, STRLEN)
- [ ] GC and hooks (never written one, may do Boehms)

### 💫 Future
- [ ] Debug info / source maps
- [ ] Disassembler
- [ ] REPL
- [ ] JIT compilation (maybe)

## Contributing

PRs welcome. Keep changes focused. Match the existing style.

## Acknowledgments

### Built with 🔧:
- Pure C99 (may jump it to C11 for _Generic and _Static_assert, better runtime handling). No external deps.
- Numpy, rich, and the pystdlib for just the tests.
- A lack of desire to sleep, and a lot of vyvanse :)

---

<p align="center">
    Made with 🌿 by <a href="https://github.com/gamerjamer43">gamerjamer43</a>
</p>

<p align="center">
    <sub>Execute responsibly.</sub>
</p>