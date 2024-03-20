/**
 * @author wwotz (wwotz12@gmail.com)
 * @file cyaml.h
 * @description A single header yaml parser.
 * @instructions add '#define CYAML_IMPLEMENTATION' to the
 * start of the file that you want to include the source into
 * like this:
 *
 * `#define CYAML_IMPLEMENTATION
 *  #include "cyaml.h"`
 *
 */

#ifndef CYAML_H_
#define CYAML_H_

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef CYAMLDEF
#ifdef CYAMLSTATIC
#define CYAMLDEF static
#else /* !defined(CYAMLSTATIC) */
#define CYAMLDEF extern
#endif /* CYAMLSTATIC */
#endif /* CYAMLDEF */

#if !defined(CYAML_MALLOC) || !defined(CYAML_REALLOC) || !defined(CYAML_CALLOC) || !defined(CYAML_FREE)
#define CYAML_MALLOC(sz) malloc(sz)
#define CYAML_REALLOC(x, newsz) realloc(x, newsz)
#define CYAML_CALLOC(nmemb, size) calloc(nmemb, size)
#define CYAML_FREE(x) free(x)
#endif

#define CYAML_LOG_MESSAGE_CAPACITY (256) /* maximum capacity of a message reported by cyaml */
#define CYAML_LOG_STACK_CAPACITY   (20)  /* maximum life span of a message in the cyaml logging
					  *  system */

typedef struct cyaml_t {
	size_t size;
	size_t capacity;
	struct cyaml_dict_t *data;
} cyaml_t;

typedef struct cyaml_trie_t {
	char character;
	struct cyaml_trie_t *left;
	struct cyaml_trie_t *children;
	struct cyaml_trie_t *right;

	size_t size;
	size_t capacity;
	struct cyaml_cyaml_t *lists;
} cyaml_trie_t;

typedef struct cyaml_dict_t {
	enum cyaml_storage_type {
		CYAML_STORAGE_LIST,
		CYAML_STORAGE_MAPPING
	} type;

	union {
		struct cyaml_t *keys;
		struct trie_t *list;
	} storage;
} cyaml_dict_t;

typedef enum cyaml_loc_t {
	CYAML_LOC_MEMORY,
	CYAML_LOC_DISK
} cyaml_loc_t;

CYAMLDEF cyaml_t *
cyaml_parse(char *s, size_t n, cyaml_loc_t loc);

CYAMLDEF cyaml_t *
cyaml_lookup(cyaml_t *cyaml, char *path);

CYAMLDEF void
cyaml_free(cyaml_t *cyaml);

#ifdef CYAML_IMPLEMENTATION

/**
 * @Internal: The logging stack, used to store messages in a cyclical
 * way such that we can store many messages, but old ones that have
 * not been checked are overwritten with newer error messages.
 */
static char cyaml_log_stack[CYAML_LOG_STACK_CAPACITY][CYAML_LOG_MESSAGE_CAPACITY];
static size_t cyaml_log_stack_ptr;
static size_t cyaml_log_stack_size;

#define CYAML_LOG_MESSAGE(fmt, ...)				\
	cyaml_log_message("%s: " fmt __VA_OPT__(,) __VA_ARGS__)

static inline int
cyaml_log_stack_empty(void)
{
	return cyaml_log_stack_size == 0;
}

static inline int
cyaml_log_stack_full(void)
{
	return cyaml_log_stack_size == CYAML_LOG_STACK_CAPACITY;
}

static void
cyaml_log_message(const char *fmt, ...)
{
	va_list args;
	if (!cyaml_log_stack_full()) {
		cyaml_log_stack_size++;
	}

	va_start(args, fmt);
	memset(cyaml_log_stack + cyaml_log_stack_ptr, 0,
	       sizeof(cyaml_log_stack[cyaml_log_stack_ptr]));
	vsnprintf((char *) (cyaml_log_stack + cyaml_log_stack_ptr),
		  CYAML_LOG_MESSAGE_CAPACITY - 1, fmt, args);
	va_end(args);
	cyaml_log_stack_ptr = (cyaml_log_stack_ptr + 1) % CYAML_LOG_STACK_CAPACITY;
}

CYAMLDEF const char *
cyaml_error_pop(void)
{
	if (!cyaml_log_stack_empty()) {
		cyaml_log_stack_size--;
		cyaml_log_stack_ptr--;
		if (cyaml_log_stack_ptr < 0)
			cyaml_log_stack_ptr += CYAML_LOG_STACK_CAPACITY;
		return (const char *) cyaml_log_stack[cyaml_log_stack_ptr];
	}
	return "No error.";
}


