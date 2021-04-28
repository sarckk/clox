#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
    Chunk chunk;
    initChunk(&chunk);

    // test loading constants
    /* int constant = addConstant(&chunk, 1.2); */
    /* writeChunk(&chunk, OP_CONSTANT, 123); */
    /* writeChunk(&chunk, constant, 123); */
    writeConstant(&chunk, 256, 123);

    writeChunk(&chunk, OP_RETURN, 123);
    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);

    return 0;
}
