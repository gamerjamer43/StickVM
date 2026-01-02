/**
 * @file typing.h
 * @author Noah Mingolelli
 * @brief a header for anything that has to do with assigning type
 * the instruction typedef, type enum, and value struct all live here
 * just to seperate this from the general vm header
 *
 *
 * Instruction -> uint32_t (to use fixed 32 bit packed ints for instructions)
 * IField      -> uint8_t  (to extract specific fields from the packed instruction)
 *
 * ValueType -> enum       (primitive type spec, read through to see all the primitives)
 * FuncType -> enum        (2 options, NATIVE function from C, or a normal BYTECODE function)
 *
 * Value -> struct, fields:
 * - ValueType type;       (you know what this means, the type the Value currently has attatched to it)
 * - union as;             (so you can hold that object based on its type)
 *   - bool b;             (for BOOL)
 *   - int64_t i;          (for I64)
 *   - uint64_t u;         (for U64)
 *   - float f;            (for FLOAT)
 *   - double d;           (for DOUBLE)
 *   - Func* f;            (for CALLABLE)
 *   - void* o;            (for OBJ, points to heap object)
 *
 * Func -> struct, fields:
 * - FuncType kind;        (either BYTECODE for local or NATIVE for C)
 * - union as;             (so you can call the function properly based on its type)
 *   - BytecodeFunc bc;    (for BYTECODE functions)
 *   - NativeFunc nat;     (for NATIVE functions)
 *
 * BytecodeFunc -> struct, fields:
 * - uint32_t entry_ip;    (instruction index where function starts)
 * - uint16_t argc;        (number of arguments)
 * - uint16_t regc;        (number of registers needed)
 *
 * NativeFunc -> struct, fields:
 * - NativeFn fn;          (pointer to the C native function)
 * - uint16_t argc;        (number of arguments)
 * - uint16_t _pad;        (padding for alignment)
 */

#ifndef TYPING_H
#define TYPING_H

#include <stdint.h>
#include <stdbool.h>

// instructions are 32 bit packed. (opcode << 24 | src0 << 16 | src1 << 8 | src2 << 0)
typedef uint32_t Instruction;

// so specific fields are 8 bit
typedef uint8_t IField;

// enum type listing up here
typedef enum ValueType {
    NUL,        // standard "None"/"null" type
    BOOL,       // a true or false value (represented by 0 or 1)
    U64,        // an unsigned 64 bit integer
    I64,        // a signed 64 bit integer
    FLOAT,      // a 32 bit single precision float
    DOUBLE,     // a 64 bit double precision float
    OBJ,        // a general object
    CALLABLE,   // a callable
} ValueType;

// support for native C functions will be added.
// this is so webservers and shit can exist
typedef enum {
    BYTECODE,
    NATIVE,
} FuncType;


// forward declare AND create alias
typedef struct Func Func;
typedef struct Value {
    ValueType type;

    // store the value properly typed
    union {
        bool     b;
        int64_t  i;
        uint64_t u;
        float    f;
        double   d;
        Func*    fn;
        void*    obj;  // points to the objects location on the heap
    } as;
} Value;

// object types
typedef struct Obj Obj; // dunno how to do this yet

typedef struct {
    size_t charc;  // c length - 1 to exclude null term
    char* value[]; // string content
} ObjStr;

typedef struct {
    size_t itemc;  // how many items are in the array
    void* value[]; // whatever content the type has
} ObjArray;

typedef struct ObjTable ObjTable;  // write a hashtable impl for this

// this has to be above the funcs ig
// forward decl so NativeFn can take VM*
typedef struct VM VM;

// allow C natives (pointer to a function that takes these args, this is a feature of the language)
// to be properly passed to value
typedef Value (*NativeFn)(VM* vm, Value* args, uint16_t argc);

// function types
typedef struct {
    uint32_t entry_ip;  // instruction index
    uint16_t argc;      // how many args this takes
    uint16_t regc;      // how many registers this call needs when it runs
} BytecodeFunc;

typedef struct {
    NativeFn  fn;       // pointer to the C native function
    uint16_t  argc;     // how many args this takes
    uint16_t  _pad;     // keep alignment nice
} NativeFunc;

// properly resolve both bytecode and native functions as callables
typedef struct Func {
    FuncType kind;
    union {
        BytecodeFunc bc;
        NativeFunc   nat;
    } as;
} Func;

#endif