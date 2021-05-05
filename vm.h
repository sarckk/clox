#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "compiler.h"
#include "value.h"

#define STACK_MAX 256

typedef struct{
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
    Obj* objects; // head of the instrusive list of objects which act as nodes in linked list
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
