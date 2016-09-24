#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "spyre.h"
#include "api.h"
#include "assembler.h"

SpyState*
Spy_newState(uint32_t option_flags) {
	SpyState* S = (SpyState *)malloc(sizeof(SpyState));
	S->memory = (uint8_t *)calloc(1, SIZE_MEMORY);
	S->ip = NULL; /* to be assigned when code is executed */
	S->sp = &S->memory[START_STACK - 1]; /* stack grows upwards */
	S->bp = &S->memory[START_STACK - 1];
	S->option_flags = option_flags;
	S->runtime_flags = 0;
	S->c_functions = NULL;
	S->memory_chunks = NULL;
	SpyL_initializeStandardLibrary(S);
	return S;
}

void 
Spy_log(SpyState* S, const char* format, ...) {
	if (!(S->option_flags | SPY_DEBUG)) return;
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
}

void
Spy_crash(SpyState* S, const char* format, ...) {
	printf("SPYRE RUNTIME ERROR: ");
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
	putc('\n', stdout);
	exit(1);
}

inline void
Spy_pushInt(SpyState* S, int64_t value) {
	S->sp += 8;
	*(int64_t *)S->sp = value;
}

inline uint64_t
Spy_readInt32(SpyState* S) {
	S->ip += 4;
	return (uint64_t)(*(uint32_t *)(S->ip - 4));
}

inline uint64_t
Spy_readInt64(SpyState* S) {
	S->ip += 8;
	return *(uint64_t *)(S->ip - 8);
}

inline int64_t
Spy_popInt(SpyState* S) {
	int64_t result = *(int64_t *)S->sp;
	S->sp -= 8;
	return result;
}

inline void
Spy_saveInt(SpyState* S, uint8_t* addr, int64_t value) {
	*(int64_t *)addr = value;
}

inline void
Spy_saveFloat(SpyState* S, uint8_t* addr, double value) {
	*(double *)addr = value;
}

inline uint8_t*
Spy_popRaw(SpyState* S) {
	S->sp -= 8;
	return S->sp + 8;
}

inline void
Spy_pushPointer(SpyState* S, void* ptr) {
	S->sp += 8;
	*(int64_t *)S->sp = (uintptr_t)ptr;
}

inline void*
Spy_popPointer(SpyState* S) {
	void* result = (void *)(*(uintptr_t *)S->sp);
	S->sp -= 8;
	return result;
}

inline void
Spy_pushFloat(SpyState* S, double value) {
	S->sp += 8;
	*(double *)S->sp = value;
}

inline double
Spy_readFloat(SpyState* S) {
	S->ip += 8;
	return *(double *)(S->ip - 8);
}

inline double
Spy_popFloat(SpyState* S) {
	double result = *(double *)S->sp;
	S->sp -= 8;
	return result;
}

inline void
Spy_pushString(SpyState* S, const char* str) {
	while (*str) {
		Spy_pushInt(S, (int64_t)*str++);
	}
}

inline char*
Spy_popString(SpyState* S) {
	return (char *)&S->memory[Spy_popInt(S)];
}

void
Spy_dumpStack(SpyState* S) {
	for (const uint8_t* i = &S->memory[SIZE_ROM] + 2; i <= S->sp + 7; i++) {
		printf("0x%08lx: %02x | %c | ", i - S->memory, *i, isprint(*i) ? *i : '.');	
		if ((&S->memory[SIZE_ROM] - i + 1) % 8 == 0) {
			fputc('\n', stdout);
			for (int j = 0; j < 24; j++) {
				fputc('-', stdout);
			}
		}
		fputc('\n', stdout);
	}
}

