/**
 * @author wwotz (wwotz12@gmail.com)
 * @file cyaml.h
 * @description A single header yaml parser.
 * @instructions add '#define CYAML_IMPLEMENTATION' to the start of the
 * file that you want to include the source into like
 * this:
 *
 * `#define CYAML_IMPLEMENTATION
 *  #include "cyaml.h"`
 *
 */

#ifndef CYAML_H_
#define CYAML_H_

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef CYAMLDEF
#ifdef CYAMLSTATIC
#define CYAMLDEF static
#else /* !defined(CYAMLSTATIC) */
#define CYAMLDEF extern
#endif /* CYAMLSTATIC */
#endif /* CYAMLDEF */

#define CYAML_LOG_MESSAGE_CAPACITY (256) /* maximum capacity of a message reported by cyaml */
#define CYAML_LOG_STACK_CAPACITY   (20)  /* maximum life span of a message in the cyaml logging
					 *  system */

CYAMLDEF const char *
cyaml_error_pop(void);

typedef struct cyaml_list_t {
	size_t size;
	size_t capacity;
	char *key;
	struct cyaml_list_t *values;
} cyaml_list_t;

typedef struct cyaml_dict_t {
	size_t size;
	size_t capacity;
	cyaml_list_t **lists;
} cyaml_dict_t;

typedef enum cyaml_loc_t {
	CYAML_LOC_MEMORY,
	CYAML_LOC_DISK,
} cyaml_loc_t;

CYAMLDEF cyaml_dict_t *
cyaml_parse(char *src, size_t n, cyaml_loc_t loc);

CYAMLDEF cyaml_list_t *
cyaml_lookup(cyaml_dict_t *dict, char *src, size_t n);

CYAMLDEF void
cyaml_dict_free(cyaml_dict_t *dict);

#ifdef CYAML_IMPLEMENTATION

/**
 * @Internal: The logging stack, used to store messages in a cyclical
 * way such that we can store many messages, but old ones that have
 * not been checked are overwritten with newer error messages.
 */
static char cyaml_log_stack[CYAML_LOG_STACK_CAPACITY][CYAML_LOG_MESSAGE_CAPACITY];
static size_t cyaml_log_stack_ptr;
static size_t cyaml_log_stack_size;

#define CYAML_LOG_MESSAGE(fmt, ...) \
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

static inline char *
cyaml_read_file(char *src, size_t n)
{
	FILE *fd;
	char fname[n+1], *buffer;
	size_t fsize, nread;
	snprintf(fname, n + 1, "%.*s", n, src);
	fname[n] = '\0';
	fd = fopen(fname, "r");
	if (!fd) {
		cyaml_log_message("Failed to open file '%s'", fname);
		return NULL;
	}

	fseek(fd, 0, SEEK_END);
	fsize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	if (fsize <= 0) {
		cyaml_log_message("'%s' is empty!", fname);
		fclose(fd);
		return NULL;
	}

	buffer = malloc(fsize + 1);
	if (!buffer) {
		cyaml_log_message("Failed to allocate space for file '%s'", fname);
		fclose(fd);
		return NULL;
	}

	nread = fread(buffer, sizeof(*buffer), fsize, fd);
	if (nread != fsize) {
		cyaml_log_message("Failed to read '%s'!", fname);
		free(buffer);
		fclose(fd);
		return NULL;
	}
		
	buffer[nread] = '\0';
	fclose(fd);
	return buffer;
}

#define CYAML_DICT_DEFAULT_CAPACITY (4)

static cyaml_dict_t *
cyaml_dict_create(void)
{
	cyaml_dict_t *dict;
	dict = malloc(sizeof(*dict));
	if (!dict) {
		cyaml_log_message("Failed to allocate yaml dictionary!");
		return NULL;
	}

	dict->size = 0;
	dict->capacity = CYAML_DICT_DEFAULT_CAPACITY;
	dict->lists = calloc(CYAML_DICT_DEFAULT_CAPACITY, sizeof(*dict->lists));
	if (!dict->lists) {
		cyaml_log_message("Failed to allocate yaml dictionary!");
		free(dict);
		return NULL;
	}
	return dict;
}

