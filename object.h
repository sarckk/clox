#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value)  isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)    isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value)   isObjType(value, OBJ_CLOSURE)
#define IS_CLASS(value)     isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value)  isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value)  ((ObjInstance*)AS_OBJ(value))
#define AS_CLASS(value)     ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)   ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (((ObjNative*)AS_OBJ(value))->function)
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_BOUND_METHOD,
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};


typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;
 
typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    int arity;
    Chunk chunk;
    ObjString* name;
    int upvalueCount;
} ObjFunction;

typedef struct ObjUpvalue {
    Obj obj;
    Value closed;
    Value* location;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjString{
    Obj obj;
    int length;
    uint32_t hash;
    char* chars;
};

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjInstance* newInstance(ObjClass* klass);
ObjClass* newClass(ObjString* name);
ObjFunction* newFunction();
ObjUpvalue* newUpvalue(Value* value);
ObjClosure* newClosure(ObjFunction* function);
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    // we don't put this in the macro because it refers to value twice
    // and macro is expanded by evaluating the expression of the args
    // each time, if parameter value had a side effect, it would be run
    // twice
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
