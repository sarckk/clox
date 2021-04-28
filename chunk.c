#include <stdlib.h>

#include "chunk.h" 

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}


void writeChunk(Chunk* chunk, uint8_t byte, int line) {

    if(chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
    }


    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1; // offset where the constant was stored for future access
}

void writeConstant(Chunk* chunk, Value value, int line) {
    int constant = addConstant(chunk, value);
    // store constant as 4 byte number
    writeChunk(chunk, OP_CONSTANT_LONG, line);

    // split constant number into 4 bytes and store them 
    // endianness? store it as little endian -> least significant byte comes first
    writeChunk(chunk, constant & 0xFF, line);
    writeChunk(chunk, (constant >> 8) & 0xFF, line);
    writeChunk(chunk, (constant >> 16) & 0xFF, line);
}

