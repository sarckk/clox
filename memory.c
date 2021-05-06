#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if(newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* ptr = realloc(pointer, newSize);
    if(ptr == NULL) exit(1);
    return ptr;
}

static void freeObject(Obj* object) {
    switch(object->type) {
        case OBJ_STRING: {
                                ObjString* string = (ObjString*)object;
                                FREE_ARRAY(char, string->chars, string->length + 1);
                                FREE(ObjString, object);
                                break;
                         }
    }
}

void freeObjects() {
    Obj* object = vm.objects; 
    while(vm.objects != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}

