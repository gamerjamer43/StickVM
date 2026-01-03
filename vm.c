/**
 * @file vm.c
 * @author Noah Mingolelli
 * doccing this later cuz i would rather kill myself rn no cap
 */
#include "vm.h"

/**
 * basic debug logging. prints all bytecode instructions (as hex values)
 * @param vm a pointer to an initialized `VM` struct with instructions loaded
 */
void log_instructions(VM *vm) {
    if (!vm || !vm->istream || vm->icount == 0) {
        printf("Code: []\n"); return;
    }

    printf("Code: [");
    for (int i = 0; (uint32_t)i < vm->icount - 1; i++) {
        printf("0x%08" PRIX32 ",", vm->istream[i]);
    }
    printf("0x%08" PRIX32 "]\n", vm->istream[vm->icount - 1]);
    // end of section to remove
}

/**
 * read an exact amt of bits from a packed struct
 */
static bool read_exact(FILE* f, void* out, size_t n) {
    return fread(out, 1, n, f) == n;
}

/**
 * read a 16 bit value from a packed struct
 */
static uint16_t read_u16_le(const uint8_t b[2]) {
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

/**
 * read a 32 bit value from a packed struct
 */
static uint32_t read_u32_le(const uint8_t b[4]) {
    return (uint32_t)b[0]
         | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16)
         | ((uint32_t)b[3] << 24);
}

/**
 * read and load a file into memory. exclusively deals with the file, then vm_load handles the rest
 */
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

    // header shit (3 = truncated header, make sure it's exactly 12 bytes).
    // it goes: STIK <2 byte version> <2 byte flags> <4 byte count>
    uint8_t magic[4];
    uint8_t vbuf[2], fbuf[2], cbuf[4];

    if (!read_exact(f, magic, 4) ||
        !read_exact(f, vbuf, 2)  ||
        !read_exact(f, fbuf, 2)  ||
        !read_exact(f, cbuf, 4))
    {
        err = 3;
        goto fail_file;
    }

    // ensure magic first (4 = bad magic)
    if (memcmp(magic, MAGIC, 4) != 0) {
        err = 4;
        goto fail_file;
    }

    // pulled from the file
    uint16_t version = read_u16_le(vbuf);
    uint16_t flags   = read_u16_le(fbuf); // unused frn i'll let the compiler keep warning me
    uint32_t count   = read_u32_le(cbuf);

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
    fclose(f);
    f = NULL;

    // ensure real count and listed count of instructions match up (9 = truncated code)
    if (got != count) {
        err = 9;
        goto fail_code;
    }

    // vm load deals with the rest
    vm_load(vm, code, count, NULL, 0, NULL, 0);
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

/**
 * init using struct zeroing
 * @param vm a pointer to an empty vm struct
 */
void vm_init(VM* vm) {
    if (!vm) return;
    *vm = (VM){0};
}

/**
 * free up everything allocated by the `VM` to not leak memory (like a responsible citizen)
 * @param vm a pointer to a `VM` struct (that already has vm_load used on it)
 */
void vm_free(VM* vm) {
    // if already nulled no worry
    if (vm == NULL) return;

    // TODO: avoid ownership transfers, and if we do transfer ownership (like the const pool or istream)
    // fuck with this to avoid a double free.
    if (vm->regs) {
        free(vm->regs);
        vm->regs = NULL;
        vm->regs_cap = 0;
    }

    // free instruction stream (casting to void pointer shuts the compiler up)
    if (vm->istream) {
        free((void*)vm->istream);
        vm->istream = NULL;
        vm->icount  = 0;
    }

    // any dead frames get nulled out
    if (vm->frames) {
        free(vm->frames);
        vm->frames = NULL;
        vm->frame_count = 0;
        vm->frame_cap = 0;
    }


    // the vm will store globals, so we can free them here
    if (vm->globals) {
        free(vm->globals);
        vm->globals = NULL;
        vm->globals_len = 0;
    }

    // reset non-owned fields to safe defaults. null out constant pool
    vm->consts     = NULL;
    vm->constcount = 0;
    vm->ip         = 0;
    vm->panic_code = 0;
}


/**
 * load a compiled chunk into the `VM` (VM takes ownership of instruction stream)
 * DO NOT REUSE `VM` OBJECTS
 * @param vm a pointer to the `VM` to load into
 * @param code `Instruction` stream pointer
 * @param code_len `Instruction` count
 * @param consts constant pool pointer
 * @param consts_len number of constants
 * @param globals_init initial globals (copied into VM-owned storage)
 * @param globals_len number of globals
 */
void vm_load(
    VM* vm,
    const Instruction* code, uint32_t code_len,
    const Value* consts, uint32_t consts_len,
    const Value* globals_init, uint32_t globals_len
) {
    // if no allocated vm ep ep ep bad boy
    if (!vm) return;

    if (vm->istream && vm->istream != code) {
        free((void*)vm->istream);
    }

    // keeps the vm in a safe state in case load fails part way
    vm->istream = NULL;
    vm->icount = 0;
    vm->consts = NULL;
    vm->constcount = 0;
    vm->ip = 0;
    vm->panic_code = 0;
    vm->frame_count = 0;

    // take ownership of the instructions (no copy)
    if (code_len > 0) {
        if (!code) {
            vm->panic_code = 8;
            return;
        }

        vm->istream = code;
        vm->icount = code_len;
    }

    // set constants to provided pool (will be dealt with on load of file)
    vm->consts = consts;
    vm->constcount = consts_len;

    // allocate globals (if necessary, if none just return)
    if (globals_len == 0) return;
    vm->globals = (Value*)calloc(globals_len, sizeof(Value));
    if (!vm->globals) return;

    vm->globals_len = globals_len;
    if (globals_init) {
        memcpy(vm->globals, globals_init, globals_len * sizeof(Value));
    }
}

/**
 * main vm run loop. while ip < icount execute instructions (may move this)
 * return false if we do not properly hit a halt, or if we hit panic
 * @param vm the `VM` with instructions loaded into it (vm_load)
 */
bool vm_run(VM* vm) {
    // init checks
    if (!vm || !vm->istream) return false;

    // default 0, panic = 0 means no errors
    vm->panic_code = 0;
    while (vm->ip < vm->icount) {
        // pull current instruction and increment ip
        Instruction ins = vm->istream[vm->ip++];

        switch ((Opcode)opcode(ins)) {
            // normal halt returns with no issues
            case HALT:
                return true;

            // panic on failure, panic returns a code from 0-256 with op_a
            case PANIC:
                vm->panic_code = (uint32_t)op_a(ins);
                return false;

            // no opcode found
            default:
                vm->panic_code = 1;
                return false;
        }
    }

    // no halt found
    vm->panic_code = 1;
    return false;
}

/**
 * main loop driving this big boy. gonna figure out how to properly modularize next
 */
int main(int argc, char const *argv[]) {
    // default to program.stk if no path for rn. no emitter so im just writing test binaries using python's struct.pack
    const char* path = (argc > 1) ? argv[1] : "program.stk";

    // init and load file (if failed free safely, return panic code or 1 if no code)
    VM vm;
    vm_init(&vm);
    if (!vm_load_file(&vm, path)) {
        printf("error loading %s, code: %u\n", path, vm.panic_code);
        vm_free(&vm);
        return vm.panic_code ? (int)vm.panic_code : 1;
    }

    // run returns a status (false with code set if failed)
    bool ok = vm_run(&vm);
    uint32_t code = vm.panic_code;

    // free everything safely when done, log any errors
    vm_free(&vm);
    if (!ok && code != 0) printf("error, code: %u\n", code);
    return (int)code;
}