void
Spy_dumpHeap(SpyState* S) {
	SpyMemoryChunk* at = S->memory_chunks;
	int index = 0;
	while (at) {
		printf("chunk %d:\n\t%zu pages\n\t%lu bytes\n\t", index, at->pages, at->pages * SIZE_PAGE);
		int filled = 0;
		for (int i = 0; i < at->pages * SIZE_PAGE; i++) {
			if (at->absolute_address[i]) {
				filled++;
			}
		}
		printf("%lu%% non-zero\n\tvm address: 0x%llX\n\t", (100 * filled) / (at->pages * SIZE_PAGE), at->vm_address);
		printf("absolute address: 0x%lX\n", (uintptr_t)at->absolute_address);
		at = at->next;
		index++;
	}
}

void
Spy_pushC(SpyState* S, const char* identifier, uint32_t (*function)(SpyState*)) {
	SpyCFunction* container = (SpyCFunction *)malloc(sizeof(SpyCFunction));
	container->identifier = identifier;
	container->function = function;
	container->next = NULL;
	if (!S->c_functions) {
		S->c_functions = container;
	} else {
		SpyCFunction* at = S->c_functions;
		while (at->next) at = at->next;
		at->next = container;
	}
}

void
Spy_execute(const char* filename, uint32_t option_flags, int argc, char** argv) {

	SpyState S;

	S.memory = (uint8_t *)calloc(1, SIZE_MEMORY);
	if (!S.memory) {
		Spy_crash(&S, "couldn't allocate memory\n");
	}
	S.ip = NULL; /* to be assigned when code is executed */
	S.sp = &S.memory[START_STACK + 2]; /* stack grows upwards */
	S.bp = &S.memory[START_STACK + 2];
	S.option_flags = option_flags;
	S.runtime_flags = 0;
	S.c_functions = NULL;
	S.memory_chunks = NULL;
	SpyL_initializeStandardLibrary(&S);

	FILE* f;
	unsigned long long flen;
	uint32_t code_start;
	f = fopen(filename, "rb");
	if (!f) Spy_crash(&S, "Couldn't open input file '%s'", filename);
	fseek(f, 0, SEEK_END);
	flen = ftell(f);
	fseek(f, 0, SEEK_SET);
	S.bytecode = (uint8_t *)malloc(flen + 1);
	fread(S.bytecode, 1, flen, f);
	S.bytecode[flen] = 0;
	fclose(f);

	for (int i = 12; i < *(uint32_t *)&S.bytecode[8]; i++) {
		S.memory[i - 12] = S.bytecode[i];
	}

	/* prepare instruction pointer, point it to code */	
	S.bytecode = &S.bytecode[*(uint32_t *)&S.bytecode[8]];
	S.ip = S.bytecode;

	/* push command line arguments */
	for (int i = argc - 1; i >= 0; i--) {
		Spy_pushInt(&S, strlen(argv[i]));
		SpyL_malloc(&S);
		/* allocated space for the string, now find tail of malloc blocks */
		SpyMemoryChunk* chunk = S.memory_chunks;
		while (chunk->next) chunk = chunk->next;
		strcpy((char *)chunk->absolute_address, argv[i]);
	}

	/* push ng */
	Spy_pushInt(&S, argc);

	/* push junk for ng, ip, and bp onto the stack to maintain alignment for arg instruction */
	Spy_pushInt(&S, 0x7369DB6469766164);
	Spy_pushInt(&S, 0xDB6C6F6F63DB61DB);
	Spy_pushInt(&S, 0x212121212164696B);
	/* assign BP to SP to simulate a function call */
	S.bp = S.sp;

	/* general purpose vars for interpretation */
	int64_t a, c;
	double b;
	uint8_t *pa, *pb;

	/* IP saver */
	uint8_t ipsave = 0;

	/* pointers to labels, (direct threading, significantly faster than switch/case) */
	static const void* opcodes[] = {
		&&noop, &&ipush, &&iadd, &&isub,
		&&imul, &&idiv, &&mod, &&shl, 
		&&shr, &&and, &&or, &&xor, &&not,
		&&neg, &&igt, &&ige, &&ilt,
		&&ile, &&icmp, &&jnz, &&jz,
		&&jmp, &&call, &&iret, &&ccall,
		&&fpush, &&fadd, &&fsub, &&fmul,
		&&fdiv, &&fgt, &&fge, &&flt, 
		&&fle, &&fcmp, &&fret, &&ilload,
		&&ilsave, &&iarg, &&iload, &&isave,
		&&res, &&ilea, &&ider, &&icinc, &&cder,
		&&lor, &&land, &&padd, &&psub, &&log,
		&&vret, &&dbon, &&dboff, &&dbds, &&cjnz,
		&&cjz, &&cjmp, &&ilnsave, &&ilnload,
		&&flload, &&flsave, &&ftoi, &&itof
	};

	int total = 0;

	/* main interpreter loop */
	dispatch:
	total++;
	if (S.sp >= &S.memory[START_HEAP]) {
		Spy_crash(&S, "stack overflow");
	}
	if (option_flags & SPY_STEP && option_flags & SPY_DEBUG) {
		for (int i = 0; i < 100; i++) {
			fputc('\n', stdout);
		}
		Spy_dumpStack(&S);
		printf("\nexecuted %s\n", instructions[ipsave].name);
		getchar();
	}
	ipsave = *S.ip;
	goto *opcodes[*S.ip++];

	noop:
	goto done;
	
	ipush:
	Spy_pushInt(&S, Spy_readInt64(&S));
	goto dispatch;

	iadd:
	Spy_pushInt(&S, Spy_popInt(&S) + Spy_popInt(&S));
	goto dispatch;

	isub:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) - a);
	goto dispatch;

	imul:
	Spy_pushInt(&S, Spy_popInt(&S) * Spy_popInt(&S));
	goto dispatch;

	idiv:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) / a);
	goto dispatch;

	mod:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) % a);
	goto dispatch;

	shl:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) << a);
	goto dispatch;
	
	shr:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) >> a);
	goto dispatch;

	and:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) & a);
	goto dispatch;

	or:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) | a);
	goto dispatch;

	xor:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) ^ a);
	goto dispatch;

	not:
	Spy_pushInt(&S, ~Spy_popInt(&S));
	goto dispatch;

	neg:
	Spy_pushInt(&S, -Spy_popInt(&S));
	goto dispatch;

	igt:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) > a);
	goto dispatch;

	ige:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) >= a);
	goto dispatch;

	ilt:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) < a);
	goto dispatch;

	ile:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) <= a);
	goto dispatch;

	icmp:
	Spy_pushInt(&S, Spy_popInt(&S) == Spy_popInt(&S));
	goto dispatch;

	jnz:
	a = Spy_readInt32(&S);
	if (Spy_popInt(&S)) {
		S.ip = (uint8_t *)&S.bytecode[a];
	}
	goto dispatch;

	jz:
	a = Spy_readInt32(&S);
	if (!Spy_popInt(&S)) {
		S.ip = (uint8_t *)&S.bytecode[a];
	}
	goto dispatch;

	jmp:
	S.ip = (uint8_t *)&S.bytecode[Spy_readInt32(&S)];
	goto dispatch;

	call:
	{
		a = Spy_readInt32(&S);
		uint32_t num_args = Spy_readInt32(&S);
		int64_t* pops = malloc(num_args * 8);
		/* flip the arguments */
		for (int i = 0; i < num_args; i++) {
			pops[i] = *(int64_t *)Spy_popRaw(&S);
		}
		for (int i = 0; i < num_args; i++) {
			Spy_pushInt(&S, pops[i]);
		}
		free(pops);
		Spy_pushInt(&S, num_args); /* push number of arguments */
		Spy_pushPointer(&S, (void *)S.bp); /* push base pointer */
		Spy_pushPointer(&S, (void *)S.ip); /* push return address */
		S.bp = S.sp;
		S.ip = (uint8_t *)&S.bytecode[a];
	}
	goto dispatch;

	iret:
	a = Spy_popInt(&S); /* return value */
	S.sp = S.bp;
	S.ip = (uint8_t *)Spy_popPointer(&S);	
	S.bp = (uint8_t *)Spy_popPointer(&S);
	S.sp -= Spy_popInt(&S) * 8;
	Spy_pushInt(&S, a);
	goto dispatch;	

	ccall:
	{
		uint32_t name_index = Spy_readInt32(&S);
		uint32_t num_args = Spy_readInt32(&S);
		int64_t* pops = malloc(num_args * 8);
		/* flip the arguments */
		for (int i = 0; i < num_args; i++) {
			pops[i] = *(int64_t *)Spy_popRaw(&S);
		}
		for (int i = 0; i < num_args; i++) {
			Spy_pushInt(&S, pops[i]);
		}
		free(pops);
		SpyCFunction* cf = S.c_functions;
		while (cf && strcmp(cf->identifier, (const char *)&S.memory[name_index])) cf = cf->next;
		if (!cf) {
			Spy_crash(&S, "Attempt to call undefined C function '%s'\n", &S.memory[name_index]);
		}
		cf->function(&S);
	}
	goto dispatch;
		
	fpush:
	Spy_pushFloat(&S, Spy_readFloat(&S));
	goto dispatch;

	fadd:
	Spy_pushFloat(&S, Spy_popFloat(&S) + Spy_popFloat(&S));
	goto dispatch;

	fsub:
	b = Spy_popFloat(&S);
	Spy_pushFloat(&S, Spy_popFloat(&S) - b);
	goto dispatch;

	fmul:
	Spy_pushFloat(&S, Spy_popFloat(&S) * Spy_popFloat(&S));
	goto dispatch;

	fdiv:
	b = Spy_popFloat(&S);
	Spy_pushFloat(&S, Spy_popFloat(&S) / b);
	goto dispatch;

	fgt:
	b = Spy_popFloat(&S);
	Spy_pushFloat(&S, Spy_popFloat(&S) > b);
	goto dispatch;

	fge:
	b = Spy_popFloat(&S);
	Spy_pushFloat(&S, Spy_popFloat(&S) >= b);
	goto dispatch;

	flt:
	b = Spy_popFloat(&S);
	Spy_pushFloat(&S, Spy_popFloat(&S) < b);
	goto dispatch;

	fle:
	b = Spy_popFloat(&S);
	Spy_pushFloat(&S, Spy_popFloat(&S) <= b);
	goto dispatch;

	fcmp:
	Spy_pushInt(&S, Spy_popFloat(&S) == Spy_popFloat(&S));
	goto dispatch;

	fret:
	b = Spy_popFloat(&S); /* return value */
	S.sp = S.bp;
	S.ip = (uint8_t *)Spy_popPointer(&S);	
	S.bp = (uint8_t *)Spy_popPointer(&S);
	S.sp -= Spy_popInt(&S);
	Spy_pushFloat(&S, a);
	goto dispatch;	

	ilload:
	Spy_pushInt(&S, *(int64_t *)&S.bp[Spy_readInt32(&S)*8 + 8]);
	goto dispatch;

	ilsave:
	Spy_saveInt(&S, &S.bp[Spy_readInt32(&S)*8 + 8], Spy_popInt(&S));
	goto dispatch;

	iarg:
	Spy_pushInt(&S, *(int64_t *)&S.bp[-3*8 - Spy_readInt32(&S)*8]);
	goto dispatch;

	iload:
	Spy_pushInt(&S, *(int64_t *)&S.memory[(uint64_t)Spy_popInt(&S)]);
	goto dispatch;

	isave:
	a = Spy_popInt(&S); /* pop value */
	Spy_saveInt(&S, &S.memory[Spy_popInt(&S)], a);
	goto dispatch;

	res:
	S.sp += Spy_readInt32(&S) * 8;
	goto dispatch;

	ilea:
	Spy_pushPointer(&S, (void *)(&S.bp[Spy_readInt32(&S)*8 + 8] - S.memory));
	goto dispatch;

	ider:
	Spy_pushInt(&S, *(uint64_t *)&S.memory[Spy_popInt(&S)]);
	goto dispatch;

	icinc:
	Spy_pushInt(&S, Spy_popInt(&S) + Spy_readInt64(&S));
	goto dispatch;

	cder:
	Spy_pushInt(&S, *(uint8_t *)&S.memory[Spy_popInt(&S)]);
	goto dispatch;
	
	lor:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) || a);
	goto dispatch;

	land:
	a = Spy_popInt(&S);
	Spy_pushInt(&S, Spy_popInt(&S) && a);
	goto dispatch;

	padd:
	a = Spy_popInt(&S) * 8;
	Spy_pushInt(&S, Spy_popInt(&S) + a);
	goto dispatch;

	psub:
	a = Spy_popInt(&S) * 8;
	Spy_pushInt(&S, Spy_popInt(&S) - a);
	goto dispatch;

	log:
	printf("%llu\n", Spy_readInt32(&S));
	goto dispatch;

	vret:
	S.sp = S.bp;
	S.ip = (uint8_t *)Spy_popPointer(&S);	
	S.bp = (uint8_t *)Spy_popPointer(&S);
	S.sp -= Spy_popInt(&S) * 8;
	goto dispatch;

	dbon:
	option_flags |= (SPY_DEBUG | SPY_STEP);
	goto dispatch;

	dboff:
	option_flags &= ~SPY_DEBUG;
	option_flags &= ~SPY_STEP;
	goto dispatch;

	dbds:
	Spy_dumpStack(&S);
	goto dispatch;

	cjnz:
	a = Spy_popInt(&S); /* location */
	c = Spy_popInt(&S); /* condition */
	if (c) {
		S.ip = (uint8_t *)&S.bytecode[a];
	}
	goto dispatch;

	cjz:
	a = Spy_popInt(&S); /* location */
	c = Spy_popInt(&S); /* condition */
	if (!c) {
		S.ip = (uint8_t *)&S.bytecode[a];
	}
	goto dispatch;

	cjmp:
	S.ip = (uint8_t *)&S.bytecode[Spy_popInt(&S)];
	goto dispatch;

	ilnsave:
	{
		uint32_t addr = Spy_readInt32(&S);
		uint32_t numsave = Spy_readInt32(&S);
		uint64_t* pops = (uint64_t *)malloc(numsave * 8);
		for (int i = numsave - 1; i >= 0; i--) {
			pops[i] = Spy_popInt(&S);
		}
		memcpy(&S.bp[addr*8 + 8], pops, numsave * 8);
		free(pops);
	}
	goto dispatch;

	ilnload:
	goto dispatch;

	flload:
	Spy_pushFloat(&S, *(double *)&S.bp[Spy_readInt32(&S)*8 + 8]);
	goto dispatch;

	flsave:
	Spy_saveFloat(&S, &S.bp[Spy_readInt32(&S)*8 + 8], Spy_popFloat(&S));
	goto dispatch;

	/* ***NOTE*** THIS ADDRESSES OFF THE TOP OF THE STACK */
	ftoi:
	a = Spy_readInt32(&S);
	Spy_saveInt(&S, &S.sp[-a*8], (int64_t)(*(double *)&S.sp[-a*8]));
	goto dispatch;
	
	/* ***NOTE*** THIS ADDRESSES OFF THE TOP OF THE STACK */
	itof:
	a = Spy_readInt32(&S);
	Spy_saveFloat(&S, &S.sp[-a*8], (double)(*(int64_t *)&S.sp[-a*8]));
	goto dispatch;

	done:
	if (option_flags & SPY_DEBUG) {
		printf("\nSpyre process terminated\n");
		printf("%d instructions were executed\n", total);
	}

	return;

}

