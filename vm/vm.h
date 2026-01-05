/**
 * @file vm.h
 * @author Noah Mingolelli
 * @brief header for the virtual machine core: VM struct, Frame, and public VM API
 * License: GPLv3
 *
 * This file declares the runtime VM which executes packed 32-bit Instructions and
 * manages Values, registers, globals, and call frames.
 *
 * VM -> struct, fields:
 * INSTRUCTIONS:
 * - const Instruction* istream; (VM-owned instruction stream, decays to a pointer)
 * - uint32_t icount;            (number of instructions)
 * - const Value* consts;        (constant pool used by LOADI/LOADC)
 * - uint32_t constcount;        (length of constant pool)
 * - uint32_t ip;                (instruction pointer, just a flat index into the stream)
 *
 * VALUE: just a typed container
 * - Value* regs;                (flat register array used by all frames)
 * - uint32_t regs_cap;          (capacity of regs array)
 *
 * GLOBALS: vm always owns
 * - Value* globals;             (globals storage)
 * - uint32_t globals_len;       (number of globals)
 *
 * FRAMES: all stack frames
 * - Frame* frames;              (call stack array)
 * - uint32_t frame_count;       (current number of active frames)
 * - uint32_t frame_cap;         (capacity of frames array)
 * - uint32_t panic_code;        (set on error (0 if no error))
 *
 *
 * Frame -> struct, fields:
 * - uint32_t return_ip;         (where to resume after RET)
 * - uint16_t base;              (base register index for this frame)
 * - uint16_t regc;              (number of registers reserved by this frame)
 * - Func*    callee;            (the function being executed in this frame)
 *
 * API:
 * - void vm_init(VM* vm);       (initialize VM fields to safe defaults)
 * - void vm_free(VM* vm);       (release any allocated resources)
 * - void vm_load(               (load a compiled chunk; VM takes ownership of instruction stream)
 *          VM* vm,
 *          const Instruction* code, uint32_t code_len,
 *          const Value* consts, uint32_t consts_len,
 *          Value* globals, uint32_t globals_len
 *   );
 *
 * - bool vm_call(VM* vm, Func* fn, Value* args, uint16_t argc, Value* out);  (invoke a callable; returns success)
 * - bool vm_run(VM* vm);                                                     (execute until HALT or PANIC; returns success)
 *
 * HELPERS:
 * - instr_op/instr_a/instr_b/instr_c
 *   (extract packed instruction fields from Instruction)
 * - instr_simm8_b(Instruction)
 *   (interpret B-field as signed 8-bit immediate for branches)
 * - value_falsy(Value)
 *   (determine falsiness for control flow)
 *
 * NOTES:
 * - Instructions are 32-bit packed values: [op:8][a:8][b:8][c:8].
 * - Registers are a single flat array; each call frame is a window into it
 *   defined by Frame.base and Frame.regc.
 * - Callables are represented by Func (bytecode functions or native hooks).
 * - GC / heap management and more advanced error handling are left to future work.
 */

// header spec
#ifndef VM_H
#define VM_H

// c std headers
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

// remove with logging
#include <inttypes.h>

// my headers (also pulls in stdint and stdbool)
#include "typing.h"
#include "opcodes.h"

// read from the header
#define MAGIC "STIK"
#define VERSION 1
// #define FLAG_VERBOSE 0x0001 (gonna add this later)

// legit just an array helper. properly sizes instructions. do not pass anything that isnt an array into this
#define LEN(a) ((uint32_t)(sizeof(a) / sizeof((a)[0])))

// pack instructions (shift everything to its proper location) only needed for testing cuz instructions will be packed
// enum just acts as opcode spec, cuz it's enumerated
static inline Instruction pack(uint8_t op, uint8_t a, uint8_t b, uint8_t c) {
    return ((uint32_t)op << 24) | ((uint32_t)a << 16) | ((uint32_t)b << 8) | ((uint32_t)c);
}

// getters for each part of the instruction (simple bit shift & mask)
static inline uint32_t opcode(Instruction ins) { return (uint8_t)((ins >> 24) & 0xFF); }
static inline uint32_t op_a  (Instruction ins) { return (uint8_t)((ins >> 16) & 0xFF); }
static inline uint32_t op_b  (Instruction ins) { return (uint8_t)((ins >>  8) & 0xFF); }
static inline uint32_t op_c  (Instruction ins) { return (uint8_t)((ins >>  0) & 0xFF); }

// for JMP/JMPIF/JMPIFZ: signed offsets are in src0 for JMP, and src1 for JMPIFZ/JMPIF
static inline int32_t op_signed_a(Instruction ins) { return (int32_t)op_a(ins); }
static inline int32_t op_signed_b(Instruction ins) { return (int32_t)op_b(ins); }

// call frames
typedef struct Frame {
    uint32_t return_ip;  // where to continue after RET
    uint16_t base;       // base register index in vm -> regs for this call
    uint16_t regc;       // number of registers reserved for this frame
    Func*    callee;     // function currently being executed
} Frame;

// the big dawg
typedef struct VM {
    // stream of instructions
    const Instruction* istream;     // instruction stream
    uint32_t           icount;      // number of instructions

    // constant pooling (pulled from using LOADI or LOADC)
    const Value*       consts;      // constant pool (allocated at compile time)
    uint32_t           constcount;  // length of pool

    // instruction pointer
    uint32_t ip;

    // registers (gonna carve Frames via Frame.base as this is a flat array)
    Value*   regs;
    uint32_t regs_cap;

    // globals table (switching to hash but for rn this is ok)
    Value*   globals;
    uint32_t globals_len;

    // call stack
    Frame*   frames;
    uint32_t frame_count;
    uint32_t frame_cap;

    // last error/panic info
    uint32_t panic_code;

    // heap/GC hooks coming later
} VM;


// setup/teardown
void vm_init(VM* vm);
void vm_free(VM* vm);

// load a file into VM owned memory
bool vm_load_file(VM* vm, const char* path);

// load a chunk from the instruction stream
void vm_load(
    VM* vm,                                            // pointer to vm (to load into)
    const Instruction* code, uint32_t code_len,        // instruction stream (owned by VM after call)
    const Value* consts, uint32_t consts_len,          // const pool (borrowed)
    const Value* globals_init, uint32_t globals_len    // global pool
);

// call entry function (which will be a CALLABLE)
bool vm_call(
    VM* vm, Func* fn,
    Value* args, uint16_t argc,
    Value* out
);

// run until HALT/PANIC
bool vm_run(VM* vm);

// register a native function. may do this a different way
// Func* vm_new_native(VM* vm, NativeFn fn, uint16_t argc);

// helper for implicit falisness
// TODO: decide if comparisons are all canonical (overhead) or done by true value
static inline bool value_falsy(VM* vm, Value v) {
    switch (v.type) {
        case NUL:      return true;   // null always false
        case BOOL:     return v.as.b;  // boolean do by value

        // any number check zero equality
        case I64:      return v.as.i   == 0;
        case U64:      return v.as.u   == 0;
        case FLOAT:    return v.as.f   == 0.0f;
        case DOUBLE:   return v.as.d   == 0.0;

        // objects check if nulled
        case OBJ:      return v.as.obj == NULL;

        // functions/others
        case CALLABLE: {
            // null checks
            if (vm == NULL || v.as.fn == NULL) return true;

            // call it (no args yet)
            Value out;
            // TODO: add vm_call
            // if (!vm_call(vm, v.as.fn, NULL, 0, &out)) return true;

            // use its return value's truthiness
            return value_falsy(vm, out);
        }

        // otherwise its just false i'll deal w this later lol
        default:       return false;
    }
}

#endif
