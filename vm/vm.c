/**
 * @file vm.c
 * @author Noah Mingolelli
 * doccing this later cuz i would rather kill myself rn no cap
 */
#include "vm.h"
#include "io/reader.h"

// listing of all error messages. im making it work then im modularizing. alr prematurely optimized lol
const char *const MESSAGES[] = {
    "",
    "File IO error",
    "Register overflow",
    "No halt",
    "Bad magic",
    "Unsupported version",
    "Empty program",
    "Program too large",
    "Out of memory",
    "Truncated code",
    "Const pool read failed",
    "Globals read failed",
    "Register limit exceeded",
    "Stack overflow",
    "Stack underflow",
    "Invalid callable",
    "Call failed",
    "Type mismatch",
    "Invalid opcode",
};

/**
 * init using struct zeroing
 * @param vm a pointer to an empty vm struct
 */
void vm_init(VM* vm) {
    if (!vm) return;
    *vm = (VM){0};

    // TODO: potentially limit registers and frames here (maxregs and maxframes)
}

/**
 * free up everything allocated by the `VM` to not leak memory (like a responsible citizen)
 * @param vm a pointer to a `VM` struct (that already has vm_load used on it)
 */
void vm_free(VM* vm) {
    // if already nulled no worry
    if (vm == NULL) return;

    // free any leftovers
    if (vm->regs) {
        free(vm->regs);
        vm->regs = NULL;
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
        vm->framecap = 0;
    }


    // the vm will store globals, so we can free them here
    if (vm->globals) {
        free(vm->globals);
        vm->globals = NULL;
        vm->globalcount = 0;
    }

    // reset non-owned fields to safe defaults. null out constant pool
    // TODO: i am not freeing the constant pool. pissing memory
    if (vm->consts) {
        free((void*)vm->consts);
        vm->consts = NULL;
        vm->constcount = 0;
    }

    vm->ip         = 0;
    vm->panic_code = NO_ERROR;
}


/**
 * load a compiled chunk into the VM instance. the VM takes ownership of the instructions
 * DO NOT REUSE A VM
 * @param vm a pointer to the `VM` to load into
 * @param code `Instruction` stream pointer
 * @param instrcount `Instruction` count
 * @param consts constant pool pointer
 * @param constcount number of constants
 * @param globals_init initial globals (copied into owned storage)
 * @param globalcount number of globals
 */
void vm_load(
    VM* vm,
    const Instruction* code, u32 instrcount,
    const Value* consts, u32 constcount,
    const Value* globals_init, u32 globalcount
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
    vm->panic_code = NO_ERROR;
    vm->framecount = 0;

    // take ownership of the instructions (no copy, 8 = out of memory)
    if (instrcount > 0) {
        if (!code) {
            vm->panic_code = PANIC_OOM;
            return;
        }

        vm->istream = code;
        vm->icount = instrcount;
    }

    // set constants to provided pool (will be dealt with on load of file)
    vm->consts = consts;
    vm->constcount = constcount;

    // allocate globals (if necessary, if none just return)
    if (globalcount == 0) return;
    vm->globals = (Value*)calloc(globalcount, sizeof(Value));
    if (!vm->globals) {
        vm->panic_code = PANIC_OOM;
        return;
    }

    vm->globalcount = globalcount;
    if (globals_init) {
        memcpy(vm->globals, globals_init, globalcount * sizeof(Value));
    }
}

/**
 * ensure we have enough registers to store a value, havent added wides yet but 256 prealloced will do
 * TODO: check if i should allow for > 256 frame local registers (and how spills should be dealt with)
 * @param vm the vm instance to check
 * @param need a u32 proclaiming how many registers are needed
 */
static inline bool ensure_regs(VM* vm, u32 need) {
    if (need > MAX_REGISTERS) {
        if (vm) vm->panic_code = PANIC_REG_LIMIT;
        return false;
    }
    return true;
}

