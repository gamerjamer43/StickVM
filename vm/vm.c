/**
 * @file vm.c
 * @author Noah Mingolelli
 * doccing this later cuz i would rather kill myself rn no cap
 */
#include "vm.h"
#include "io/reader.h"

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
        vm->maxregs = 0;
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
        vm->framecount = 0;
        vm->maxframes = 0;
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
    const Instruction* code, u32 code_len,
    const Value* consts, u32 consts_len,
    const Value* globals_init, u32 globals_len
) {
    // if no allocated vm ep ep ep bad boy
    if (!vm) return;

    // make sure instructions point to the right thing
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
    vm->framecount = 0;

    // take ownership of the instructions (no copy, 8 = out of memory)
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
    if (!vm->globals) {
        vm->panic_code = 8;
        return;
    }

    vm->globals_len = globals_len;
    if (globals_init) {
        memcpy(vm->globals, globals_init, globals_len * sizeof(Value));
    }
}

/**
 * ensure we have enough registers to store a value
 * TODO: check if i should allow for > 256 frame local registers (and how spills should be dealt with)
 * @param vm the vm instance to check
 * @param need a u32 proclaiming how many registers are needed
 */
static inline bool ensure_regs(VM* vm, u32 need) {
    if (!vm) return false;
    if (need <= vm->maxregs) return true;

    // resize or code 8 (out of memory)
    Value* regs = (Value*)realloc(vm->regs, (size_t)need * sizeof(Value));
    if (!regs) {
        vm->panic_code = 8;
        return false;
    }

    // zero initialize registers
    memset(regs + vm->maxregs, 0, (need - vm->maxregs) * sizeof(Value));

    // set properly and return true (no error)
    vm->regs = regs;
    vm->maxregs = need;
    return true;
}

/**
 * jump to a relative offset. used for JMP, JMPIF, and JMPIFZ. improved safety by casting to int64 (avoiding overflows)
 * @param vm a vm struct to read from
 * @param off the offset to jump forward or backwards from
 */
static inline bool jump_rel(VM* vm, i32 off) {
    i64 next = (i64)vm->ip + (i64)off;

    if (next < 0 || next >= (i64)vm->icount) {
        vm->panic_code = 1;
        return false;
    }

    vm->ip = (u32)next;
    return true;
}

/**
 * when run, if this is a native function it's just called normally (i think maybe i should create a stack frame but TODO)
 * if this is a bytecode function
 */
bool vm_call(VM *vm, Func *fn, Value *args, u16 argc, Value *out) {
    if (!vm || !fn) return false;

    switch (fn->kind) {
        case BYTECODE:
            // TODO: implement bytecode function usage
            return false;

        case NATIVE:
            if (!fn->as.nat.fn || argc != fn->as.nat.argc) return false;
            Value ret = fn->as.nat.fn(vm, args, argc);
            if (out) *out = ret;
            return true;

        default:
            return false;
    }
}

/**
 * copy a value from register to register (nulling does not happen here)
 * @param vm a vm struct to read from
 * @param dest the register to copy to
 * @param src the register to copy from
 */
