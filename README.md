<h1 align="center"> StickVM 🌿 </h1>

<p align="center"><img width="256" height="256" alt="Stick Logo" src="https://github.com/user-attachments/assets/27d2184b-8638-4f0f-a2ad-48379adcad30" /></p>
<p align="center">
    <img alt="Version" src="https://img.shields.io/badge/version-0.0.20%20indev-blue.svg" />
    <img alt="C-Edition" src="https://img.shields.io/badge/C-C99%2FC11-grey.svg" />
    <img alt="License" src="https://img.shields.io/badge/license-GPLv3-green.svg" />
</p>
<p align="center">You can hit people with it, you can burn it, you can dig in the dirt with it. What, you need a swiss army knife?</p>
<p align="center"><em>A lightweight, register-based bytecode virtual machine written in C.</em></p>

## Table of Contents

- [Introduction](#introduction)
- [Specs](#specs)
- [Planned Features](#planned-features)
- [Quickstart](#quickstart)
- [Architecture](#architecture)
- [Current Instruction Set](#currently-implemented)
- [File Format](#file-format)
- [Development logs](#development-logs)
- [Roadmap](#roadmap)
- [Contributing](#contributing)

## Introduction

<h3 align="center">StickVM is a simple, stable and fast register VM, with tightly packed 32 bit instructions you make the most out of each operation. The design is meant to be extremely compute light, optimizing for minimal cycle count and low memory footprint wherever possible. This makes it <b>perfect</b> for both embedding in larger projects, and using standalone for raw power. The goal is a clean, portable runtime that's easy to understand and extend.</h3>

## Specs

- **32 bit instruction width** — `[op:8][a:8][b:8][c:8]`, giving us room for plenty of information dense bytecode. (a million instructions is only 4mb itself)
- **Windowed register-based architecture** — The entire VM runs on 65536 global registers. Each frame carves out its own slice of the register file using a base offset, and on return everything in that scope is cleared and the pointer is pulled back. Simple.
- **Strong static typing, at both compile and runtime** — The goal is to make this as absolutely explicit as possible. Logical operations can only be done on booleans, each type has RTTI (full metadata WIP, but for right now at least the type)
- **Constant & global pools** — Constants are immutable and baked in at compile time. Globals are mutable and persist across the entire runtime. No need to construct values at runtime, they just get loaded in.
- **Frame Based Model** — In the way the actual stack works, space is reserved for the entry frame (aka main) on run. From there, frames are pushed and popped until we reach the bottom layers end, in which case it will expect a halt (if none is found it will panic with Code 1, though I could make it automatically insert). Registers are local to the VM with the frame reserving a base value to start placing registers at on creation.
- **Values fixed 9 byte width, no packing** — I toyed around with this in 5 different ways and found a solution I really like. Constant/Global values are stored as u8 byte arrays pretty much, and all the others live in a properly aligned registers "file". Cuz I'm crazy, I will likely still look into alternative options, but this leaves us at 9 bytes fixed no matter what, "wasting" only one for the type. If I were to NAN box I would not be able to do typed canonical widths, so this allows extension to basically just be a noop. The layout is:
```c
// constants, globals, and values that otherwise need to be serialized. if we do not need to memcpy more than once in a hot loop it only costs abt 2 cycles + disk/ram overhead
typedef struct {
    u8 type;
    u8 payload[8];
} Value;

// any value in a register actively being used during runtime lives in here. u64 before u8 to keep 8 byte vals aligned properly before dropping to 1 byte
// though idk if this is an issue if we're just allocating all 65536 registers at the start
typedef struct {
    u64 payloads[MAX_REGISTERS];
    u8  types[MAX_REGISTERS];
} Registers;
```
- **Other Stuff** — Quite honestly, I don't exactly know what to show off here. Will be updated as I write more.

## Planned Features 
### (roadmap contains general ideas, whereas this will contain larger more focused items.)

- **Tail-call Optimization** — I cannot tell how often TCO is used, but all I know is that I rarely see it taken advantage of in the languages I use. Tail-call allows you to use the previous stack frame for your next iteration, setting the speed similar to that of a linear operation. This VM will PRIORITIZE tail-call if it can be done, and my goal for a compiler opt is, in the event an unoptimized call is found (that hits some criterion), the function content itself is modified to have an inner and outer, so that it could be converted into a proper tail-call.
- **Mark-and-Sweep (M&S) GC vs Refcounting (RC)** — Not entirely sure if I want to do M&S or refcounting. Both have their perks and downsides, for one refcounting is way easier to implement, and I can stop the circular reference bs by just checking if two objects only reference each other in a loop, and then freeing them both (from this mortal realm). I also do not know how to offer both standard and atomic reference counting. I buy myself a lot more allowed complexity and safety if I learn how to properly implement M&S.
- **Basic Compiler Optimizations** — There's probably about a hundred glaring optimizations I can make during compile time to make it much less of a hassle on startup, wind down, or on hot loops. A small excerpt: <p style="margin-bottom:5px;"></p>
    1. Constant Folding. If there's an expression that can reliably evaluate to an integer constant (i.e. int x = 2 + 3) it will be "folded" (more or less solved and THEN constant pooled, i.e. int x = 6) to avoid the extra operation
    2. Dead Code Trimming. If there's unreachable code, we're going to warn the user at the very least. Now the question is do I want to FORCE them to listen or just make it a compiler warning.
    3. Branch Prediction. If a branch is highly more likely to happen than another (this may require a JIT) it will be made the happy path, and any sort of else statement will be a jump OUT of that loop. 
    4. Inlining and Macros. This can easily be done as the first step of the compiler. Basically anything macroed or inlined will never hit the Bytecode vm, and will instead be embedded directly as an instruction.
    5. String interning. This would be for memory footprint, but in the event a string is reused, it will be interned and the reference will be saved instead of needlessly reallocating.
    6. Stack Allocate when in Scope. If a value does not escape local scope, it can be often be allocated on the stack (this does not apply for dynamically sized elements). Otherwise it goes on the heap and is stored as a global (then pointed to ofc).
    7. Inline CACHING. the most recent function call is cached so we don't have to pull it again. good for hot loops.
- **Lazy Evaluation** — I already have this planned relatively soon. The thought would be to create a u8 type CELL (which stores a sort of frozen computation), which will then be evaluated (prolly the basic cell will allow for multiple methods of evaluation based on usage, but i only have once locks in mind like Rust for right now.) or CLOSURE (which Rust also has but works for both cells and closures).
- **Stack Tracing & Disassembler** — This is gonna be hard at the bytecode level, unless I create a DISASSEMBLER. I may just deal with that at the compiler level, and make any errors throw a generic error that kinda tells you where your error is coming from. But this is deffo a lot later on the list, potentially even after the JIT.
- **Again... "more stuff"** — I still have plenty more to research, so this will be incrementally optimized. Once the core language is done and school picks up **this will still be in maintenance,** although I plan to only sink about 3 hours a week into dedicated tasks.


## Quickstart

To build, I used C99 thru GCC. However, **although both are untested** (feel free to let me know though) you can do Clang if you'd like, and MSVC should be fine too. You may get slightly different (potentially faster?) results.

1) Clone the repo

```bash
git clone https://github.com/gamerjamer43/StickVM
cd StickVM
```

2) Build (mess w the Makefile if you want, -O3 is on with no -g)

```bash
make
```

3) test.py builds test files, and places them in a test folder for the runner to iterate through
```bash
# just run and done. 47 test files currently
python test.py
```
```
runner.py flags:
1. -v (verbose): enables additional logging
2. -p (path arg): the path to your binary (after running `make`)
3. -f (verbose mode): enables more logging
4. -f (verbose mode): enables more logging
```
```bash
python runner.py
```

4) Whenever you want to run a compiled file, simply run
```bash
.\vm <filepath> <args> # .\vm.exe if windows
```

## A look under the hood

A lot of moving parts come together to make this so fast. A combination of multiple different types of data storage specifically made for their types of access, proper handling of ownership vs copying, and doing a lot of reading lead me to create a design that allows for some things to be mutably global, some things to be immutably global, and eventually by default I would like to make locals immutable (allowing mutability, but behind a mut keyword).

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

## Currently Implemented*
***(and passing tests)**

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

### Functions
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
│ Instructions (4 bytes each)        │
├────────────────────────────────────┤
│ Constants (9 bytes ea: type + val) │
├────────────────────────────────────┤
│ Globals (9 bytes ea: type + val)   │
└────────────────────────────────────┘
```

## Development logs
### These are only minor dev logs, (see log.md, I'm a bit behind but I'm focused more on the coding than the documentation.)

#### Recent Changes:
- Preallocated 65536 registers for frame handling (will potentially expand further but that would require 2 word instructions.)
- Added the actual stack, including push/pop of frames, CALL and RET, and the entry frame that gets pushed on main (as we should lol.)
- Added operation macros and ALL operations (BINOP, CMPOP, UNOP are all basically the same, so once one was done all the others were too.)
- Fixed LNOT to **only** work on BOOL types
- Fixed both Value struct padding and `memcpy` abuse.
- All 43 (with the addition of 35 new ones) passing.

#### Known Issues:
- No frontend compiler yet legit just using packed structs.
- Will have to brute

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
- [X] Function calls (instead they're loaded at preruntime)
- [x] CALL/RET tests (comes with the above)

### 🔨 Immediate
- [ ] Make arithmetic operations work with any numeric type (avoid the need for unsigned operators)
- [ ] TCO and the TAILCALL opcode (force it where you can)
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