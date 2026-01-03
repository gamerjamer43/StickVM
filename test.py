"""
simple writer for VM tests.
all i'm doing with this is taking serialized u32 instructions
and packing them w a 12 byte header.
"""
from struct import pack
from numpy import array, uint32
from numpy.typing import NDArray

# verbose flag (true if you want each step to print)
VERBOSE: bool = False

# trying to decide on a header, as a temp i'll use this
HEADER: bytes = b"STIK"

# pack as unsigned 4 byte ints, also save instruction count
# 01 = panic opcode, 01 = panic code, 0000 rest are null
code: NDArray[uint32] = array([0x01010000], dtype="<u4")
count: int = len(code)

# other necessary info
version: int = 1
flags: int = 1 if VERBOSE else 0

# using .stk extension
with open("program.stk", "wb") as f:
    f.write(pack("<4sHHI", HEADER, version, flags, count))
    f.write(code.tobytes())