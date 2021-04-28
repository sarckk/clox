#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
    OP_RETURN,
    OP_CONSTANT,
} OpCode;

typedef struct {
  int offset;
  int line;
} LineStart;

typedef struct{
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
<<<<<<< Updated upstream
    int* lines;
    int linesEncCount;
    int linesEncCapacity;
=======
    int lineCount;
    int lineCapacity;
     LineStart* lines;
>>>>>>> Stashed changes
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void freeChunk(Chunk* chunk);
int addConstant(Chunk* chunk, Value value);
int getLine(Chunk* chunk, int instruction);

#endif
