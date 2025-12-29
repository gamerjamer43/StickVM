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
 * - const Instruction* istream; (pointer to active instruction stream)
 * - uint32_t icount;            (number of instructions)
 * - const Value* consts;        (constant pool used by LOADI/LOADC)
 * - uint32_t constcount;        (length of constant pool)
 * - uint32_t ip;                (instruction pointer / index into code)
 *
 * - Value* regs;                (flat register array used by all frames)
 * - uint32_t regs_cap;          (capacity of regs array)
 *
 * - Value* globals;             (globals storage)
 * - uint32_t globals_len;       (number of globals)
 *
 * - Frame* frames;              (call stack array)
 * - uint32_t frame_count;       (current number of active frames)
 * - uint32_t frame_cap;         (capacity of frames array)
 * - uint32_t panic_code;        (last error / panic code)
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
 * - void vm_load(               (load a compiled chunk ugly looking func)
 *          VM* vm,
 *          const Instruction* code, uint32_t code_len,
 *          const Value* consts, uint32_t consts_len,
 *          Value* globals, uint32_t globals_len
 *   );
 *
 * - bool vm_call(VM* vm, Func* fn, Value* args, uint16_t argc, Value* out);  (invoke a callable; returns success)
 * - bool vm_run(VM* vm);                                                     (execute until HALT or PANIC; returns success)
 *
 * Helpers:
 * - instr_op/instr_a/instr_b/instr_c
 *   (extract packed instruction fields from Instruction)
 * - instr_simm8_b(Instruction)
 *   (interpret B-field as signed 8-bit immediate for branches)
 * - value_truthy(Value)
 *   (determine truthiness for control flow)
 *
 * Notes:
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

// my headers (also pulls in stdint and stdbool)
#include "typing.h"
#include "opcodes.h"

// pack instructions (shift everything to its proper location) likely not needed as instructions should already be packed
// enum just acts as opcode spec, cuz it's enumerated
// static inline Instruction instr_pack(uint8_t op, uint8_t a, uint8_t b, uint8_t c) {
//     return ((uint32_t)op << 24) | ((uint32_t)a << 16) | ((uint32_t)b << 8) | ((uint32_t)c);
// }

// getters for each part of the instruction (simple bit shift & mask)
static inline IField instr_op (Instruction ins) { return (uint8_t)((ins >> 24) & 0xFF); }
static inline IField instr_a  (Instruction ins) { return (uint8_t)((ins >> 16) & 0xFF); }
static inline IField instr_b  (Instruction ins) { return (uint8_t)((ins >>  8) & 0xFF); }
static inline IField instr_c  (Instruction ins) { return (uint8_t)((ins >>  0) & 0xFF); }

// for JMP/JMPIF/JMPIFZ: signed offsets are in src1
static inline int8_t instr_simm8_b(Instruction ins) { return (int8_t)instr_b(ins); }

// object forward decls
typedef struct Obj Obj;
typedef struct ObjStr ObjStr;
typedef struct ObjArray ObjArray;
typedef struct ObjTable ObjTable;

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


// api
void vm_init(VM* vm);
void vm_free(VM* vm);

// load a chunk from the instruction stream
void vm_load(
    VM* vm,                                            // pointer to vm (to load into)
    const Instruction* code, uint32_t code_len,        // instruction stream
    const Value* consts, uint32_t consts_len,          // const pool
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
static inline bool value_truthy(VM* vm, Value v) {
    switch (v.type) {
        case NUL:      return false;   // null always false
        case BOOL:     return v.as.b;  // boolean do by value

        // any number check zero equality
        case I64:      return v.as.i   != 0;
        case U64:      return v.as.u   != 0;
        case FLOAT:    return v.as.f   != 0.0f;
        case DOUBLE:   return v.as.d   != 0.0;

        // objects check if nulled
        case OBJ:      return v.as.obj != NULL;

        // if callable, call it (no args) and use its return value's truthiness
        case CALLABLE: {
            if (vm == NULL || v.as.fn == NULL) return false;
            Value out;

            if (!vm_call(vm, v.as.fn, NULL, 0, &out)) return false;
            return value_truthy(vm, out);
        }

        // otherwise its just false i'll deal w this later lol
        default:       return false;
    }
}

#endif