static int
cyaml_dict_resize(cyaml_dict_t *dict)
{
	size_t new_capacity;
	cyaml_list_t **new_lists;
	new_capacity = dict->capacity * 2;

	new_lists = realloc(dict->lists, new_capacity * sizeof(*new_lists));
	if (!new_lists) {
		cyaml_log_message("Failed to resize yaml dictionary!");
		return -1;
	}

	dict->lists = new_lists;
	dict->capacity = new_capacity;
	return 0;
}

static int
cyaml_dict_insert(cyaml_dict_t *dict, cyaml_list_t *list)
{
	if (dict->size == dict->capacity) {
		if (cyaml_dict_resize(dict) != 0) {
			return -1;
		}
	}

	dict->lists[dict->size++] = list;
	return 0;
}

static int
cyaml_parse_key(cyaml_dict_t *dict, char *parent, char *buffer)
{
	char *key;
	char *p = buffer;
	p += strcspn(p, ":\r\t\n ");

	key = strndup(buffer, (size_t) (p-buffer));
	if (!key) {
		cyaml_log_message("Ran out of memory!");
		return -1;
	}

	printf("Key: %s\n", key);
	
	p += strcspn(p, ":");
	if (*p != ':') {
		cyaml_log_message("Expected ':' after key '%.*s'",
				  (size_t) (p-buffer), buffer);
		return -1;
	}

	if (isalpha(*p)) {
		if (cyaml_parse_key(dict, p) != 0) {
			cyaml_dict_free(dict);
			dict = NULL;
			break;
		}
	} else if (*p == '"') {
		if (cyaml_parse_string(dict, p) != 0) {
			cyaml_dict_free(dict);
			dict = NULL;
			break;
		}
	} else {
		cyaml_log_message("Invalid yaml syntax!");
		cyaml_dict_free(dict);
		dict = NULL;
		break;
	}
	
	p++;
	p += strspn(p, "\t\n\r ");
	if (*p == '-') {
		cyaml_parse_list(dict, key, p);
	} else if (isalpha(*p)) {
		if (cyaml_parse_symbol(dict, key, p)
	} else if (*p == '"') {
	}
	return -1;
}

static int
cyaml_parse_string(cyaml_dict_t *dict, char *p)
{
	printf("%s\n", p);
	return -1;
}

CYAMLDEF cyaml_dict_t *
cyaml_parse(char *src, size_t n, cyaml_loc_t loc)
{
	char *buffer, *p;
	cyaml_dict_t *dict;
	if (src == NULL || n <= 0) {
		cyaml_log_message("String is empty!");
		return NULL;
	}

	dict = cyaml_dict_create();
	if (!dict) return NULL;
	
	if (loc == CYAML_LOC_DISK) {
		buffer = cyaml_read_file(src, n);
	} else {
		buffer = strndup(src, n);
	}

	if (!buffer) {
		free(dict);
		return NULL;
	}

	p = buffer;
	p += strspn(p, "\t\n\r ");
	while (*p != '\0') {
		if (isalpha(*p)) {
			if (cyaml_parse_key(dict, NULL, p) != 0) {
				cyaml_dict_free(dict);
				dict = NULL;
				break;
			}
		} else if (*p == '"') {
			if (cyaml_parse_string(dict, p) != 0) {
				cyaml_dict_free(dict);
				dict = NULL;
				break;
			}
		} else {
			cyaml_log_message("Invalid yaml syntax!");
			cyaml_dict_free(dict);
			dict = NULL;
			break;
		}
		p += strspn(p, "\t\n\r ");
	}

	free(buffer);
	return dict;
}

CYAMLDEF cyaml_list_t *
cyaml_lookup(cyaml_dict_t *dict, char *src, size_t n)
{
	return NULL;
}

CYAMLDEF void
cyaml_dict_free(cyaml_dict_t *dict)
{
	return;
}

#endif /* CYAML_IMPLEMENTATION */
#endif /* CYAML_H_ */




