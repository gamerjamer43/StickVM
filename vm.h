/**
 * @file vm.h
 * @author Noah Mingolelli
 * @brief this header contains the main portion of the vm, and in turn the VM struct
 * License: GPLv3
 */

#ifndef VM_H
#define VM_H

// c std headers
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "typing.h"
#include "opcodes.h"

// pack instructions (shift everything to its proper location)
static inline Instruction instr_pack(uint8_t op, uint8_t a, uint8_t b, uint8_t c) {
    return ((uint32_t)op << 24) | ((uint32_t)a << 16) | ((uint32_t)b << 8) | ((uint32_t)c);
}

// getters for each part of the instruction (simple bit shift & mask)
static inline uint8_t instr_op (Instruction ins) { return (uint8_t)((ins >> 24) & 0xFF); }
static inline uint8_t instr_a  (Instruction ins) { return (uint8_t)((ins >> 16) & 0xFF); }
static inline uint8_t instr_b  (Instruction ins) { return (uint8_t)((ins >>  8) & 0xFF); }
static inline uint8_t instr_c  (Instruction ins) { return (uint8_t)((ins >>  0) & 0xFF); }

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
    uint16_t nregs;      // number of registers reserved for this frame
    Func*    callee;     // function currently being executed
} Frame;

// the big dawg
typedef struct VM {
    // stream of instructions
    const Instruction* code;        // instruction stream
    uint32_t           code_len;    // number of instructions

    // constant pooling (pulled from using LOADI or LOADC)
    const Value*       consts;      // constant pool (allocated at compile time)
    uint32_t           consts_len;  // length of pool

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

// load a chunk
void vm_load(VM* vm,
             const Instruction* code, uint32_t code_len,
             const Value* consts, uint32_t consts_len,
             Value* globals, uint32_t globals_len);

// call entry function (which will be a CALLABLE)
bool vm_call(VM* vm, Func* fn,
             Value* args, uint16_t argc,
             Value* out);

// run until HALT/PANIC
bool vm_run(VM* vm);

// register a native function. may do this a different way
// Func* vm_new_native(VM* vm, NativeFn fn, uint16_t arity);

// helper for implicit falisness
static inline bool value_truthy(Value v) {
    switch (v.type) {
        case NUL:      return false;           // null always false
        case BOOL:     return v.as.b;          // boolean do by value

        // any number check zero equality
        case I64:      return v.as.i != 0;
        case U64:      return v.as.u != 0;
        case FLOAT:    return v.as.f != 0.0f;
        case DOUBLE:   return v.as.d != 0.0;

        // objects check if nulled
        case OBJ:      return v.as.o != NULL;
        case CALLABLE: return v.as.f != NULL;

        // otherwise its just false i'll deal w this later lol
        default:       return false;
    }
}

#endif