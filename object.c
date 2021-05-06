#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, length, dataType, objType)\
    (type*)allocateObject(sizeof(type) + (length + 1) * sizeof(dataType), objType);

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}

ObjString* allocateString(const char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, length, char, OBJ_STRING);
    string->length = length;
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    return string;
}

ObjString* takeString(char* chars, int length) {
    return allocateString(chars, length);
}

/* ObjString* copyString(const char* chars, int length) { */
/*     char* heapChars = ALLOCATE(char, length + 1); */
/*     memcpy(heapChars, chars, length); */ 
/*     heapChars[length] = '\0'; */
/*     return allocateString(heapChars, length); */
/* } */

void printObject(Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_STRING: 
            printf("%s", AS_CSTRING(value));
            break;
    }
}


