#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_POP,
    OP_ADD, 
    OP_SUBTRACT, 
    OP_DIVIDE, 
    OP_MULTIPLY, 
    OP_NIL,
    OP_FALSE,
    OP_NOT,
    OP_TRUE,
    OP_NEGATE,
    OP_EQUAL,
    OP_GREATER, 
    OP_LESS,
    OP_CONSTANT,
    OP_RETURN,
    OP_PRINT,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_CLASS,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_PROPERTY_VAR,
    OP_SET_PROPERTY_VAR,
    OP_DEL_PROPERTY,
} OpCode;

typedef struct{
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
    int* lines;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value value);

#endif
