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

# verbose flag
VERBOSE: bool = False

# trying to decide on a header, as a temp i'll use this
HEADER: bytes = b"STIK"

# version and flags
VERSION: int = 1
FLAGS: int = 1 if VERBOSE else 0

# opcode table
class Opcode(IntEnum):
    HALT     = 0
    PANIC    = auto()
    JMP      = auto()
    JMPIF    = auto()
    JMPIFZ   = auto()
    COPY     = auto()
    MOVE     = auto()
    LOADI    = auto()
    LOADC    = auto()
    LOADG    = auto()
    STOREG   = auto()

    # call stack
    CALL     = auto()
    TAILCALL = auto()
    RET      = auto()

    # arithmetic
    ADD      = auto()
    SUB      = auto()
    MUL      = auto()
    DIV      = auto()
    DIVU     = auto()
    MOD      = auto()
    MODU     = auto()
    NEG      = auto()

    # comparisons
    EQ       = auto()
    NEQ      = auto()
    GT       = auto()
    GTU      = auto()
    GE       = auto()
    GEU      = auto()
    LT       = auto()
    LTU      = auto()
    LE       = auto()
    LEU      = auto()

    # bitwise
    AND      = auto()
    OR       = auto()
    XOR      = auto()
    LNOT     = auto()
    BNOT     = auto()

    # shifts
    SHL      = auto()
    SHR      = auto()
    SAR      = auto()

# type tags (typing.h)
class Type(IntEnum):
    NUL  = 0
    BOOL = auto()
    U64  = auto()
    I64  = auto()


def ins(op: int, a: int = 0, b: int = 0, c: int = 0) -> int:
    return (op << 24) | (a << 16) | (b << 8) | c


def code(*words: int) -> NDArray[uint32]:
    return array(words, dtype="<u4")


def val_bool(value: bool) -> bytes:
    return pack("<Bq", Type.BOOL, 1 if value else 0)


def val_i64(value: int) -> bytes:
    return pack("<Bq", Type.I64, value)


def test(opcode: Opcode, name: str, program: NDArray[uint32], consts=None, globals_=None):
    return (opcode, name, program, list(consts or []), list(globals_ or []))


