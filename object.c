#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objType)\
    (type*)allocateObject(sizeof(type), objType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* obj = (Obj*)reallocate(NULL, 0, size);
    obj->type = type;
    obj->next = vm.objects;
    vm.objects = obj;
    return obj;
}

ObjString* makeString(int length) {
    ObjString* string = (ObjString*)allocateObject(sizeof(ObjString) + length + 1, OBJ_STRING);
    string->length = length;
    return string;
}


ObjString* copyString(const char* chars, int length) {
    ObjString* string = makeString(length);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    return string;
}

void printObject(Value value) {
    switch(OBJ_TYPE(value)) {
        case OBJ_STRING: 
            printf("%s", AS_CSTRING(value));
            break;
    }
}


/* ObjString* allocateString(const char* chars, int length) { */
/*     ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING); */
/*     string->length = length; */
/*     string->chars = chars; */
/*     return string; */
/* } */
