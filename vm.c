#include "vm.h"

// init using struct zeroing
void vm_init(VM* vm) {
    if (!vm) return;
    *vm = (VM){0};
}

/**
 * free up everything allocated by the VM to not leak memory (like a responsible citizen)
 * @param vm a pointer to a VM struct containing all of your VM's data
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

int main(int argc, char *argv[]) {
    VM vm;

    // init (i have o3 on. this wont print if this dont work)
    vm_init(&vm);
    printf("init successful");

    // free
    vm_free(&vm);
    printf("free successful");

    // no errors
    return 0;
}