/**
 * add a frame to the stack
 */
static inline bool push_frame(VM* vm, Frame *frame) {
    if (!vm || !frame) return false;

    // grows will be x2 here too, base of 8
    if (vm->framecount >= vm->framecap) {
        u32 newmax = vm->framecap == 0 ? 8 : vm->framecap * 2;
        if (newmax > MAX_FRAMES) newmax = MAX_FRAMES;

        // already at max capacity
        if (vm->framecount >= newmax) {
            vm->panic_code = PANIC_STACK_OVERFLOW;
            return false;
        }

        // alloc, poll, and set counters properly
        Frame* newframes = (Frame*)realloc(vm->frames, newmax * sizeof(Frame));
        if (!newframes) {
            vm->panic_code = PANIC_OOM;
            return false;
        }
        vm->frames = newframes;
        vm->framecap = newmax;
    }

    // push the frame
    vm->frames[vm->framecount++] = *frame;
    return true;
}

/**
 * pop a frame from the stack
 * @param vm the vm instance
 * @param out optional pointer to store the popped frame
 */
static inline bool pop_frame(VM* vm, Frame *out) {
    if (!vm || vm->framecount == 0) {
        if (vm) vm->panic_code = PANIC_STACK_UNDERFLOW;
        return false;
    }
    vm->framecount--;

    // TODO: look into coroutines, generators, and etc. this is why it's a "pop" not a destruction
    if (out) *out = vm->frames[vm->framecount];
    return true;
}

/**
 * jump to a relative offset. used for JMP, JMPIF, and JMPIFZ. improved safety by casting to int64 (avoiding overflows)
 * @param vm a vm struct to read from
 * @param off the offset to jump forward or backwards from
 */
static inline bool jump_rel(VM* vm, i32 off) {
    i64 next = (i64)vm->ip + (i64)off;

    // (2 = register overflow)
    if (next < 0 || next >= (i64)vm->icount) {
        vm->panic_code = PANIC_REG_OVERFLOW;
        return false;
    }

    vm->ip = (u32)next;
    return true;
}

/**
 * copy a value from register to register (nulling does not happen here)
 * @param vm a vm struct to read from
 * @param dest the register to copy to
 * @param src the register to copy from
 */
static inline bool copy(VM* vm, u32 dest, u32 src, u32 offset) {
    // assess the need, based on destination or src, and grow if needed
    u32 need = (dest > src ? dest : src) + vm->current->base + 1;
    if (!ensure_regs(vm, need)) return false;

    // move from source to destination, zero out source register
    vm->regs->types[dest + offset] = vm->regs->types[src + offset];
    vm->regs->payloads[dest + offset] = vm->regs->payloads[src + offset];
    return true;
}

/**
 * when run, if this is a native function it's just called normally (i think maybe i should create a stack frame but TODO)
 * if this is a bytecode function. base is the register index where args start.
 */
bool vm_call(VM *vm, Func *fn, u32 base, u16 argc, u16 reg) {
    if (!vm || !fn) return false;

    switch (fn->kind) {
        case BYTECODE: {
            // validate argc before doing anything
            if (argc != fn->as.bc.argc) return false;

            // ensure frames (calc via base)
            Frame* caller = vm->current;
            u16 new_base = caller->base + caller->regc;
            if (!ensure_regs(vm, new_base + fn->as.bc.regc)) return false;

            // push frame (safely ofc) and update fp
            Frame callee_frame = {
                .jump = vm->ip,
                .base = new_base,
                .regc = fn->as.bc.regc,
                .reg = reg,
                .callee = fn
            };
            if (!push_frame(vm, &callee_frame)) return false;
            vm->current = &vm->frames[vm->framecount - 1];

            // jump (args are in the first slots so saves us some bullshit)
            vm->ip = fn->as.bc.entry_ip;
            return true;
        }

        // natives r easy legit just a call
        case NATIVE: {
            if (!fn->as.nat.fn || argc != fn->as.nat.argc) return false;
            u32 dest = vm->current->base + reg;
            fn->as.nat.fn(vm, base, argc, dest);
            return true;
        }

        default:
            return false;
    }
}

