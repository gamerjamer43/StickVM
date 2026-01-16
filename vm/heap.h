/**
 * @file heap.h
 * @author Noah Mingolelli
 * @brief header used for heap and garbage collection. will doc later
 * 
 * model type: 
 */

#ifndef HEAP_H
#define HEAP_H


// I HAVE TWO OPTIONS FOR EXPOSING POINTERS TO OBJECTS ON THE HEAP.
// VIRTUAL POINTERS - store pointers to a specific index in a list, do pointer arithmetic. could pointer pack (or cap @ 4b vals on the heap)
// RAW POINTERS - store pointers to the actual raw object in memory. then its just a reference, though i would want to add guards which could cost cycles.

typedef enum {
    IDLE = 0, // standard gc state, nothing happening.
    MARK,     // mark directly accesible objects (not necessary to go thru heap)
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

// rtti for objects on the heap (figure this out)
typedef struct ObjInfo ObjInfo;

// object helpers
typedef struct {
    u8 type;
    u8 color;    // white black gray (gc markings)
    u16 fieldc;

    // TODO: add methods
    // Method* methods
} Obj; // dunno how to do this yet

typedef struct {
    size_t charc;  // c length - 1 to exclude null term
    char* value[]; // string content
} ObjStr;

typedef struct {
    size_t itemc;  // how many items are in the array
    void* value[]; // whatever content the type has
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