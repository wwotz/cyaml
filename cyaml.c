/**
 * @author wwotz (wwotz12@gmail.com)
 * @file cyaml.c
 * @description parses yaml and stores it inside of a hashtable.
 */

// An item is simply just dynamic arrays.
typedef struct cyaml_item_t {
	size_t size;
	size_t capacity;
	char *data;
} cyaml_item_t;

// the first element in the list is the key whereas 
// the rest of the elements are the values.
typedef struct cyaml_list_t {
	size_t size;
	size_t capacity;
	cyaml_item_t *items;
} cyaml_dict_t;

typedef struct cyaml_dict_t {
	size_t size;
	size_t capacity;
	cyaml_item_t *lists;
};




