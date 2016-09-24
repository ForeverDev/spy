#ifndef API_H
#define API_H

#include "spyre.h"

void SpyL_initializeStandardLibrary(SpyState*);

/* stdio */
static uint32_t SpyL_println(SpyState*);
static uint32_t SpyL_print(SpyState*);

/* file system */
static uint32_t SpyL_fopen(SpyState*);
static uint32_t SpyL_fclose(SpyState*);
static uint32_t SpyL_fputc(SpyState*);
static uint32_t SpyL_fputs(SpyState*);
static uint32_t SpyL_fprintf(SpyState*);
static uint32_t SpyL_fgetc(SpyState*);
static uint32_t SpyL_fread(SpyState*);
static uint32_t SpyL_ftell(SpyState*);
static uint32_t SpyL_fseek(SpyState*);

/* memory management */
uint32_t		SpyL_malloc(SpyState*);
static uint32_t SpyL_free(SpyState*);
static uint32_t	SpyL_exit(SpyState*);

/* math */
static uint32_t SpyL_max(SpyState*);
static uint32_t SpyL_min(SpyState*);
static uint32_t SpyL_map(SpyState*);

#endif
