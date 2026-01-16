/**
 * @file heap.h
 * @author Noah Mingolelli
 * @brief header used for heap and garbage collection. will doc later
 * uncertain if im even gonna use all of this, just some common things i need before actually implementing this.
 * the plan is deffo a lot longer than the impl
 * 
 */

#ifndef HEAP_H
#define HEAP_H

#include "typing.h"

// I HAVE TWO OPTIONS FOR EXPOSING POINTERS TO OBJECTS ON THE HEAP.
// VIRTUAL POINTERS - store pointers to a specific index in a list, do pointer arithmetic. could pointer pack (or cap @ 4b vals on the heap)
// OR
// RAW POINTERS - store pointers to the actual raw object in memory. then its just a reference, though i would want to add guards which could cost cycles.
// choice is likely raw though i will add a light safety element

// ALSO TWO OPTIONS FOR HEAP:
// BUMP ALLOC, FREE GENERATIONALLY. allocate young objects that die often together (heap objects only used in a local scope, other shit). cons are i'm locked into generational frees
// OR
// FREELIST, GENERATIONAL AND OCCASIONAL DIRECT FREES BASED ON LIFETIME: (i could have an optional lifetime system for better gc handling, 
// and objects can be freed at specific timings by the compiler), cons if i do compiler timings i would have to bake it into the bytecode (dont know how), allocs and frees would cost more than
// a bump allocator (which is the cost for better handling)
// choice is uncertain. free list might make it easier to make a lighter gc op using threading, though i have to figure out if i can ever avoid the pause

typedef enum {
    IDLE = 0, // standard gc state, nothing happening.
    MARK,     // mark directly accessible objects (not necessary to go thru heap)
    TRACE,    // starting from roots, follow their pointers to mark everything reachable. this could be one alloc, or a graph of multiple allocs.
    PREPARE,  // pause the world. pause execution with a flag or call to gc_prepare, and set everything up to sweep (may not stick w this)
    SWEEP,    // walk all heap objects, and free any unmarked objects. this will be done by checking a marked bit
    RESUME,   // an "in between" between sweep and idle, allows the program to catch up and the gc to reset its state

    // may add incremental/tri color gc
    // PROCESS, (process gray objects, white = not proven reachable, gray = proven reachable but pointers not scanned, black = proven reachable and scanned)
    // REMARK, (for mutations during mark)

    // look into weak refs. leave options open
} GC_STATE;

// forward declare GC and object header, cuz im unsure of what to do quite yet
typedef struct GC GC;
typedef struct FieldInfo FieldInfo;
typedef struct MethodInfo MethodInfo;

// rtti for types (one shared instance per type cuz otherwise i got to 48 bytes an instance...)
typedef struct {
    const char *name;       // 4/8 byte ptr to type name
    FieldInfo *fields;      // 4/8 byte ptr to field metadata array
    MethodInfo *methods;    // 4/8 byte ptr to method metadata array
    u16 type;               // 2 byte type id (added to registry)
    u16 flags;              // 2 byte type flags (for compiler attributes like cloneable, builtin, and runtime attributes like abstract)
    u16 parent;             // 2 byte parent type id (allows for ~65.5k classes)
    u16 field_count;        // 2 byte field count (max 65536)
    u16 method_count;       // 2 byte method count (max 65536)
    u16 padding;            // 2 bytes for "idk what the fuck to do"
} ObjInfo;                  // = 40 byte info per type (div by 8 valid)

// rtti for individual fields (metadata only)
typedef struct FieldInfo {
    const char *name;       // 4/8 byte ptr to field name
    u16 type;               // 2 byte for the type this field holds
    u16 offset;             // 2 byte offset from objs start location
    u16 flags;              // 2 bytes for field specific flags (readonly, etc idk)
    u16 padding;            // 2 bytes for "idk what the fuck to do" again
} FieldInfo;                // = 16 bytes (div by 8 valid)

// rtti for methods (metadata only)
typedef struct MethodInfo {
    const char *name;       // 4/8 byte ptr to method name
    void *func_ptr;         // 8 bytes - actual function pointer
    u16 rtntype;            // 2 byte for the type this method returns
    u16 argc;               // 2 byte argument count
    u16 flags;              // 4 bytes - static, virtual, etc.
} MethodInfo;               // = 24 bytes (div by 8 valid)

// per-instance header (one per object)
typedef struct ObjHeader {
    ObjInfo *info;          // 8 bytes - points to shared type metadata
    u32 size;               // 4 bytes - allocation size
    u8  mark;               // 1 byte  - gc mark bits
    u8  tid;                // 1 byte  - thread id
    u8  state;              // 1 byte  - lock state
    u8  generation;         // 1 byte  - gc generation
} ObjHeader;                // = 16 bytes (one per INSTANCE, div by 8)

// builtin object types (god i understand why rust has 16 string types now. C ownership interoperability hard)
typedef struct {
    ObjHeader header;       // 16 bytes
    size_t length;          // 8 byte char count
    char data[];            // string OWNS the data (MIGHT MAKE BORROWED STRINGS TOO CIRCA RUST)
} ObjString;

typedef struct {
    ObjHeader header;       // 16 bytes
    size_t length;          // 8 bytes - item count
    size_t capacity;        // 8 bytes - allocated capacity
    void *data[];           // flexible array member
} ObjArray;

typedef struct ObjTable ObjTable;  // write a hashtable impl for this

/**
 * runs at program startup to initialize the gc
 * @param vm the instance of the vm to check
 * @param gc the gc to check it with
 */
bool gc_init(VM *vm, GC* gc);

/**
 * runs at program shutdown to stop and free the gc
 * @param vm the instance of the vm to check
 * @param gc the gc to check it with
 */
bool gc_free(VM *vm, GC* gc);

/**
 * poll for a potential sweep by checking the allocation counter, marked objects, and then if necessary sweep
 * this will be light on the hot path but the actual sweep will probably be a little extensive
 * trying to decide on stop the world or concurrent
 * @param vm the instance of the vm to check
 * @param gc the gc to check it with
 */
bool gc_poll(VM *vm, GC* gc);

/**
 * mark only the roots (lighter, and will be done often) i like the tri color model, where a lot of objects 
 * sit in gray space and then are sifted back into white/black so that may be what is used here.
 * tri color means roots are marked and potential grays are identified, and then the actual trace will be a sorting process. 
 * then when prepare is called, either the world is stopped or a thread is spawned
 * @param vm the instance of the vm to check
 * @param gc the gc to check it with
 */
bool gc_mark(VM *vm, GC* gc);

/**
 * do a full trace. on every root marked, go through and see what instances can be marked for collection
 * this will potentially contain a black/white/gray sort
 * @param vm the instance of the vm to check
 * @param gc the gc to check it with
 */
bool gc_trace(VM *vm, GC* gc);

/**
 * allocate a header to the gc, used for tracking an object 
 * @param vm the instance of the vm to check
 * @param gc the gc to check it with
 * @return true if successfully allocated, false if not (error)
 * TODO: decide on how to pack all this into a 64 bit val
 */
bool gc_alloc(VM *vm, GC* gc);

#endif