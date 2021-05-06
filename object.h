#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"


#define OBJ_TYPE(value)     (AS_OBJ(value)->type)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)

#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString{
    Obj obj;
    int length;
    char chars[]; // variable length array
};

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjString* allocateString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    // we don't put this in the macro because it refers to value twice
    // and macro is expanded by evaluating the expression of the args
    // each time, if parameter value had a side effect, it would be run
    // twice
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
