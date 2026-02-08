#include "hashtable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TABLE_MAX_LOAD 0.75

typedef struct Entry {
    char* key;
    void* value;
    struct Entry* next; 
    
    
} Entry;

struct HashTable {
    int count;
    int capacity;
    Entry* entries;
};


uint32_t hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

HashTable* table_create(void) {
    HashTable* table = malloc(sizeof(HashTable));
    if (table == NULL) return NULL;
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
    return table;
}

void table_free(HashTable* table) {
    if (table == NULL) return;
    for (int i = 0; i < table->capacity; i++) {
        if (table->entries[i].key != NULL) {
            free(table->entries[i].key);
        }
    }
    free(table->entries);
    free(table);
}

static Entry* find_entry(Entry* entries, int capacity, const char* key) {
    uint32_t index = hash_string(key, strlen(key)) % capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (entry->value == NULL) { 
                return tombstone != NULL ? tombstone : entry;
            } else { 
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (strcmp(entry->key, key) == 0) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(HashTable* table, int capacity) {
    Entry* entries = malloc(sizeof(Entry) * capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(HashTable* table, const char* key, void* value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
        adjust_capacity(table, capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && entry->value == NULL) table->count++;

    if (is_new_key) {
        entry->key = strdup(key); 
    } else {
        
        if (entry->value != NULL) {
            free(entry->value);
        }
    }

    entry->value = value;
    
    return is_new_key;
}

bool table_get(HashTable* table, const char* key, void** out_value) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *out_value = entry->value;
    return true;
}

bool table_delete(HashTable* table, const char* key) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    
    free(entry->key);
    entry->key = NULL;
    entry->value = (void*)1; 
    
    return true;
}

void table_print_keys(HashTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        if (table->entries[i].key != NULL) {
            printf("%s, ", table->entries[i].key);
        }
    }
    printf("\n");
}

void table_iterate(HashTable* table, TableIterateFn callback, void* userdata) {
    if (table == NULL) return;
    for (int i = 0; i < table->capacity; i++) {
        if (table->entries[i].key != NULL) {
            callback(table->entries[i].key, table->entries[i].value, userdata);
        }
    }
}
