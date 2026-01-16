if anyone has recommendations feel free to hit me up @ [noah@noahmingolel.li](mailto:noah@noahmingolel.li) like it 1996.

asap:
- just write the fucking parser. (deciding on a parser comb or writing one hurts me. i dont think i wanna do a parser gen cuz the combs seem to push butter but gens reduce overhead. LALR or pratt hand written would potentially push faster tho...)
- get started on the emitter, make it do basic register ops and math
- sleep. its 4:35 u have class.
- opcodes:
    - SAR
    - TAILCALL (duh its listed 3 times its just cool to me)
    - NEWARR, NEWTABLE, NEWOBJ (when heap done, maybe add a raw ALLOC too)
    - GETELEM, SETELEM (hashtable ops)
    - ARRGET, ARRSET, ARRLEN (array ops)
    - CONCAT, STRLEN (string ops)

decisions:
- decide on heap model (bump allocator: requires less frees, and best for generational or free list: you can free whenever you want, whenever you alloc more you mark the last word as a pointer to the next areas first word)
- decide on gc model (likely tricolor generational, mark generations black white or gray and sift during the trace if still needed prove it white, if dropped mark black and next gc cycle sweep)
- decide on lookup model (bitmap seems fine, but i need to know the max U, so prob hashmap)
- decide on header size (16 bytes with a 40 byte per type header seems fine but i need to logic that too could get it down)
- decide on another potential Value conversion (do i want to suck up the padding after all that fucking work to store some shit in the remaining 7 bytes)
- decide on object layout (already mostly done, just want to finalize some things)
- decide on calling convention (still need to finalize. as of rn frames slot their args in the front registers, i could dedicate some, and i havent tested c native calling but it is pretty basic so should work. *out might contain pointer structures tho so i have to figure THAT out too... prolly the 2nd most complex besides header/heap and the most detail intensive)
- decide on numeric types (how will i offer <64 bit ints, VM level or compiler level)
- decide on error model (panic codes are runtime errors but i could just store a char* to a message and ref THAT. i think that may even be less overhead than dealing w it in vm_panic though i need to confirm no UAF/double free)
- double check frame for opts (may jus stall. they work fine as is.)

implementations (everything i can think of):
- maybe include vs import? i mean if i have a preprocessor and force compilation of every lib like most langs do nah, but my bytecode is portable i could allow deps, both ones that need to be bundled in and compiled (then can be used w a module system) and ones that r already compiled and can directly be used w wtv module system
- tail call (figure out how to reuse a frame, was thinking at the start taking a frame "snapshot" when tailcall detected, but that requires a copy and free every restart and thats expensive)
- vm dispatch loop (finalize some things to reduce cycle count and mem footprint. a lot of memcpys due to 9 byte vals in const/glob pool. might fucking plow thru all my work)
- closures/upvalues/coroutines (scope escaping methods, i dont entirely understand them but we'll learn! we always do!)
- native/ffi hooks (already have the basic callables just need to figure out how to call from the bytecode, and if the C function is stored in there and compiled on first run or if it comes from lib, which is dealt with at compile time)
- string interning (simple. just if a string is alr on the heap and you make another instance of it in a loop/a different instance uses the same value we use the interned string)