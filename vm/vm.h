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
 * - u32 icount;                 (number of instructions)
 * - const Value* consts;        (constant pool used by LOADI/LOADC)
 * - u32 constcount;             (length of constant pool)
 * - u32 ip;                     (instruction pointer, just a flat index into the stream)
 *
 * VALUE: just a typed container
 * - Value* regs;                (flat register array used by all frames)
 * - u32 maxregs;                (capacity of regs array)
 *
 * GLOBALS: vm always owns
 * - Value* globals;             (globals storage)
 * - u32 globals_len;            (number of globals)
 *
 * FRAMES: all stack frames
 * - Frame* frames;              (call stack array)
 * - u32 framecount;             (current number of active frames)
 * - u32 maxframes;              (capacity of frames array)
 * - u32 panic_code;             (set on error (0 if no error))
 *
 *
 * Frame -> struct, fields:
 * - u32 return_ip;              (where to resume after RET)
 * - u16 base;                   (base register index for this frame)
 * - u16 regc;                   (number of registers reserved by this frame)
 * - Func*    callee;            (the function being executed in this frame)
 *
 * API:
 * - void vm_init(VM* vm);       (initialize VM fields to safe defaults)
 * - void vm_free(VM* vm);       (release any allocated resources)
 * - void vm_load(               (load a compiled chunk; VM takes ownership of instruction stream)
 *          VM* vm,
 *          const Instruction* code, u32 code_len,
 *          const Value* consts, u32 consts_len,
 *          Value* globals, u32 globals_len
 *   );
 *
 * - bool vm_run(VM* vm);        (execute until HALT or PANIC; returns success)
 * - bool vm_call(               (invoke a callable; returns success)
 *      VM* vm, Func* fn,
 *      Value* args, u16 argc,
 *      Value* out
 *   );
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
#define LEN(a) ((u32)(sizeof(a) / sizeof((a)[0])))

// max amount of registers and frames we can grow to
#define MAX_REGISTERS 65536
#define MAX_FRAMES 256

// pack instructions (shift everything to its proper location)
static inline Instruction pack(Field op, Field a, Field b, Field c) {
    return ((u32)op << 24) | ((u32)a << 16) | ((u32)b << 8) | ((u32)c);
}

// getters for each part of the instruction (simple bit shift & mask)
static inline u32 opcode(Instruction ins) { return (ins >> 24) & 0xFFu; }
static inline u32 op_a  (Instruction ins) { return (ins >> 16) & 0xFFu; }
static inline u32 op_b  (Instruction ins) { return (ins >>  8) & 0xFFu; }
static inline u32 op_c  (Instruction ins) { return (ins >>  0) & 0xFFu; }

// for LOADI16 (which i may find use elsewhere but just to clean it for rn)
// relies on the same behavior as below
/**
 * does a sign extension of the last 2 bytes of the instruction
 * @param ins a 32 bit instruction
 * @returns a signed 32 bit int (MIGHT change to 64 so this can just load er up)
 */
static inline i32 op_signed_i16(Instruction ins) {
    return ((i32)(ins << 16) >> 16);
}

// for JMPW and other wide single operand signed instructions.
// this works b/c of normal signed right shift behavior. last bit gets pulled down allowing for both zero and sign ext.
/**
 * does a sign extension of the last 3 bytes of the instruction
 * @param ins a 32 bit instruction
 * @returns a signed 32 bit int (64 here potentially too)
 */
static inline i32 op_signed_i24(Instruction ins) {
    return ((i32)(ins << 8)) >> 8;
}

// each frame (and the VM itself) would manage a max of 256 locals. (locals live in registers)
// max of 256 globals (globals live in the global pool)
//

// call frames
typedef struct Frame {
    u32   ret_ip;     // where to continue after RET
    u16   base;       // base register index in vm -> regs for this call
    u16   regc;       // number of registers reserved for this frame
    Func* callee;     // function currently being executed
} Frame;

// the big dawg
typedef struct VM {
    // stream of instructions (and count)
    const Instruction* istream;
    u32    icount;

    // constant pooling (pulled from using LOADC)
    const Value* consts;  // constant pool (allocated at compile time)
    u32    constcount;       // length of pool

    // instruction pointer
    u32    ip;

    // registers (gonna carve Frames via Frame.base as this is a flat array)
    Value* regs;
    u32    maxregs;

    // globals table (switching to hash but for rn this is ok)
    Value* globals;
    u32    globals_len;

    // call stack
    Frame* frames;
    u32    framecount;
    u32    maxframes;

    // last error/panic info
    u32 panic_code;

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
    const Instruction* code, u32 code_len,        // instruction stream (owned by VM after call)
    const Value* consts, u32 consts_len,          // const pool (borrowed)
    const Value* globals_init, u32 globals_len    // global pool
);

// call entry function (which will be a CALLABLE)
bool vm_call(
    VM* vm, Func* fn,
    Value* args, u16 argc,
    Value* out
);

// run until HALT/PANIC
bool vm_run(VM* vm);

// register a native function. may do this a different way
// Func* vm_new_native(VM* vm, NativeFn fn, u16 argc);

// helper for implicit falisness
// TODO: decide if comparisons are all canonical (overhead) or done by true value
static inline bool value_falsy(Value v) {
    u64 val;
    memcpy(&val, v.val, 8);

    switch (v.type) {
        // add a unit type as it's necessary
        case NUL:   return true;       // null always false
        case BOOL:  return !v.val[7];  // boolean do by last bit value. avoids expensive memcmp

        // ints can just be normally compared to zero
        case I64:
        case U64:
            return val == 0;

        // clear signed bit to compare floats properly, 0111 = 7
        case FLOAT: return (val & 0x7FFFFFFF) == 0;
        case DOUBLE: return (val & 0x7FFFFFFFFFFFFFFFULL) == 0;

        // objects check if nulled (TODO: make objs and do this)
        // case OBJ:

        // functions/others (decide how non bools should be handled)
        // case CALLABLE:

        // otherwise its just false i'll deal w this later lol
        default:       return false;
    }
}

#endif