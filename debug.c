#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n",name);

    for(int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset+1];

    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int getLine(Chunk* chunk, int offset) {
    for(int i = 0; i < chunk->linesEncCount; i+=2) {
        // we progress i by 2 to skip over the actual value and get the length 
        offset -= chunk->lines[i];
        if(offset < 0) {
            return chunk->lines[i+1];
        }
    }
    return -1;
}

int disassembleInstruction(Chunk* chunk, int offset) { 
    printf("%04d ", offset);
    
<<<<<<< Updated upstream
    if(offset > 0 && getLine(chunk, offset) == getLine(chunk, offset-1)) {
        // came from same line
        printf("   | ");
    } else {
        printf("%4d ", getLine(chunk, offset));
=======
    int line = getLine(chunk, offset);
    if (offset > 0 && line == getLine(chunk, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", line);
>>>>>>> Stashed changes
    }


    uint8_t instruction = chunk->code[offset];
    switch(instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}


