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

    // reset non owned fields to safe defaults. null out istream and constant pool
    vm->istream    = NULL;
    vm->icount     = 0;
    vm->consts     = NULL;
    vm->constcount = 0;
    vm->ip         = 0;
    vm->panic_code = 0;
}

/**
 * load a compiled chunk into the `VM` (no ownership of code/const pools)
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

    // init everything provided to the vm
    vm->istream = code;
    vm->icount = code_len;
    vm->consts = consts;
    vm->constcount = consts_len;
    vm->ip = 0;
    vm->panic_code = 0;
    vm->frame_count = 0;

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
 * main vm run loop. while ip < icount execute instructions
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
 * main runtime loop for all this (may move elsewhere)
 * you know what argc and argv do
 */
int main(int argc, char *argv[]) {
    VM vm;

    // init (i have o3 on. this wont print if this dont work)
    vm_init(&vm);
    printf("init successful\n");

    // testing with a halt instruction (0 opcode), and panic instruction (1 opcode, instr 1 = code)
    Instruction halt[] = { pack(0, 0, 0, 0) };
    Instruction panic[] = { pack(1, 1, 0, 0) };
    vm_load(&vm, halt, 1, NULL, 0, NULL, 0);

    uint8_t result = vm_run(&vm);
    if (result) printf("run successful.\n");
    else printf("error, code: %d\n", vm.panic_code);

    // free
    vm_free(&vm);
    printf("free successful\n");

    // no errors
    return 0;
}