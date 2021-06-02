#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "object.h"
#include "value.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}


static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    // get the hash
    Entry* tombstone = NULL;
    uint32_t index = key->hash & (capacity-1);

    for(;;) {
        Entry* entry = &entries[index];

        if(entry->key == NULL) {
            if(IS_NIL(entry->value)) {
                return tombstone!=NULL ? tombstone : entry; 
            } else {
                if(tombstone == NULL) tombstone = entry;
            }
        } else if(entry->key == key) {
            // found the appropriate key
            return entry;
        }

        index = (index+1) & (capacity - 1);
    }
}

static void adjustCapacity(Table* table, int capacity) {
    // expand and rearrange 
    Entry* entries = ALLOCATE(Entry, capacity);

    // populate default val
    for(int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    // reassign and keep count 
    table->count = 0;

    for(int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if(entry->key == NULL) continue; // skip empty values

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }


    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    // returns true iff new entry was added
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    } 
    
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if(isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    // if found entry with key <key>, set entry's value into <value>
    if(table->count == 0) return false; // protect against entries being NULL

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false;

    // found it
    *value = entry->value;
    return true;
}

bool tableDelete(Table* table, ObjString* key) {
    // replace with tombstone instead of actually deleting it
    // and also do not decrement count
    if(table->count == 0) return false; 

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false; // key doesnt exist or is already deleted

    entry->key = NULL;
    entry->value = BOOL_VAL(true); 

    return true;
}

void tableAddAll(Table* from, Table* to){
    // adds all entries in table <from> into table <to>
    for(int i = 0; i < from->capacity; i++)  {
        Entry* entry = &from->entries[i];
        if(entry->key == NULL) continue;

        tableSet(to, entry->key, entry->value);
    }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if(table->count == 0) return NULL; 

    uint32_t index = hash & (table->capacity - 1);

    for(;;) {
        Entry* entry = &table->entries[index];

        if(entry->key == NULL) {
            if(IS_NIL(entry->value)) return NULL;
        } else if(entry->key->length == length && 
                entry->key->hash == hash && 
                memcmp(entry->key->chars, chars, length) == 0){
            return entry->key;
        }


        index = (index + 1) & (table->capacity - 1);
    }
}

void tableRemoveWhite(Table* table) {
    for(int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if(entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    for(int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}
