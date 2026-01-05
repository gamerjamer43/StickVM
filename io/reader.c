#include "reader.h"

bool read_exact(FILE* f, void* out, size_t n) {
    return fread(out, 1, n, f) == n;
}

uint16_t read_u16_le(const uint8_t b[2]) {
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

uint32_t read_u32_le(const uint8_t b[4]) {
    return pack(b[3], b[2], b[1], b[0]);
}


bool vm_load_file(VM* vm, const char* path) {
    if (!vm || !path) return false;

    // any io errors stop here (2 = file io error)
    FILE* f = fopen(path, "rb");
    if (!f) {
        vm->panic_code = 2;
        return false;
    }

    // no error or code yet
    int err = 0;
    Instruction* code = NULL;

    // header shit (3 = truncated header, make sure it's exactly 20 bytes).
    // it goes: STIK <2 byte version> <2 byte flags> <4 byte instruction count> <4 byte constant count> <4 byte global count>
    uint8_t magic[4], vbuf[2], fbuf[2], icount[4], ccount[4], gcount[4];

    if (
        !read_exact(f, magic, 4)  ||
        !read_exact(f, vbuf, 2)   ||
        !read_exact(f, fbuf, 2)   ||
        !read_exact(f, icount, 4) ||
        !read_exact(f, ccount, 4) ||
        !read_exact(f, gcount, 4)
    ) {
        err = 3;
        goto fail_file;
    }

    // ensure magic first (4 = bad magic)
    if (memcmp(magic, MAGIC, 4) != 0) {
        err = 4;
        goto fail_file;
    }

    // pulled from the file
    uint16_t version     = read_u16_le(vbuf);
    uint32_t count       = read_u32_le(icount);
    uint32_t constcount  = read_u32_le(ccount);
    uint32_t globalcount = read_u32_le(gcount);

    // also pulled but unused frn i'll let the compiler keep warning me
    uint16_t flags       = read_u16_le(fbuf);

    // version check (5 = unsupported version)
    if (version != VERSION) {
        err = 5;
        goto fail_file;
    }

    // sanity checks (6 = empty. empty programs are errors for rn, will write a halt later)
    if (count == 0) {
        err = 6; // empty
        goto fail_file;
    }

    // prevent overflow in malloc
    if (count > (UINT32_MAX / (uint32_t)sizeof(Instruction))) {
        err = 7; // too big
        goto fail_file;
    }

    // setup instruction pointer
    code = (Instruction*)malloc((size_t)count * sizeof(Instruction));
    if (!code) {
        err = 8; // out of memory
        goto fail_file;
    }

    // read instructions into the buffer (using little endian by default) and safely null file when done
    size_t got = fread(code, sizeof(Instruction), count, f);

    // MAY EDIT UP THIS SECTION, A DRY FEELS VERY NECESSARY
    // read constant pool after code
    Value* consts = NULL;
    if (constcount > 0) {
        consts = (Value*)malloc(constcount * sizeof(Value));
        if (!consts || !read_exact(f, (uint8_t*)consts, constcount * sizeof(Value))) {
            err = 10;  // const pool read failed
            free(consts);
            goto fail_code;
        }
    }

    // lastly the global pool
    Value* globals = NULL;
    if (globalcount > 0) {
        globals = (Value*)malloc(globalcount * sizeof(Value));
        if (!globals || !read_exact(f, (uint8_t*)globals, globalcount * sizeof(Value))) {
            err = 11;  // globals read failed
            free(globals);
            free(consts);
            goto fail_code;
        }
    }
    // END POTENTIAL EDIT

    // close safely and null out
    fclose(f);
    f = NULL;

    // ensure real count and listed count of instructions match up (9 = truncated code)
    if (got != count) {
        err = 9;
        goto fail_code;
    }

    // vm load deals with the rest
    vm_load(vm, code, count, consts, constcount, globals, globalcount);
    if (!vm->istream) {
        err = vm->panic_code ? (int)vm->panic_code : 8;
        goto fail_code;
    }

    // TODO: load constant and global pools next
    // vm->file_flags = flags; will deal with storing flags later

    return true;

// frees code on an error (then falls thru to fail file)
fail_code:
    free(code);

// errors before the code exists or after the code is freed
fail_file:
    if (f) fclose(f);
    vm->panic_code = err;
    return false;
}