// need to tokenize the input before hand, makes parsing it far easier.
typedef struct cyaml_token_t {
	enum cyaml_token_type {
		CYAML_TOKEN_EMPTY,
		CYAML_TOKEN_COLON,
		CYAML_TOKEN_STRING,
		CYAML_TOKEN_SYMBOL,
		CYAML_TOKEN_INDENT,
		CYAML_TOKEN_UNDENT,
		CYAML_TOKEN_DASH,
		CYAML_TOKEN_END,
		CYAML_TOKEN_ERROR,
	} type;
	size_t len;
	char *data;
} cyaml_token_t;

static size_t peeked;
static cyaml_token_t peek_token;

#define CYAML_TOKEN_CREATE(type, len, data) ((cyaml_token_t) { type, len, data })
#define CYAML_EMPTY_CREATE() (CYAML_TOKEN_CREATE(CYAML_TOKEN_EMPTY, 0, NULL))
#define CYAML_COLON_CREATE() (CYAML_TOKEN_CREATE(CYAML_TOKEN_COLON, 0, NULL))
#define CYAML_STRING_CREATE(len, data) (CYAML_TOKEN_CREATE(CYAML_TOKEN_STRING, len, data))
#define CYAML_SYMBOL_CREATE(len, data) (CYAML_TOKEN_CREATE(CYAML_TOKEN_SYMBOL, len, data))
#define CYAML_INDENT_CREATE(len) (CYAML_TOKEN_CREATE(CYAML_TOKEN_INDENT, len, NULL))
#define CYAML_UNDENT_CREATE(len) (CYAML_TOKEN_CREATE(CYAML_TOKEN_UNDENT, len, NULL))
#define CYAML_DASH_CREATE() (CYAML_TOKEN_CREATE(CYAML_TOKEN_DASH, 0, NULL))
#define CYAML_END_CREATE() (CYAML_TOKEN_CREATE(CYAML_TOKEN_END, 0, NULL))
#define CYAML_ERROR_CREATE(len, data) (CYAML_TOKEN_CREATE(CYAML_TOKEN_ERROR, len, data))
#define CYAML_TOKEN_STRINGP(token) (token.type == CYAML_TOKEN_STRING)
#define CYAML_TOKEN_KEYP(token)    (token.type == CYAML_TOKEN_SYMBOL)
#define CYAML_TOKEN_VALUEP(token)  (token.type == CYAML_TOKEN_STRING || token.type == CYAML_TOKEN_SYMBOL)
#define CYAML_TOKEN_COLONP(token)  (token.type == CYAML_TOKEN_COLON)
#define CYAML_TOKEN_INDENTP(token) (token.type == CYAML_TOKEN_INDENT)
#define CYAML_TOKEN_UNDENTP(token) (token.type == CYAML_TOKEN_UNDENT)
#define CYAML_TOKEN_SPACEP(token) (token.type == CYAML_TOKEN_INDENT || token.type == CYAML_TOKEN_UNDENT)
#define CYAML_TOKEN_DASHP(token)   (token.type == CYAML_TOKEN_DASH)

static size_t indent_level;

static cyaml_token_t
cyaml_token_get(char **buffer)
{
	cyaml_token_t token;
	char *p = *buffer, *q;
	if (peeked) {
		peeked = 0;
		return peek_token;
	}
	
	if (*p == '\n') {
		p = p + strspn(p, "\n");
	}

	if (*p == '\0') {
		return CYAML_END_CREATE();
	}
	
	if (isspace(*p)) {
		q = p + strspn(p, " \t");
		size_t current_indent_level = (size_t) (q-p);
		if (current_indent_level == indent_level) {
			token = CYAML_EMPTY_CREATE();
		} else if (current_indent_level > indent_level) {
			token = CYAML_INDENT_CREATE(current_indent_level);
		} else {
			token = CYAML_UNDENT_CREATE(current_indent_level);
		}
		indent_level = current_indent_level;
		*buffer = q;
		return token;
	} else if (isalpha(*p)) {
		q = p + strcspn(p, " \t\n\":-");
		if (*q == '"') {
			char *error = "Invalid symbol!";
			return CYAML_ERROR_CREATE(strlen(error), error);
		}
		*buffer = q;
		return CYAML_SYMBOL_CREATE((size_t)(q-p), p);
	} else if (*p == '"') {
		p++;
		q = p;
		while (*q != '"' && *q != '\0') {
			q += strcspn(q, "\"");
			if (*q == '\0') break;
			if (q[-1] == '\\') {
				q++;
			}
		}

		if (*q == '\0') {
			char *error = "Unterminated string!";
			return CYAML_ERROR_CREATE(strlen(error), error);
		}

		*buffer = q + 1;
		return CYAML_STRING_CREATE((size_t)(q-p), p);
	} else if (*p == '-') {
		p++;
		*buffer = p;
		return CYAML_DASH_CREATE();
	} else if (*p == ':') {
		p++;
		*buffer = p;
		return CYAML_COLON_CREATE();
	}
	
	char *error = "Unrecognized token!";
	return CYAML_ERROR_CREATE(strlen(error), error);
}

