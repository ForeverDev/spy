#ifndef SPYRE_H
#define SPYRE_H

#include <stddef.h>
#include <stdint.h>

/* option flags */
#define SPY_NOFLAG	0x00
#define SPY_DEBUG	0x01
#define SPY_STEP	0x02

/* runtime flags */
#define SPY_CMPRESULT 0x01

/* constants */
#define SIZE_MEMORY 0x500000
#define SIZE_STACK	0x100000
#define SIZE_ROM	0x100000
#define SIZE_PAGE	8

#define START_ROM	0
#define START_STACK	(SIZE_ROM)
#define START_HEAP	(SIZE_ROM + SIZE_STACK)

typedef struct SpyState SpyState;
typedef struct SpyCFunction SpyCFunction;
typedef struct SpyMemoryChunk SpyMemoryChunk;


struct SpyCFunction {
	const char*		identifier;
	uint32_t		(*function)(SpyState*);
	SpyCFunction*	next;
};

struct SpyMemoryChunk {
	size_t			pages;
	uint8_t*		absolute_address;
	uint64_t		vm_address;
	SpyMemoryChunk*	next;
	SpyMemoryChunk*	prev;
};

struct SpyState {
	size_t			static_memory_size;
	uint8_t*		static_memory;
	uint8_t*		bytecode;
	uint8_t*		memory;
	const uint8_t*	ip;
	uint8_t*		sp;
	uint8_t*		bp;
	uint32_t		option_flags;
	uint32_t		runtime_flags;
	SpyCFunction*	c_functions;
	SpyMemoryChunk*	memory_chunks;
};

SpyState*	Spy_newState(uint32_t);
void		Spy_log(SpyState*, const char*, ...);
void		Spy_crash(SpyState*, const char*, ...);
void		Spy_dumpStack(SpyState*);
void		Spy_dumpHeap(SpyState*);

void		Spy_pushInt(SpyState*, int64_t);
int64_t 	Spy_popInt(SpyState*);
void		Spy_saveInt(SpyState*, uint8_t*, int64_t);
void		Spy_saveFloat(SpyState*, uint8_t*, double);
uint64_t	Spy_readInt32(SpyState*);
uint64_t	Spy_readInt64(SpyState*);

void		Spy_pushPointer(SpyState*, void*);
void*		Spy_popPointer(SpyState*);

void		Spy_pushFloat(SpyState*, double);
double		Spy_popFloat(SpyState*);
double		Spy_readFloat(SpyState*);

void		Spy_pushString(SpyState*, const char*);
char*		Spy_popString(SpyState*);

uint8_t*	Spy_popRaw(SpyState*);

void		Spy_pushC(SpyState*, const char*, uint32_t (*)(SpyState*));
void		Spy_execute(const char*, uint32_t, int, char**);

#endif
