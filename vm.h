#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct{
    int capacity; // start with STACK_MAX
    Value* bot;
    Value* top;
} ValueStack; 

typedef struct{
    Chunk* chunk;
    uint8_t* ip;
    ValueStack stack;
} VM;

typedef enum{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;


void initVM();
void freeVM();
InterpretResult interpret(Chunk* chunk);
void push(Value value);
Value pop();

#endif
