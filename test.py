"""
simple writer for VM tests
writes a 20 byte header + u32 instruction stream,
plus optional const/global pools (raw Value bytes).
little endian.
"""
from struct import pack
from numpy import array, uint32
from numpy.typing import NDArray
from pathlib import Path

# verbose flag (true if you want each step to print)
VERBOSE: bool = False

# trying to decide on a header, as a temp i'll use this
HEADER: bytes = b"STIK"

# version and flags
VERSION: int = 1
FLAGS: int = 1 if VERBOSE else 0

# opcode table (keep in sync with vm/opcodes.h)
HALT: int = 0
PANIC: int = 1
JMP: int = 2
JMPIF: int = 3
JMPIFZ: int = 4
MOVE: int = 5
LOADI: int = 6
LOADC: int = 7
LOADG: int = 8
STOREG: int = 9

# ValueType tags (from vm/typing.h)
TYPE_I64: int = 3


def ins(op: int, a: int = 0, b: int = 0, c: int = 0) -> int:
    return (op << 24) | (a << 16) | (b << 8) | c


def code(*words: int) -> NDArray[uint32]:
    return array(words, dtype="<u4")


def val_i64(value: int) -> bytes:
    return pack("<B7xq", TYPE_I64, value)


def test(name: str, program: NDArray[uint32], consts=None, globals_=None):
    return (name, program, list(consts or []), list(globals_ or []))


# list of (name, test code, const pool, global pool)
TESTS: list[tuple[str, NDArray[uint32], list[bytes], list[bytes]]] = [
    # halt, successful stop of program (passed)
    test("halt", code(ins(HALT))),

    # immediately panic with code 1 (passed)
    test("panic_code_1", code(ins(PANIC, 1))),

    # jump past panic with code one, run panic with code 3 (passed)
    test("basic_jmp", code(ins(JMP, 1), ins(PANIC, 1), ins(PANIC, 3))),

    # jmpifz should jump over panic when register 1 is 0 (passed)
    test(
        "jmpifz_taken",
        code(
            ins(MOVE, 0, 0),
            ins(JMPIFZ, 0, 1),
            ins(PANIC, 1),
            ins(HALT),
        ),
    ),

    # move should preserve non-zero into the destination register
    # jumps over halt to panic if value in register 1 is 0 (passed)
    test(
        "move_nonzero",
        code(
            ins(LOADC, 0, 0),
            ins(MOVE, 1, 0),
            ins(JMPIFZ, 1, 1),
            ins(HALT),
            ins(PANIC, 1),
        ),
        consts=[val_i64(7)],
    ),

    # loadi should create a non-zero register value (passed, was doing a constant pool bounds check for immediates)
    test(
        "loadi_nonzero",
        code(
            ins(LOADI, 0, 5),
            ins(JMPIFZ, 0, 1),
            ins(HALT),
            ins(PANIC, 1),
        ),
    ),

    # loadc should read the requested constant (index 1 is zero here) (passed)
    # jumps to panic if the value in register 1 is 0
    test(
        "loadc_zero",
        code(
            ins(LOADC, 0, 1),
            ins(JMPIFZ, 0, 1),
            ins(PANIC, 1),
            ins(HALT),
        ),
        consts=[val_i64(5), val_i64(0)],
    ),

    # storeg/loadg should round-trip through the globals pool
    # load 0 from the
    test(
        "loadg_storeg",
        code(
            ins(LOADC, 0, 0),
            ins(STOREG, 0, 0),
            ins(LOADG, 1, 0),
            ins(JMPIFZ, 1, 1),
            ins(HALT),
            ins(PANIC, 1),
        ),
        consts=[val_i64(42)],
        globals_=[val_i64(0)],
    ),
]

# create programs directory if it doesn't exist
Path("programs").mkdir(exist_ok=True)

# generate test files
for i, (name, program, consts, globals_) in enumerate(TESTS):
    count = len(program)
    constc = len(consts)
    globalc = len(globals_)
    filename = f"programs/testop{i}_{name}.stk"

    with open(filename, "wb") as f:
        f.write(pack("<4sHHIII", HEADER, VERSION, FLAGS, count, constc, globalc))
        f.write(program.tobytes())
        if constc:
            f.write(b"".join(consts))
        if globalc:
            f.write(b"".join(globals_))

    if VERBOSE:
        print(f"Created {filename} ({name})")