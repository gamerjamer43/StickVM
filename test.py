"""
simple writer for VM tests.
all i'm doing with this is taking serialized u32 instructions
and packing them w a 12 byte header.
done with little endian.
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

# list of (name, test code (always packed as <u4 or 32 bit ints))
TESTS: list[tuple[str, NDArray[uint32]]] = [
    # halt, successful stop of program
    ("halt", array([0x00000000], dtype="<u4")),

    # immediately panic with code 3
    ("panic_code_1", array([0x01010000], dtype="<u4")),

    # jump past panic with code one, run panic with code 3
    ("basic_jmp", array([0x02010000, 0x01010000, 0x01030000], dtype="<u4")),
]

# create programs directory if it doesn't exist
Path("programs").mkdir(exist_ok=True)

# generate test files
for i, (name, code) in enumerate(TESTS):
    count = len(code)
    constc = 0 # placeholder
    globalc = 0 # placeholder
    filename = f"programs/testop{i}_{name}.stk"

    with open(filename, "wb") as f:
        f.write(pack("<4sHHIII", HEADER, VERSION, FLAGS, count, constc, globalc))
        f.write(code.tobytes())

    if VERBOSE:
        print(f"Created {filename} ({name})")