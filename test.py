"""
simple writer for VM tests
writes a 20 byte header + u32 instruction stream,
plus optional const/global pools (raw Value bytes).
little endian.
"""
from enum import IntEnum, auto
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
class Opcode(IntEnum):
    HALT    = 0
    PANIC   = auto()
    JMP     = auto()
    JMPIF   = auto()
    JMPIFZ  = auto()
    COPY    = auto()
    MOVE    = auto()
    LOADI   = auto() # trying to decide if i should offer a 16 bit loadi or
    LOADC   = auto()
    LOADG   = auto()
    STOREG  = auto()

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
    test("halt", code(ins(Opcode.HALT))),

    # immediately panic with code 1 (passed)
    test("panic_code_1", code(ins(Opcode.PANIC, 1))),

    # jump past panic with code one, halt safely (passed)
    test("basic_jmp", code(ins(Opcode.JMP, 0, 0, 1), ins(Opcode.PANIC, 1), ins(Opcode.HALT))),

    # jmpifz should jump over panic when register 1 is 0 (passed)
    test(
        "jmpifz_taken",
        code(
            ins(Opcode.LOADI, 0, 0, 0),
            ins(Opcode.JMPIFZ, 0, 0, 1),
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # copy should preserve non-zero into the destination register (and source, copied from below)
    # jumps over halt to panic if value in register 1 is 0 (passed)
    test(
        "copy_nonzero",
        code(
            ins(Opcode.LOADC, 0, 0),
            ins(Opcode.COPY, 1, 0),
            ins(Opcode.JMPIFZ, 1, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(7)],
    ),

    # move should preserve non-zero into the destination register
    # jumps over halt to panic if value in register 1 is 0 (passed normally, but test that the value moved is not still in the previous register)
    test(
        "move_nonzero",
        code(
            ins(Opcode.LOADC, 0, 0),
            ins(Opcode.MOVE, 1, 0),
            ins(Opcode.JMPIFZ, 1, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(7)],
    ),

    # loadi should create a non-zero register value (passed)
    test(
        "loadi_nonzero",
        code(
            ins(Opcode.LOADI, 0, 0, 5),
            ins(Opcode.JMPIFZ, 0, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # loadc should read the requested constant (index 1 is zero here)
    # jumps to panic if the value in register 1 is 0 (passed)
    test(
        "loadc_zero",
        code(
            ins(Opcode.LOADC, 0, 1),
            ins(Opcode.JMPIFZ, 0, 0, 1),
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
        consts=[val_i64(5), val_i64(0)],
    ),

    # storeg/loadg should round-trip through the globals pool
    # loads 42 from the constant pool, stores to global pool, and loads it back into register 1 for this
    test(
        "loadg_storeg",
        code(
            ins(Opcode.LOADC, 0, 0),
            ins(Opcode.STOREG, 0, 0),
            ins(Opcode.LOADG, 1, 0),
            ins(Opcode.JMPIFZ, 1, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(42)],
        globals_=[val_i64(0)],
    ),
]

# create programs directory if it doesn't exist
Path("tests").mkdir(exist_ok=True)

# generate test files
for i, (name, program, consts, globals_) in enumerate(TESTS):
    count = len(program)
    constc = len(consts)
    globalc = len(globals_)
    filename = f"tests/testop{i}_{name}.stk"

    with open(filename, "wb") as f:
        f.write(pack("<4sHHIII", HEADER, VERSION, FLAGS, count, constc, globalc))
        f.write(program.tobytes())
        if constc:
            f.write(b"".join(consts))
        if globalc:
            f.write(b"".join(globals_))

    if VERBOSE:
        print(f"Created {filename} ({name})")