static inline bool copy(VM* vm, u32 dest, u32 src) {
    // assess the need, based on destination or src, and grow if needed
    u32 need = (dest > src ? dest : src) + 1;
    if (!ensure_regs(vm, need)) return false;

    // move from source to destination, zero out source register
    vm->regs[dest] = vm->regs[src];
    return true;
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

        printf("code: %d\n", opcode(ins));
        switch ((Opcode)opcode(ins)) {
            // normal halt returns with no issues
            case HALT:
                return true;

            // panic on failure, panic returns a code from 0-256 with op_a
            case PANIC:
                vm->panic_code = op_a(ins);
                return false;

            // jump (1 instr, signed)
            case JMP: {
                i32 off = op_signed_i24(ins);
                if (!jump_rel(vm, off)) return false;
                break;
            }

            case JMPIF: {
                // not implemented yet, may have to additionally define truthiness
                vm->panic_code = 9;
                return false;
            }

            case JMPIFZ: {
                u32 src = op_a(ins);
                i32 off = op_signed_i16(ins);

                // make sure register is valid before jumping
                if (src >= vm->maxregs) {
                    vm->panic_code = 1;
                    return false;
                }

                // if falsy ignore, but if offset invalid, panic
                // TODO: fix compiler warnings that come with this: if (value_falsy(vm, vm->regs[src])) {
                if (value_falsy(vm->regs[src])) {
                    if (!jump_rel(vm, off)) return false;
                }
                break;
            }

            // copy WITHOUT nulling
            case COPY: {
                u32 dest = op_a(ins);
                u32 src  = op_b(ins);

                if (!copy(vm, dest, src)) return false;
                break;
            }

            // copy AND null (may combine into one instruction)
            case MOVE: {
                u32 dest = op_a(ins);
                u32 src  = op_b(ins);

                if (!copy(vm, dest, src)) return false;
                vm->regs[src] = (Value){0};
                break;
            }

            // load an immediate to a register (16 bit)
            case LOADI: {
                u32 dest  = op_a(ins);
                i32 imm   = op_signed_i16(ins);

                // ensure registers then make the move
                if (!ensure_regs(vm, dest + 1)) return false;

                // TODO: change this to decide signed vs unsigned (for rn loading it signed)
                // as well as bit width. this is just doing i64 for rn. janky approach but if it compiles man
                Value v;
                i64 temp;
                temp = (i64)imm;
                v.type = I64;
                memcpy(v.val, &temp, sizeof(imm));

                vm->regs[dest] = v;
                break;
            }

            // load a constant from the CONSTANT pool
            case LOADC: {
                u32 dest  = op_a(ins);
                u32 index = op_b(ins);

                // if no pool or out of bounds panic
                if (!vm->consts || index >= vm->constcount) {
                    vm->panic_code = 1;
                    return false;
                }

                // ensure registers then make the move
                if (!ensure_regs(vm, dest + 1)) return false;
                vm->regs[dest] = vm->consts[index];
                break;
            }

            // load a global from the pool
            case LOADG: {
                u32 dest  = op_a(ins);
                u32 index = op_b(ins);

                // if no pool or out of bounds panic (really needa fix this name pollution next)
                if (!vm->globals || index >= vm->globals_len) {
                    vm->panic_code = 1;
                    return false;
                }

                // ensure registers then make the move
                if (!ensure_regs(vm, dest + 1)) return false;
                vm->regs[dest] = vm->globals[index];
                break;
            }

            // store a global to the pool
            case STOREG: {
                u32 dest   = op_a(ins);
                u32 index = op_b(ins);

                if (!vm->globals || index >= vm->globals_len) {
                    vm->panic_code = 1;
                    return false;
                }

                if (!ensure_regs(vm, dest + 1)) return false;

                vm->globals[index] = vm->regs[dest];
                break;
            }

            // add src1 and src2 and store in src0
            case ADD: {
                u32 dest = op_a(ins);
                u32 lhs  = op_b(ins);
                u32 rhs  = op_c(ins);

                if (lhs >= vm->maxregs || rhs >= vm->maxregs) {
                    vm->panic_code = 1;
                    return false;
                }

                if (!ensure_regs(vm, dest + 1)) return false;

                // already run into an issue. continue here
                // vm->globals[dest] = vm->regs[lhs] + vm->regs[rhs];
                break;
            }

            // TODO: add JMPIF and JMPIFZ after const and global pool are properly read

            // nothing matched (9 = invalid opcode)
            default:
                vm->panic_code = 9;
                return false;
        }
    }

    // no halt found
    vm->panic_code = 1;
    return false;
}

/**
 * basic debug logging. prints all bytecode instructions (as hex values)
 * @param vm a pointer to an initialized `VM` struct with instructions loaded
 */
void log_instructions(VM *vm) {
    if (!vm || !vm->istream || vm->icount == 0) {
        printf("Code: []\n"); return;
    }

    printf("Code: [");
    for (int i = 0; (u32)i < vm->icount - 1; i++) {
        printf("0x%08" PRIX32 ",", vm->istream[i]);
    }
    printf("0x%08" PRIX32 "]\n", vm->istream[vm->icount - 1]);
    // end of section to remove
}

/**
 * main loop driving this big boy. gonna figure out how to properly modularize next
 */
int main(int argc, char const *argv[]) {
    // default to program.stk if no path for rn. no emitter so im just writing test binaries using python's struct.pack
    const char* path = (argc > 1) ? argv[1] : NULL;
    if (path == NULL) {
        printf("provide a compiled .stk file to run\n");
        exit(0);
    }

    // init and load file (if failed free safely, return panic code or 1 if no code)
    VM vm;
    vm_init(&vm);
    if (!vm_load_file(&vm, path)) {
        printf("error loading %s, code: %u\n", path, vm.panic_code);
        vm_free(&vm);
        return vm.panic_code ? (int)vm.panic_code : 1;
    }

    // check instructions first
    log_instructions(&vm);

    // run returns a status (false with code set if failed)
    bool ok = vm_run(&vm);
    u32 code = vm.panic_code;

    // free everything safely when done, log any errors
    vm_free(&vm);
    if (!ok && code != 0) printf("error, code: %u\n", code);
    return (int)code;
}