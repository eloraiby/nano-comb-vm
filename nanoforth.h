#pragma once

/*
** Copyright (c) 2017 Wael El Oraiby.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU Affero General Public License as
**  published by the Free Software Foundation, either version 3 of the
**  License, or (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Affero General Public License for more details.
**
**  You should have received a copy of the GNU Affero General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>

#ifdef NDEBUG
#   define log(...)
#else
#   define log(...)    fprintf(stderr, __VA_ARGS__)
#endif

#ifdef __GNUC__
#   define INLINE static __attribute__((always_inline, unused)) inline
#else
#   define INLINE static inline
#endif

#ifndef __cplusplus
#define false   0
#define true    (!false)
typedef uint32_t    bool;
#endif

typedef struct VM   VM;

// these are made as defines because in ISO the enum values are limited to 0x7FFFFFFF
#define OP_VALUE    0x00000000
#define OP_CALL     0x80000000

#define MAX_TOKEN_SIZE  1023

typedef struct {
    uint32_t        insOffset;
    uint32_t        insCount;
} InterpFunction;

typedef void (*NativeFunction)(VM* vm);

typedef enum {
    FT_INTERP   = 0,
    FT_NATIVE   = 1,
} FunctionType;

typedef struct {
    FunctionType    type;
    bool            isImmediate;
    uint32_t        nameOffset;
    uint32_t        inVS;           // numner of input values to pop from the value stack
    uint32_t        outVS;          // number of output values to push to the value stack
    union {
        InterpFunction  interp;
        NativeFunction  native;
    } u;
} Function;

typedef struct {
    uint32_t        fp;     // function pointer
    uint32_t        ip;     // next instruction address
} Return;

typedef enum {
    SM_RO,
    SM_WO,
    SM_RW,
} STREAM_MODE;

typedef struct {
    atomic_uint     refCount;   // should be incremented/decremented atomically
    STREAM_MODE     mode;
    FILE*           file;
} Stream;

typedef struct {
    uint32_t        charCount;
    uint32_t        charCap;
    char*           chars;
    uint32_t        stringCount;
    uint32_t        stringCap;
    uint32_t*       strings;
} StringStack;

typedef struct {
    bool            bf      : 1;    // boolean flag
    union {
        uint32_t        all;
        struct {

            bool            vsOF    : 1;    // value stack overflow flag
            bool            vsUF    : 1;    // value stack underflow flag
            bool            rsOF    : 1;    // return stack overflow flag
            bool            rsUF    : 1;    // return stack underflow flag
            bool            fnOF    : 1;    // function count overflow flag
            bool            insOF   : 1;    // instruction count overflow flag
            bool            chOF    : 1;    // character segment overflow flag
        } indiv;
    } exceptFlags;
} Flags;

typedef struct {
    uint32_t        funcId;     // function index
    uint32_t        ciStart;    // compiler instruction start
} CompiledFunctionEntry;

struct VM {
    bool            quit;

    uint32_t        funcCount;
    uint32_t        funCap;
    Function*       funcs;      // function segment

    uint32_t        insCount;
    uint32_t        insCap;
    uint32_t*       ins;        // code segment

    uint32_t        charCount;
    uint32_t        charCap;
    char*           chars;      // constant char segment

    uint32_t        vsCount;
    uint32_t        vsCap;
    uint32_t*       vs;         // value stack

    uint32_t        rsCount;
    uint32_t        rsCap;
    Return*         rs;         // return stack

    uint32_t        fp;         // current executing function
    uint32_t        ip;         // pointer to the next instruction to fetch
    uint32_t        lp;         // local stack pointer

    Flags           flags;

    uint32_t        strmCount;
    uint32_t        strmCap;
    Stream**        strms;      // stream stack

    StringStack     ss;         // string stack

    // compiler section
    uint32_t        cfsCount;   // compiled function stack count
    uint32_t        cfsCap;
    CompiledFunctionEntry*  cfs;

    uint32_t        cisCount;   // compiler instruction count
    uint32_t        cisCap;
    uint32_t*       cis;
};

#define ABORT_ON_EXCEPTIONS()   { if( vm->flags.exceptFlags.all ) { return; } }
#define ABORT_ON_EXCEPTIONS_V(V)   { if( vm->flags.exceptFlags.all ) { return V; } }

#define STOP_IF(FLAG, COND)     { \
        ABORT_ON_EXCEPTIONS() \
        if( (vm->flags.exceptFlags.indiv.FLAG = COND) ) { return; } } \
    }

typedef struct {
    const char*     name;
    bool            isImmediate;
    NativeFunction  native;
    uint32_t        inCount;
    uint32_t        outCount;
} NativeFunctionEntry;

INLINE
void
vmPushValue(VM* vm, uint32_t v) {
    assert(vm->vsCount < vm->vsCap);
    vm->vs[vm->vsCount] = v;
    ++vm->vsCount;
}

INLINE
uint32_t
vmPopValue(VM* vm) {
    assert(vm->vsCount != 0);
    --vm->vsCount;
    return vm->vs[vm->vsCount];
}

INLINE
void
vmPushReturn(VM* vm) {
    Return r = { .fp = vm->fp, .ip = vm->ip };
    vm->rs[vm->rsCount] = r;
    ++vm->rsCount;
}

INLINE
void
vmPopReturn(VM* vm) {
    --vm->rsCount;
    Return  r   = vm->rs[vm->rsCount];
    vm->fp  = r.fp;
    vm->ip  = r.ip;
}

INLINE
uint32_t
vmGetOperation(uint32_t opcode) {
    return opcode & 0x80000000;
}

INLINE
uint32_t
vmGetOperand(uint32_t opcode) {
    return opcode & 0x7FFFFFFF;
}

INLINE
void
vmPushInstruction(VM* vm, uint32_t opcode) {
    assert(vm->insCount < vm->insCap);
    vm->ins[vm->insCount++] = opcode;
}

INLINE
void
vmPopInstruction(VM* vm) {
    assert(vm->insCount > 0);
    --vm->insCount;
}

INLINE
void
vmPushCompilerInstruction(VM* vm, uint32_t opcode) {
    assert(vm->cisCount < vm->cisCap);
    vm->cis[vm->cisCount++] = opcode;
}

INLINE
void
vmPopCompilerInstruction(VM* vm) {
    assert(vm->cisCount > 0);
    --vm->cisCount;
}

//
// Some day, these will be the ISA for a soft-fpga microcontroller
//
//typedef enum {
//    ARITH           = 0x00,
//    LOGIC           = 0x01,
//    COMP            = 0x02,
//    CONTROL         = 0x03,
//    LOAD_STORE      = 0x04,
//
// } OP_TYPE;
//
//typedef enum {
//    NFOP_NOP        = 0x00,
//    NFOP_ADD_U      ,
//    NFOP_SUB_U      ,
//    NFOP_MUL_U      ,
//    NFOP_DIV_U      ,
//    NFOP_MOD_U      ,
//    NFOP_SHL_U      ,
//    NFOP_SHR_U      ,
//    NFOP_ROL_U      ,
//    NFOP_ROR_U      ,
//    NFOP_EQ_U       ,
//    NFOP_NEQ_U      ,
//    NFOP_GT_U       ,
//    NFOP_LT_U       ,
//    NFOP_GEQ_U      ,
//    NFOP_LEQ_U      ,
//    NFOP_AND        ,
//    NFOP_OR         ,
//    NFOP_INV        ,
//    NFOP_XOR        ,
//    NFOP_CALL_IND   ,
//    NFOP_COND       ,
//    NFOP_READ_MEM   ,
//    NFOP_WRITE_MEM  ,
//    NFOP_DUP_VS     ,
//    NFOP_READ_VS    ,
//    NFOP_WRITE_VS   ,
//    NFOP_READ_RS    ,
//    NFOP_WRITE_RS   ,
//    NFOP_VSC        ,
//    NFOP_RSC        ,
//    NFOP_READ_BF    ,
//    NFOP_WRITE_BF   ,
//    NFOP_READ_CF    ,
//    NFOP_WRITE_CF   ,

//    NFOP_ADD_S      ,
//    NFOP_SUB_S      ,
//    NFOP_MUL_S      ,
//    NFOP_DIV_S      ,
//    NFOP_MOD_S      ,
//    NFOP_SHL_S      ,
//    NFOP_SHR_S      ,
//    NFOP_ROL_S      ,
//    NFOP_ROR_S      ,
//    NFOP_EQ_S       ,
//    NFOP_NEQ_S      ,
//    NFOP_GT_S       ,
//    NFOP_LT_S       ,
//    NFOP_GEQ_S      ,
//    NFOP_LEQ_S      ,

//    NFOP_MAX        ,
//} NF_OPCODE;

void        vmPushString(VM* vm, const char* str);
void        vmPopString(VM* vm);

Stream*     vmStreamOpenFile(VM* vm, const char* name, STREAM_MODE mode);
Stream*     vmStreamFromFile(VM* vm, FILE* f, STREAM_MODE mode);
Stream*     vmStreamMemory  (VM* vm, uint32_t maxSize);
void        vmStreamPush    (VM* vm, Stream* strm);
void        vmStreamPop     (VM* vm, Stream* strm);
uint32_t    vmStreamReadChar(VM* vm, Stream* strm);
bool        vmStreamIsEOS   (VM* vm, Stream* strm);
void        vmStreamWriteChar(VM* vm, Stream* strm, uint32_t ch);
uint32_t    vmStreamSize    (VM* vm, Stream* strm);
uint32_t    vmStreamPos     (VM* vm, Stream* strm);
void        vmStreamSetPos  (VM* vm, Stream* strm, uint32_t pos);

//
// start looking for the function from the last added entry
// Return: 0        -> not found
//         v != 0   -> function index + 1 (decrement to get the function)
//
uint32_t    vmFindFunction(VM* vm, const char* str);
uint32_t    vmAllocateInterpFunction(VM* vm, const char* str);
uint32_t    vmAddNativeFunction(VM* vm, const char* str, bool isImmediate, NativeFunction native);

typedef enum {
    CS_NO_ERROR,
    CS_ERROR,
} COMPILATION_STATE;

COMPILATION_STATE   vmCompileString(VM* vm, const char* str);

void        vmRegisterStdWords(VM* vm);

void        vmNext(VM* vm);

void        vmReadEvalPrintLoop(VM* vm);

typedef struct {
    uint32_t    maxFunctionCount;       // max function count
    uint32_t    maxInstructionCount;    // max instruction count
    uint32_t    maxCharSegmentSize;     // max const char segment size

    uint32_t    maxValuesCount;         // maximum value count (value stack)
    uint32_t    maxReturnCount;         // maximum return count (return stack)
    uint32_t    maxFileCount;           // maximum file count (file stack)
    uint32_t    maxSSCharCount;         // maximum stacked char count (char stack for string stack)
    uint32_t    maxSSStringCount;       // maximum stacked string count (string stack)

    // compiler section
    uint32_t    maxCFCount;             // maximum compiler function count
    uint32_t    maxCISCount;            // maximum compiler instruction count
} VMParameters;

VM*         vmNew(const VMParameters* params);
void        vmRelease(VM* vm);