# list of (opcode, name, test code, const pool, global pool)
TESTS: list[tuple[Opcode, str, NDArray[uint32], list[bytes], list[bytes]]] = [
    # halt, successful stop of program (passed)
    test(
        Opcode.HALT, "halt", 
        code(
            ins(Opcode.HALT)
        )
    ),

    # immediately panic with code 1 (passed)
    test(
        Opcode.PANIC, "panic_code_1", 
        code(
            ins(Opcode.PANIC, 1)
        )
    ),

    # jump past panic with code one, halt safely (passed)
    test(
        Opcode.JMP, "basic_jmp", 
        code(
            ins(Opcode.JMP, 0, 0, 1), 
            ins(Opcode.PANIC, 1), 
            ins(Opcode.HALT)
        )
    ),

    # jmpif should jump over panic when register 0 is truthy (test again)
    test(
        Opcode.JMPIF, "jmpif_taken",
        code(
            ins(Opcode.LOADI, 0, 0, 1),    # reg[0] = 1 (not zero = truthy)
            ins(Opcode.JMPIF, 0, 0, 1),    # jump over panic to confirm
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # jmpifz should jump over panic when register 0 is 0
    test(
        Opcode.JMPIFZ, "jmpifz_taken",
        code(
            ins(Opcode.LOADI, 0, 0, 0),    # reg[0] = 0 (aka false)
            ins(Opcode.JMPIFZ, 0, 0, 1),   # jump over panic to confirm
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # loads a constant into 0 and copies it into one. then checks if its in 0 AND 1. preserve source w a copy (passed)
    test(
        Opcode.COPY, "copy_nonzero",
        code(
            ins(Opcode.LOADC, 0, 0),       # reg[0] = consts[0] = 7
            ins(Opcode.COPY, 1, 0),        # reg[1] = reg[0] (copy, source preserved)
            ins(Opcode.JMPIFZ, 1, 0, 2),   # if reg[1] == 0, jump to panic
            ins(Opcode.JMPIFZ, 0, 0, 1),   # if reg[0] == 0, jump to panic
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(7)],
    ),

    # move and verify source is nulled after move
    test(
        Opcode.MOVE, "move_nonzero",
        code(
            ins(Opcode.LOADC, 0, 0),       # reg[0] = consts[0] = 7
            ins(Opcode.MOVE, 1, 0),        # reg[1] = reg[0], reg[0] = null
            ins(Opcode.JMPIFZ, 0, 0, 1),   # reg[0] == 0 now if working, so skip panic
            ins(Opcode.PANIC, 1),
            ins(Opcode.JMPIFZ, 1, 0, 1),   # reg[1] should be truthy, don't skip
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(7)],
    ),

    # loadi should create a non-zero register value (passed)
    test(
        Opcode.LOADI, "loadi_nonzero",
        code(
            ins(Opcode.LOADI, 0, 0, 5),    # reg[0] = 5 (immediate)
            ins(Opcode.JMPIFZ, 0, 0, 1),   # if reg[0] == 0 jump over halt
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # loadc should read the requested constant (index 1 is zero here, index 0 is 5)
    test(
        Opcode.LOADC, "loadc_zero",
        code(
            ins(Opcode.LOADC, 0, 1),       # reg[0] = consts[1] = 0
            ins(Opcode.JMPIFZ, 0, 0, 1),   # if reg[0] == 0 skip to halt
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
        consts=[val_i64(5), val_i64(0)],
    ),

    # storeg/loadg should round-trip through the globals pool
    # loads 42 from the constant pool, stores to global pool, and loads it back into reg[1] for this
    test(
        Opcode.LOADG, "loadg_storeg",
        code(
            ins(Opcode.LOADC, 0, 0),       # reg[0] = consts[0] = 42
            ins(Opcode.STOREG, 0, 0),      # globals[0] = reg[0]
            ins(Opcode.LOADG, 1, 0),       # reg[1] = globals[0] = 42
            ins(Opcode.JMPIFZ, 1, 0, 1),   # if reg[1] == 0 panic
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(42)],
        globals_=[val_i64(0)],
    ),

    # ARITHMETIC OPERATORS
    # addition (add reg[0] and reg[1] store in reg[2])
    test(
        Opcode.ADD, "add_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 3),  # reg[0] = 3
            ins(Opcode.LOADI, 1, 0, 4),  # reg[1] = 4
            ins(Opcode.ADD, 2, 0, 1),    # reg[2] = 3 + 4 = 7
            ins(Opcode.JMPIFZ, 2, 0, 1), # if reg[2] falsy, fail
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # subtraction (do reg[0] - reg[1] and store in reg[2])
    test(
        Opcode.SUB, "sub_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 10), # reg[0] = 10
            ins(Opcode.LOADI, 1, 0, 3),  # reg[1] = 3
            ins(Opcode.SUB, 2, 0, 1),    # reg[2] = 10 - 3 = 7
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # multiplication (reg[0] * reg[1], store in reg[2])
    test(
        Opcode.MUL, "mul_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 3),  # reg[0] = 3
            ins(Opcode.LOADI, 1, 0, 4),  # reg[1] = 4
            ins(Opcode.MUL, 2, 0, 1),    # reg[2] = 12
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # signed div (reg[0] / reg[1], store in reg[2])
    # TODO: unsigned div
    test(
        Opcode.DIV, "div_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 20), # reg[0] = 20
            ins(Opcode.LOADI, 1, 0, 4),  # reg[1] = 4
            ins(Opcode.DIV, 2, 0, 1),    # reg[2] = 5
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # modulo (reg[0] % reg[1], store in reg[2])
    test(
        Opcode.MOD, "mod_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 17), # reg[0] = 17
            ins(Opcode.LOADI, 1, 0, 5),  # reg[1] = 5
            ins(Opcode.MOD, 2, 0, 1),    # reg[2] = 17 % 5 = 2
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # negation (-reg[0])
    test(
        Opcode.NEG, "neg_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 5),  # reg[0] = 5
            ins(Opcode.NEG, 0),          # reg[0] = -5 (in-place)
            ins(Opcode.JMPIFZ, 0, 0, 1), # -5 is truthy (nonzero)
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),


    # EQUALITY OPERATORS
    # equality (as this is true it should produce a 1)
    # TODO: force into boolean any equality operators
    test(
        Opcode.EQ, "eq_true",
        code(
            ins(Opcode.LOADI, 0, 0, 5),  # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 5),  # reg[1] = 5
            ins(Opcode.EQ, 2, 0, 1),     # reg[2] = (5 == 5) = 1
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # neq (this is true it should produce a 1 again)
    test(
        Opcode.NEQ, "neq_true",
        code(
            ins(Opcode.LOADI, 0, 0, 5),  # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 3),  # reg[1] = 3
            ins(Opcode.NEQ, 2, 0, 1),    # reg[2] = (5 != 3) = 1
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # gt (reg[0] > reg[1], store in reg[2])
    test(
        Opcode.GT, "gt_true",
        code(
            ins(Opcode.LOADI, 0, 0, 10), # reg[0] = 10
            ins(Opcode.LOADI, 1, 0, 5),  # reg[1] = 5
            ins(Opcode.GT, 2, 0, 1),     # reg[2] = (10 > 5) = 1
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # lt (reg[0] < reg[1], store in reg[2])
    test(
        Opcode.LT, "lt_true",
        code(
            ins(Opcode.LOADI, 0, 0, 3),  # reg[0] = 3
            ins(Opcode.LOADI, 1, 0, 10), # reg[1] = 10
            ins(Opcode.LT, 2, 0, 1),     # reg[2] = (3 < 10) = 1
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # ge (reg[0] >= reg[1], store in reg[2])
    test(
        Opcode.GE, "ge_equal",
        code(
            ins(Opcode.LOADI, 0, 0, 5),  # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 5),  # reg[1] = 5
            ins(Opcode.GE, 2, 0, 1),     # reg[2] = (5 >= 5) = 1
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # le (reg[0] <= reg[1], store in reg[2])
    test(
        Opcode.LE, "le_equal",
        code(
            ins(Opcode.LOADI, 0, 0, 5),  # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 5),  # reg[1] = 5
            ins(Opcode.LE, 2, 0, 1),     # reg[2] = (5 <= 5) = 1
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),


    # BITWISE OPERATORS
    # bitwise and
    test(
        Opcode.AND, "and_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 0b1111),  # reg[0] = 15
            ins(Opcode.LOADI, 1, 0, 0b0101),  # reg[1] = 5
            ins(Opcode.AND, 2, 0, 1),         # reg[2] = 15 & 5 = 5
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # bitwise or
    test(
        Opcode.OR, "or_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 0b1010),  # reg[0] = 10
            ins(Opcode.LOADI, 1, 0, 0b0101),  # reg[1] = 5
            ins(Opcode.OR, 2, 0, 1),          # reg[2] = 10 | 5 = 15
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # bitwise xor
    test(
        Opcode.XOR, "xor_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 0b1111),  # reg[0] = 15
            ins(Opcode.LOADI, 1, 0, 0b1010),  # reg[1] = 10
            ins(Opcode.XOR, 2, 0, 1),         # reg[2] = 15 ^ 10 = 5
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # logical not (forced into bool)
    test(
        Opcode.LNOT, "lnot_basic",
        code(
            ins(Opcode.LOADC, 0, 0),      # reg[0] = true (BOOL)
            ins(Opcode.LNOT, 0),          # reg[0] = !true = false
            ins(Opcode.JMPIFZ, 0, 0, 1),  # if reg[0] == 0, skip panic
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
        consts=[val_bool(True)],
    ),

    # bitwise not
    test(
        Opcode.BNOT, "bnot_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 0),  # reg[0] = 0
            ins(Opcode.BNOT, 0),         # reg[0] = ~0
            ins(Opcode.JMPIFZ, 0, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # SHL: shift left
    test(
        Opcode.SHL, "shl_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 1),  # reg[0] = 1
            ins(Opcode.LOADI, 1, 0, 4),  # reg[1] = 4
            ins(Opcode.SHL, 2, 0, 1),    # reg[2] = 1 << 4 = 16
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # SHR: shift right (logical)
    test(
        Opcode.SHR, "shr_basic",
        code(
            ins(Opcode.LOADI, 0, 0, 16), # reg[0] = 16
            ins(Opcode.LOADI, 1, 0, 2),  # reg[1] = 2
            ins(Opcode.SHR, 2, 0, 1),    # reg[2] = 16 >> 2 = 4
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # * bonuseses:
    # * - anything that i may have fucked up will be checked here
    # * - this includes behavior i support, behavior i wanna check if i support, or behavior not implemented

    # EDGE CASES AND TRICKY SCENARIOS
    # backward jump
    test(
        Opcode.JMP, "bonus_jmp_backward",
        code(
            ins(Opcode.LOADI, 0, 0, 3),           # reg[0] = 3 (counter)
            ins(Opcode.LOADI, 1, 0, 1),           # reg[1] = 1 (loop start, ip=1)
            ins(Opcode.SUB, 0, 0, 1),             # reg[0] -= 1
            ins(Opcode.JMPIFZ, 0, 0, 1),          # if reg[0] == 0, skip to HALT
            ins(Opcode.JMP, 0xFF, 0xFF, 0xFD),    # jump -3 (back to loop start)
            ins(Opcode.HALT),
        ),
    ),

    # jmpifz should NOT jump when register is nonzero
    test(
        Opcode.JMPIFZ, "bonus_jmpifz_not_taken",
        code(
            ins(Opcode.LOADI, 0, 0, 1),    # reg[0] = 1 (truthy)
            ins(Opcode.JMPIFZ, 0, 0, 1),   # should NOT jump
            ins(Opcode.HALT),              # should reach here
            ins(Opcode.PANIC, 1),
        ),
    ),

    # loadi with negative immediate (-1 as signed 16-bit)
    test(
        Opcode.LOADI, "bonus_loadi_negative",
        code(
            ins(Opcode.LOADI, 0, 0xFF, 0xFF),  # reg[0] = -1 (signed)
            ins(Opcode.JMPIFZ, 0, 0, 1),       # -1 is truthy
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # add large numbers (max i32 + max i32), check no crash and make sure result isnt 0
    test(
        Opcode.ADD, "bonus_add_large",
        code(
            ins(Opcode.LOADC, 0, 0),       # reg[0] = 0x7FFFFFFF
            ins(Opcode.LOADC, 1, 0),       # reg[1] = 0x7FFFFFFF
            ins(Opcode.ADD, 2, 0, 1),      # reg[2] = doubled (overflow wraps)
            ins(Opcode.JMPIFZ, 2, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(0x7FFFFFFF)],
    ),

    # subtraction leads to a zero result, make sure the jump happens
    test(
        Opcode.SUB, "bonus_sub_zero_result",
        code(
            ins(Opcode.LOADI, 0, 0, 5),    # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 5),    # reg[1] = 5
            ins(Opcode.SUB, 2, 0, 1),      # reg[2] = 0
            ins(Opcode.JMPIFZ, 2, 0, 1),   # should jump if zero
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # x * 0 = 0
    test(
        Opcode.MUL, "bonus_mul_by_zero",
        code(
            ins(Opcode.LOADI, 0, 0, 100),  # reg[0] = 100
            ins(Opcode.LOADI, 1, 0, 0),    # reg[1] = 0
            ins(Opcode.MUL, 2, 0, 1),      # reg[2] = 0
            ins(Opcode.JMPIFZ, 2, 0, 1),   # should jump if zero
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # equality returns 0 when not equal
    test(
        Opcode.EQ, "bonus_eq_false",
        code(
            ins(Opcode.LOADI, 0, 0, 5),    # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 3),    # reg[1] = 3
            ins(Opcode.EQ, 2, 0, 1),       # reg[2] = 0 (false)
            ins(Opcode.JMPIFZ, 2, 0, 1),   # should jump if zero
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # 5 > 5 is false (equal case)
    test(
        Opcode.GT, "bonus_gt_false_equal",
        code(
            ins(Opcode.LOADI, 0, 0, 5),    # reg[0] = 5
            ins(Opcode.LOADI, 1, 0, 5),    # reg[1] = 5
            ins(Opcode.GT, 2, 0, 1),       # reg[2] = 0
            ins(Opcode.JMPIFZ, 2, 0, 1),   # should jump
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # 3 > 5 is false (less than case)
    test(
        Opcode.GT, "bonus_gt_false_less",
        code(
            ins(Opcode.LOADI, 0, 0, 3),    # reg[0] = 3
            ins(Opcode.LOADI, 1, 0, 5),    # reg[1] = 5
            ins(Opcode.GT, 2, 0, 1),       # reg[2] = 0
            ins(Opcode.JMPIFZ, 2, 0, 1),   # should jump
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # bitwise and with no overlapping bits gives zero
    test(
        Opcode.AND, "bonus_and_zero",
        code(
            ins(Opcode.LOADI, 0, 0, 0b1010),  # reg[0] = 10
            ins(Opcode.LOADI, 1, 0, 0b0101),  # reg[1] = 5
            ins(Opcode.AND, 2, 0, 1),         # reg[2] = 0
            ins(Opcode.JMPIFZ, 2, 0, 1),      # should jump
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # x ^ x = 0, xor with self gives zero
    test(
        Opcode.XOR, "bonus_xor_self_zero",
        code(
            ins(Opcode.LOADI, 0, 0, 42),   # reg[0] = 42
            ins(Opcode.XOR, 1, 0, 0),      # reg[1] = 42 ^ 42 = 0
            ins(Opcode.JMPIFZ, 1, 0, 1),   # should jump
            ins(Opcode.PANIC, 1),
            ins(Opcode.HALT),
        ),
    ),

    # storeg/loadg with multiple global slots, store 10 and 20 then add them
    test(
        Opcode.STOREG, "bonus_globals_multi",
        code(
            ins(Opcode.LOADC, 0, 0),       # reg[0] = 10
            ins(Opcode.LOADC, 1, 1),       # reg[1] = 20
            ins(Opcode.STOREG, 0, 0),      # globals[0] = 10
            ins(Opcode.STOREG, 1, 1),      # globals[1] = 20
            ins(Opcode.LOADG, 2, 0),       # reg[2] = globals[0] = 10
            ins(Opcode.LOADG, 3, 1),       # reg[3] = globals[1] = 20
            ins(Opcode.ADD, 4, 2, 3),      # reg[4] = 30
            ins(Opcode.JMPIFZ, 4, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
        consts=[val_i64(10), val_i64(20)],
        globals_=[val_i64(0), val_i64(0)],
    ),

    # chained arithmetic operators: (3 + 4) * 2 - 1 = 13
    test(
        Opcode.ADD, "bonus_chained_arith",
        code(
            ins(Opcode.LOADI, 0, 0, 3),    # reg[0] = 3
            ins(Opcode.LOADI, 1, 0, 4),    # reg[1] = 4
            ins(Opcode.ADD, 2, 0, 1),      # reg[2] = 7
            ins(Opcode.LOADI, 3, 0, 2),    # reg[3] = 2
            ins(Opcode.MUL, 4, 2, 3),      # reg[4] = 14
            ins(Opcode.LOADI, 5, 0, 1),    # reg[5] = 1
            ins(Opcode.SUB, 6, 4, 5),      # reg[6] = 13
            ins(Opcode.JMPIFZ, 6, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),

    # double negation: --x = x
    test(
        Opcode.NEG, "bonus_neg_double",
        code(
            ins(Opcode.LOADI, 0, 0, 42),   # reg[0] = 42
            ins(Opcode.NEG, 0),            # reg[0] = -42
            ins(Opcode.NEG, 0),            # reg[0] = 42
            ins(Opcode.JMPIFZ, 0, 0, 1),
            ins(Opcode.HALT),
            ins(Opcode.PANIC, 1),
        ),
    ),
]

def main() -> None:
    Path("tests").mkdir(exist_ok=True)
    for (opcode, name, program, consts, globs) in TESTS:
        count = len(program)
        constc = len(consts)
        globalc = len(globs)
        filename = f"tests/testop{opcode.value}_{name}.stk"

        with open(filename, "wb") as f:
            f.write(pack("<4sHHIII", HEADER, VERSION, FLAGS, count, constc, globalc))
            f.write(program.tobytes())
            if constc: f.write(b"".join(consts))
            if globalc: f.write(b"".join(globs))

        if VERBOSE:
            print(f"Created {filename} ({name})")

if __name__ == "__main__":
    main()
