#ifndef clox_vm_h
#define clox_vm_h

#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct{
    uint16_t nextClassID;
    ObjString* initString;

    // adapative GC scheduling
    size_t bytesAllocated;
    size_t nextGCAt;
    
    // dyanmic array for GC tricolor abstraction
    Obj** grayStack;
    int grayCapacity;
    int grayCount;

    ObjUpvalue* openUpvalues;
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table strings;
    Table globals;
    int objectCount;
    Obj* objects; // head of the instrusive list of objects which act as nodes in lined list
} VM;

typedef enum{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif
