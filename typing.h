/**
 * a header for anything that has to do with assigning type
 * the instruction typedef, type enum, and value struct all live here
 * just to seperate this from the general vm header
 *
 * Instruction -> uint32_t (to use fixed 32 bit packed ints for instructions)
 *
 * ValueType -> enum       (primitive type spec)
 *
 * Value -> struct, fields:
 * - ValueType type;       (you know what this means, the type the Value currently has attatched to it)
 * - union as;             (so you can hold that object based on its type)
 */
#ifndef TYPING_H
#define TYPING_H

#include <stdint.h>
#include <stdbool.h>

// forward declare AND create alias
typedef struct Func Func;

// instructions are 32 bit packed. (opcode << 24 | src0 << 16 | src1 << 8 | src2 << 0)
typedef uint32_t Instruction;

// value stuff up here
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

typedef struct Value {
    ValueType type;

    // store the value properly typed
    union {
        bool     b;
        int64_t  i;
        uint64_t u;
        float    f;
        double   d;
        Func*    f;
        void*    o;  // points to the objects location on the heap
    } as;
};


// function types
typedef struct {
    uint32_t entry_ip;  // instruction index
    uint16_t arity;     // fixed args
    uint16_t nregs;     // callee register count
} BytecodeFunc;

typedef struct {
    NativeFn  fn;       // C function pointer
    uint16_t  arity;    // or 0xFFFF for varargs
    uint16_t  _pad;     // keep alignment nice
} NativeFunc;

// forward decl so NativeFn can take VM*
struct VM;

// support for native C functions will be added.
// this is so webservers and shit can exist
typedef enum {
    BYTECODE,
    NATIVE,
} FuncKind;

// allow native fn to be properly passed to value
typedef Value (*NativeFn)(struct VM* vm, Value* args, uint16_t argc);

// properly resolve both bytecode and native functions as callables
typedef struct Func {
    FuncKind kind;
    union {
        BytecodeFunc bc;
        NativeFunc   nat;
    } as;
} Func;

#endif