/**
 * close and free safely on a panic. panics can contain 0 and that is just gonna be a runtime error
 * may just make runtime exception handling sep but i feel like this simplifies handling
 * @param vm pointer to the current vm
 */
u32 vm_panic(u32 code) {
    // quit early
    if (!(code < PANIC_CODE_COUNT)) return code;

    // ansi colors legit arent used anywhere else
    const char* red = "\x1b[31m";
    const char* reset = "\x1b[0m";
    fprintf(
        stderr, "%s[ERROR] Code %d: %s%s\n", 
        red, code, MESSAGES[code], reset
    );
    return code;
}

/**
 * main vm run loop. while ip < icount execute instructions (may move this)
 * potentially look into a dispatch table, as hash lookup would prob speed up some already tight hot loops
 * return false if we do not properly hit a halt, or if we hit panic
 * @param vm the `VM` with instructions loaded into it (vm_load)
 */
bool vm_run(VM* vm) {
    // init checks
    if (!vm || !vm->istream || !vm->regs) return false;

    // default 0, panic = 0 means no errors
    vm->panic_code = NO_ERROR;

    // ensure a base of 16 registers for the entry frame
    if (!ensure_regs(vm, BASE_REGISTERS)) return false;

    // push the initial frame (mark return to the absolute end. deciding if this shud be a panic or if halt should do a return)
    // everything is safe i believe it's just a design choice, but i do not know for certain so i'll doubt myself
    Frame entry = {
        .jump = vm->icount,
        .base = 0,
        .regc = BASE_REGISTERS,
        .callee = NULL
    };
    if (!push_frame(vm, &entry)) return false;
    vm->current = &vm->frames[vm->framecount - 1];

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

            // dry these up but this should work lol. just check if val isnt zero or false
            case JMPIF: {
                u32 src = op_a(ins) + vm->current->base;
                i32 off = op_signed_i16(ins);

                // make sure register is valid before jumping
                if (src >= MAX_REGISTERS) {
                    vm->panic_code = PANIC_REG_OVERFLOW;
                    return false;
                }

                // if falsy ignore, but if offset invalid, panic
                u8 type = vm->regs->types[src];
                u64 payload = vm->regs->payloads[src];
                if (!value_falsy(type, payload)) {
                    if (!jump_rel(vm, off)) return false;
                }
                break;
            }

            case JMPIFZ: {
                u32 src = op_a(ins) + vm->current->base;
                i32 off = op_signed_i16(ins);

                // make sure register is valid before jumping
                if (src >= MAX_REGISTERS) {
                    vm->panic_code = PANIC_REG_OVERFLOW;
                    return false;
                }

                // if falsy ignore, but if offset invalid, panic
                u8 type = vm->regs->types[src];
                u64 payload = vm->regs->payloads[src];
                if (value_falsy(type, payload)) {
                    if (!jump_rel(vm, off)) return false;
                }
                break;
            }

            // copy WITHOUT nulling
            case COPY: {
                u32 dest = op_a(ins);
                u32 src  = op_b(ins);

                if (!copy(vm, dest, src, vm->current->base)) return false;
                break;
            }

            // copy AND null source
            case MOVE: {
                u32 dest = op_a(ins);
                u32 src  = op_b(ins);

                if (!copy(vm, dest, src, vm->current->base)) return false;
                vm->regs->types[src + vm->current->base] = 0;
                vm->regs->payloads[src + vm->current->base] = 0;
                break;
            }

            // load an immediate to a register (16 bit)
            case LOADI: {
                u32 dest  = op_a(ins);
                i32 imm   = op_signed_i16(ins);

                // ensure registers then make the move
                if (!ensure_regs(vm, dest + vm->current->base + 1)) return false;

                // adjust for base and set
                u32 adjusted = dest + vm->current->base;
                vm->regs->types[adjusted] = I64;
                vm->regs->payloads[adjusted] = imm;
                break;
            }

            // load a constant from the CONSTANT pool
            case LOADC: {
                u32 dest  = op_a(ins);
                u32 index = op_b(ins);

                // if no pool or out of bounds panic
                if (!vm->consts || index >= vm->constcount) {
                    vm->panic_code = PANIC_REG_OVERFLOW;
                    return false;
                }

                // ensure registers then make the move
                u32 adjusted = dest + vm->current->base;
                if (!ensure_regs(vm, adjusted + 1)) return false;
                vm->regs->types[adjusted] = vm->consts[index].type;
                memcpy(&vm->regs->payloads[adjusted], vm->consts[index].val, sizeof(u64));
                break;
            }

            // load a global from the pool
            case LOADG: {
                u32 dest  = op_a(ins);
                u32 index = op_b(ins);

                // if no pool or out of bounds panic (really needa fix this name pollution next)
                if (!vm->globals || index >= vm->globalcount) {
                    vm->panic_code = PANIC_REG_OVERFLOW;
                    return false;
                }

                // ensure registers then make the move
                u32 adjusted = dest + vm->current->base;
                if (!ensure_regs(vm, adjusted + 1)) return false;
                vm->regs->types[adjusted] = vm->globals[index].type;
                memcpy(&vm->regs->payloads[adjusted], vm->globals[index].val, sizeof(u64));
                break;
            }

            // store a global to the pool
            case STOREG: {
                u32 dest  = op_a(ins);
                u32 index = op_b(ins);

                if (!vm->globals || index >= vm->globalcount) {
                    vm->panic_code = PANIC_REG_OVERFLOW;
                    return false;
                }

                // copy with this ugly shite
                u32 adjusted = dest + vm->current->base;
                if (!ensure_regs(vm, adjusted + 1)) return false;
                vm->globals[index].type = vm->regs->types[adjusted];
                memcpy(vm->globals[index].val, &vm->regs->payloads[adjusted], sizeof(u64));
                break;
            }

            // call a function: CALL func_reg argc dest
            case CALL: {
                u32 reg  = op_a(ins);
                u16 argc = op_b(ins);
                u16 dest = op_c(ins);

                // bounds check
                u32 abs = reg + vm->current->base;
                if (abs >= MAX_REGISTERS) {
                    vm->panic_code = PANIC_REG_OVERFLOW;
                    return false;
                }

                // yoink from register
                u8  type    = vm->regs->types[abs];
                u64 payload = vm->regs->payloads[abs];
                if (type != CALLABLE) {
                    vm->panic_code = PANIC_INVALID_CALLABLE;
                    return false;
                }

                // extract pointer
                Func* fn;
                memcpy(&fn, &payload, sizeof(Func*));
                if (!fn) {
                    vm->panic_code = PANIC_INVALID_CALLABLE;
                    return false;
                }

                // pull args and call
                if (!vm_call(vm, fn, abs + 1, argc, dest)) {
                    if (vm->panic_code == 0) vm->panic_code = PANIC_CALL_FAILED;
                    return false;
                }

                break;
            }

            // return from function: RET register
            case RET: {
                u32 ret = op_a(ins);
                u32 abs = ret + vm->current->base;

                // get return val then pop
                Frame popped;
                Value returned = {0};
                if (abs < MAX_REGISTERS) {
                    returned.type = vm->regs->types[abs];
                    memcpy(returned.val, &vm->regs->payloads[abs], sizeof(u64));
                }
                
                // if pop somehow failed GET OUT.
                if (!pop_frame(vm, &popped)) return false;

                // jump ip back and restore previous state
                if (vm->framecount == 0) return true;
                vm->current = &vm->frames[vm->framecount - 1];
                vm->ip = popped.jump;

                // store return value in caller spec
                u32 adjusted = vm->current->base + popped.reg;
                vm->regs->types[adjusted] = returned.type;
                memcpy(&vm->regs->payloads[adjusted], returned.val, sizeof(u64));
                break;
            }

            // all operators muahahaha (i think) 
            // binary ops, arithmetic and bitwise
            case ADD:  BINOP(+);  break;
            case SUB:  BINOP(-);  break;
            case MUL:  BINOP(*);  break;
            case DIV:  BINOP(/);  break;
            case MOD:  BINOP(%);  break;
            case AND:  BINOP(&);  break;
            case OR:   BINOP(|);  break;
            case XOR:  BINOP(^);  break;
            case SHL:  BINOP(<<); break;
            case SHR:  BINOP(>>); break;
            // case SAR: figure out what to allow

            // unsigned ops
            case DIVU: UBINOP(/);  break;
            case MODU: UBINOP(%);  break;
            case GTU:  UBINOP(>);  break;
            case GEU:  UBINOP(>=); break;
            case LTU:  UBINOP(<);  break;
            case LEU:  UBINOP(<=); break;
            
            // boolean comparison ops
            case EQ:   CMPOP(==); break;
            case NEQ:  CMPOP(!=); break;
            case GT:   CMPOP(>);  break;
            case GE:   CMPOP(>=); break;
            case LT:   CMPOP(<);  break;
            case LE:   CMPOP(<=); break;

            // unary ops
            case NEG:  UNOP(-); break;
            case BNOT: UNOP(~); break;
            
            // logical not has special cases. ONLY can be used on boolean values. gonna fix jmpif and jmpifz to be the same mayb
            // also prolly gonna figure out a way to fucking dry this cuz its just a type check
            case LNOT: {
                u32 src = op_a(ins) + vm->current->base;
                if (src >= MAX_REGISTERS) { 
                    vm->panic_code = PANIC_REG_OVERFLOW; 
                    return false; 
                }

                if (!ensure_regs(vm, src + 1)) return false;
                if (vm->regs->types[src] != BOOL) { 
                    vm->panic_code = PANIC_TYPE_MISMATCH; 
                    return false; 
                }
                
                vm->regs->payloads[src] = vm->regs->payloads[src] ? 0 : 1;
                break;
            }

            // LEFT TO IMPLEMENT SO FAR:
            // SAR - arithmetic shift right
            // TAILCALL - reuse the same stack frame for a recursive call
            // NEWARR, NEWTABLE, NEWOBJ - heap allocated objects
            // GETELEM, SETELEM - hash table operations
            // ARRGET, ARRSET, ARRLEN - arrray operations
            // CONCAT, STRLEN - string operations
            // I2D, I2F, D2I, F2I - signed conversions (ADD I2U, U2I)
            // U2D, U2F, D2U, F2U - unsigned conversions (ADD THESE OPCODES)

            // nothing matched (9 = invalid opcode)
            default: {
                vm->panic_code = PANIC_INVALID_OPCODE;
                return false;   
            }
        }
    }

    // (3 = no halt found)
    vm->panic_code = PANIC_NO_HALT;
    return false;
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
    if (vm.istream && vm.icount > 0) {
        printf("Code: [");
        for (u32 i = 0; i < vm.icount - 1; i++) {
            printf("0x%08" PRIX32 ",", vm.istream[i]);
        }
        printf("0x%08" PRIX32 "]\n", vm.istream[vm.icount - 1]);
    } 
    else printf("Code: []\n");

    // run returns a status (false with code set if failed)
    bool ok = vm_run(&vm);
    u32 code = vm.panic_code;

    // free everything safely when done, log any errors
    vm_free(&vm);
    if (!ok && code != 0) vm_panic(code);
    return (int)code;
}