#include <stdio.h>
#include <stdlib.h>
#include "api.h"

void SpyL_initializeStandardLibrary(SpyState* S) {
	Spy_pushC(S, "println", SpyL_println);
	Spy_pushC(S, "print", SpyL_print);

	Spy_pushC(S, "fopen", SpyL_fopen);
	Spy_pushC(S, "fclose", SpyL_fclose);
	Spy_pushC(S, "fputc", SpyL_fputc);
	Spy_pushC(S, "fputs", SpyL_fputs);
	Spy_pushC(S, "fgetc", SpyL_fgetc);
	Spy_pushC(S, "fread", SpyL_fread);
	Spy_pushC(S, "ftell", SpyL_ftell);
	Spy_pushC(S, "fseek", SpyL_fseek);

	Spy_pushC(S, "malloc", SpyL_malloc);
	Spy_pushC(S, "free", SpyL_free);
	Spy_pushC(S, "exit", SpyL_exit);

	Spy_pushC(S, "min", SpyL_min);
	Spy_pushC(S, "max", SpyL_max);
}

static uint32_t
SpyL_println(SpyState* S) {
	SpyL_print(S);
	fputc('\n', stdout);
	return 0;
}

static uint32_t
SpyL_print(SpyState* S) {
	const char* format = Spy_popString(S);
	while (*format) {
		switch (*format) {
			case '%':
				switch (*++format) {
					case 's':
						fputs(Spy_popString(S), stdout);
						break;
					case 'd':
						printf("%lld", Spy_popInt(S));
						break;
					case 'x':
						printf("%llX", Spy_popInt(S));
						break;
					case 'p':
						printf("0x%lX", (uintptr_t)Spy_popPointer(S));
						break;
					case 'f':
						printf("%f", Spy_popFloat(S));
						break;
					case 'c':
						printf("%c", (char)Spy_popInt(S));
						break;
				}
				break;
			case '\\':
				switch (*++format) {
					case 'n': fputc('\n', stdout); break;
					case 't': fputc('\t', stdout); break;
					case '\\': fputc('\\', stdout); break;
				}
			default:
				fputc(*format, stdout);
		}
		format++;
	}
	return 0;
}

static uint32_t
SpyL_fopen(SpyState* S) {
	const char* filename = Spy_popString(S);
	const char* mode = Spy_popString(S);
	Spy_pushPointer(S, fopen(filename, mode));
	return 1;
}

static uint32_t
SpyL_fclose(SpyState* S) {
	fclose((FILE *)Spy_popPointer(S));
	return 0;
}

/* note called as fputc(FILE*, char) */
static uint32_t
SpyL_fputc(SpyState* S) {
	FILE* f = (FILE *)Spy_popPointer(S);
	fputc((char)Spy_popInt(S), f);
	return 0;
}

/* note called as fputs(FILE*, char*) */
static uint32_t
SpyL_fputs(SpyState* S) {
	FILE* f = (FILE *)Spy_popPointer(S);
	fputs(Spy_popString(S), f);
	return 0;	
}

/* note called as fprintf(FILE*, char*, ...) */
static uint32_t
SpyL_fprintf(SpyState* S) {
	FILE* f = (FILE *)Spy_popPointer(S);
	const char* format = Spy_popString(S);
	return 0;
}

static uint32_t
SpyL_fgetc(SpyState* S) {
	Spy_pushInt(S, fgetc((FILE *)Spy_popPointer(S)));
	return 1;
}

/* note called as fread(FILE*, void*, int) */
static uint32_t
SpyL_fread(SpyState* S) {
	FILE* f = (FILE *)Spy_popPointer(S);
	void* dest = &S->memory[Spy_popInt(S)];
	uint64_t bytes = Spy_popInt(S);
	fread(dest, 1, bytes, f);
	return 0;
}

static uint32_t
SpyL_ftell(SpyState* S) {
	Spy_pushInt(S, ftell((FILE *)Spy_popPointer(S)));
	return 1;
}

/* note called as fseek(FILE*, int mode, int offset) */
static uint32_t
SpyL_fseek(SpyState* S) {
	FILE* f = Spy_popPointer(S);
	uint64_t mode = Spy_popInt(S);
	uint64_t offset = Spy_popInt(S);
	fseek(f, offset, (mode == 1 ? SEEK_SET : SEEK_END));
	return 0;
}

uint32_t
SpyL_malloc(SpyState* S) {
	SpyMemoryChunk* chunk = (SpyMemoryChunk *)malloc(sizeof(SpyMemoryChunk));
	if (!chunk) Spy_crash(S, "Out of memory\n");
	uint64_t size = Spy_popInt(S);

	/* round chunk->size up to nearest SIZE_PAGE multiple */
	if (size == 0) chunk->pages = 1;
	else if (size % SIZE_PAGE > 0) {
		chunk->pages = (size + (SIZE_PAGE - (size % SIZE_PAGE))) / SIZE_PAGE;
	} else {
		chunk->pages = size / SIZE_PAGE;	
	}
	
	/* find an open memory slot */
	if (!S->memory_chunks) {
		S->memory_chunks = chunk;
		chunk->next = NULL;
		chunk->prev = NULL;
		chunk->absolute_address = &S->memory[START_HEAP];
		chunk->vm_address = START_HEAP;
	} else {
		SpyMemoryChunk* at = S->memory_chunks;
		uint8_t found_slot = 0;
		while (at->next) {
			uint64_t offset = at->pages * SIZE_PAGE;
			uint8_t* abs_addr = at->absolute_address + offset;
			uint64_t page_distance = (at->next->absolute_address - abs_addr) / SIZE_PAGE;
			if (page_distance >= chunk->pages) {
				chunk->absolute_address = abs_addr;
				chunk->vm_address = at->vm_address + offset;
				chunk->next = at->next;
				chunk->prev = at;
				at->next->prev = chunk;
				at->next = chunk;
				found_slot = 1;
				break;
			}
			at = at->next;
		}
		if (!found_slot) {
			chunk->absolute_address = at->absolute_address + at->pages * SIZE_PAGE;
			chunk->vm_address = at->vm_address + at->pages * SIZE_PAGE;
			chunk->next = NULL;
			chunk->prev = at;
			at->next = chunk;
		}
	}

	Spy_pushInt(S, chunk->vm_address);

	return 0;
}

static uint32_t
SpyL_free(SpyState* S) {
	static const char* errmsg = "Attempt to free an invalid pointer (0x%x)";
	uint8_t success = 0;
	uint64_t vm_address = Spy_popInt(S);
	SpyMemoryChunk* at = S->memory_chunks;
	if (!at) Spy_crash(S, errmsg, vm_address);
	while (at) {
		if (at->vm_address == vm_address) {
			if (at->next) {
				at->prev->next = at->next;
				at->next->prev = at->prev;
			} else {
				at->prev->next = NULL;
			}
			free(at);
			success = 1;
		}
		at = at->next; 
	}
	if (!success) {
		Spy_crash(S, errmsg, vm_address);
	}
	return 0;
}

static uint32_t
SpyL_exit(SpyState* S) {
	exit(0);
	return 0;
}

static uint32_t
SpyL_min(SpyState* S) {
	int64_t a, b;
	a = Spy_popInt(S);
	b = Spy_popInt(S);
	Spy_pushInt(S, a < b ? a : b);
	return 1;
}

static uint32_t
SpyL_max(SpyState* S) {
	int64_t a, b;
	a = Spy_popInt(S);
	b = Spy_popInt(S);
	Spy_pushInt(S, a > b ? a : b);
	return 1;
}