static cyaml_token_t
cyaml_token_peek(char **buffer)
{
	char *p = *buffer;
	peek_token = cyaml_token_get(&p);
	peeked = 1;
	return peek_token;
}

static inline char *
cyaml_read_file(char *s, size_t n)
{
	FILE *fd;
	size_t fsize, nread;
	char fname[n+1], *buffer;
	snprintf(fname, n + 1, "%.*s", n, s);
	fname[n] = '\0';
	fd = fopen(fname, "r");
	if (!fd) {
		cyaml_log_message("Failed to open file!");
		return NULL;
	}

	fseek(fd, 0, SEEK_END);
	fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	if (fsize <= 0) {
		fclose(fd);
		cyaml_log_message("File was empty!");
		return NULL;
	}

	buffer = malloc(fsize + 1);
	if (!buffer) {
		fclose(fd);
		cyaml_log_message("Ran out of memory!");
		return NULL;
	}

	nread = fread(buffer, sizeof(*buffer), fsize, fd);
	if (nread != fsize) {
		free(buffer);
		fclose(fd);
		cyaml_log_message("Failed to read file!");
		return NULL;
	}

	buffer[nread] = '\0';
	fclose(fd);
	return buffer;
}

static cyaml_t *
cyaml_create(void)
{
	cyaml_t *storage;
	storage = malloc(sizeof(*storage));
	if (!storage) {
		cyaml_log_message("Ran out of memory!");
		return NULL;
	}
}

CYAMLDEF cyaml_t *
cyaml_parse(char *s, size_t n, cyaml_loc_t loc)
{
	cyaml_t *storage;
	char *buffer, *p;
	cyaml_token_t token, ptoken;
	char spaces[1024];
	if (s == NULL || n <= 0) {
		return NULL;
	}

	storage = cyaml_create();
	if (!storage) {
		
	}

	if (loc == CYAML_LOC_DISK) {
		buffer = cyaml_read_file(s, n);
	} else {
		buffer = strndup(s, n);
	}

	p = buffer;
	
	token = cyaml_token_get(&p);
	memset(spaces, ' ', sizeof(spaces));
	while (token.type != CYAML_TOKEN_ERROR && token.type != CYAML_TOKEN_END) {
		if (CYAML_TOKEN_KEYP(token)) {
			ptoken = cyaml_token_peek(&p);
			if (!CYAML_TOKEN_COLONP(ptoken)) {
			}
		}
		
		if (token.type == CYAML_TOKEN_INDENT) {
			printf("INDENT: '%.*s'\n", token.len, spaces);
		} else if (token.type == CYAML_TOKEN_UNDENT) {
			printf("UNDENT: '%.*s'\n", token.len, spaces);
		} else if (token.type == CYAML_TOKEN_STRING) {
			printf("String: \"%.*s\"\n", token.len, token.data);
		} else if (token.type == CYAML_TOKEN_SYMBOL) {
			printf("Symbol: '%.*s'\n", token.len, token.data);
		} else if (token.type == CYAML_TOKEN_DASH) {
			printf("DASH: '-'\n");
		} else if (token.type == CYAML_TOKEN_COLON) {
			printf("COLON: ':'\n");
		}
		token = cyaml_token_get(&p);
	}
	
	free(buffer);
	return NULL;
}

CYAMLDEF cyaml_t *
cyaml_lookup(cyaml_t *cyaml, char *path)
{
	cyaml_log_message("TODO: Implement 'cyaml_lookup'");
	return NULL;
}

CYAMLDEF void
cyaml_free(cyaml_t *cyaml)
{
	cyaml_log_message("TODO: Implement 'cyaml_free'");
	return NULL;
}

#undef CYAML_TOKEN_STRINGP
#undef CYAML_TOKEN_KEYP
#undef CYAML_TOKEN_VALUEP
#undef CYAML_TOKEN_COLONP
#undef CYAML_TOKEN_INDENTP
#undef CYAML_TOKEN_UNDENTP
#undef CYAML_TOKEN_DASHP

#endif /* CYAML_IMPLEMENTATION */
#endif /* CYAML_H_ */
