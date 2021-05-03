#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
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
