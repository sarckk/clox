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
        case OBJ_UPVALUE: {
                              FREE(ObjUpvalue, object);
                              break;
                          }
        case OBJ_CLOSURE: {
                              ObjClosure* closure = (ObjClosure*)object;
                              FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
                              FREE(ObjClosure, object);
                              break;
                          }
        case OBJ_NATIVE: {
                             FREE(ObjNative, object);
                             break;
                         }
        case OBJ_FUNCTION: {
                               ObjFunction* function = (ObjFunction*)object;
                               freeChunk(&function->chunk);
                               FREE(ObjFunction, function);
                               break;
        }
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
    while(object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}

