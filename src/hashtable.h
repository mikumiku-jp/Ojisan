#ifndef OJISAN_HASHTABLE_H
#define OJISAN_HASHTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct Entry Entry;
typedef struct HashTable HashTable;


HashTable* table_create(void);


void table_free(HashTable* table);


bool table_set(HashTable* table, const char* key, void* value);


bool table_get(HashTable* table, const char* key, void** out_value);


bool table_delete(HashTable* table, const char* key);


void table_print_keys(HashTable* table);


typedef void (*TableIterateFn)(const char* key, void* value, void* userdata);
void table_iterate(HashTable* table, TableIterateFn callback, void* userdata);


uint32_t hash_string(const char* key, int length);

#endif 
