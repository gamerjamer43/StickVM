/**
 * @file opcodes.h
 * @author Noah Mingolelli
 * @brief the opcode table is so long that i wanted to make a sep file
 *
 * read what each one does and how i have it laid out. i commented under each to avoid a giant docstring.
 */

#ifndef OPCODES_H
#define OPCODES_H

typedef enum {
    /**
     * to make things easier
     *
     * order of ops will always be denoted
     * [op][src0][src1][src2]
     */

    HALT = 0,  // stop program execution

    // move instruction pointer
    JMP,       // offset instruction pointer by a signed 32 bit value
    JMPIF,     // if src0 != 0, IP += (int32_t)src1
    JMPIFZ,    // if src0 == 0, IP += (int32_t)src1

    // movement and storage
    MOVE,      // move values between registers
    LOADC,     // load constant into src0     C = constant
    LOADG,     // load global into src0
    STOREG,    // store global into the table G = global

    // call stack
    CALL,      // create a stack frame and jump
    TAILCALL,  // reuse current stack frame for another call
    RET,       // return to caller

    // arithmetic operators
    ADD,       // add src1 and src2 and store in the src0
    SUB,       // do src1 minus src2 and store in the src0
    MUL,       // multiply src1 and src2 and store in src0
    DIV,       // do src1 divided by src2 and store in src0 (signed ints)
    DIVU,      // do src1 divided by src2 and store in src0 (unsigned ints)
    MOD,       // do src1 mod src2 and store in src0 (signed ints)
    MODU,      // do src1 mod src2 and store in src0 (unsigned ints)
    NEG,       // unary negation. multiply the value in src0 by -1

    // logical operators
    EQ,        // src0 = (src1 == src2)
    NEQ,       // src0 = (src1 != src2)
    GT,        // src0 = (src1 >  src2) (signed)
    GTU,       // src0 = (src1 >  src2) (unsigned)
    GE,        // src0 = (src1 >= src2) (signed)
    GEU,       // src0 = (src1 >= src2) (unsigned)
    LT,        // src0 = (src1 <  src2) (signed)
    LTU,       // src0 = (src1 <  src2) (unsigned)
    LE,        // src0 = (src1 <= src2) (signed)
    LEU,       // src0 = (src1 <= src2) (unsigned)

    // bitwise (and also logical operators as we're gonna do implicit falsiness)
    AND,       // bitwise and src1 by src2 and store in src0
    OR,        // bitwise or src1 by src2 and store in src0
    XOR,       // bitwise exclusive or src1 by src2 and store in src0
    NOT,       // bitwise not src1 by src2 and store in src0

    // shifts
    SHL,       // shift left src1 by src2 and store in src0 (signed ints)
    SHLU,      // shift left src1 by src2 and store in src0 (unsigned ints)
    SHR,       // shift right src1 by src2 and store in src0 (signed ints)
    SHRU,      // shift right src1 by src2 and store in src0 (unsigned ints)
    SAR,       // arithmetic shift right src1 by src2 and store in src0 (signed ints only)

    // heap
    NEWARR,    // dst = new array(length)
    NEWTABLE,  // dst = new hashmap
    NEWOBJ,    // dst = instance/struct with fields

    // tables
    GETELEM,   // dst = table[key] (nil if missing)
    SETELEM,   // table[key] = value

    // arrays
    ARRGET,    // dst = arr[idx]
    ARRSET,    // arr[idx] = val
    ARRLEN,    // dst = length(arr)

    // strings
    CONCAT,    // dst = str(src1) + str(src2)
    STRLEN,    // dst = length(str)

    // conversions
    I2D,       // int to double. dst = (double)src1
    I2F,       // int to float. dst = (float)src1
    D2I,       // double to int. dst = (int64_t)src1
    F2I,       // float to int. dst = (int64_t)src1
    I2U,       // signed to unsigned 64 bit. dst = (uint64_t)src1
    U2I,       // unsigned to signed 64 bit. dst = (int64_t)src1

    // runtime errors
    PANIC,     // abort execution (attach an error code thru a constant)

    // maybe intermediate forms idk yet
    // LOADI   // load an immediate value (src1) into src0 (LOADI r0, 420)

    // more here
} Opcode;

#endif