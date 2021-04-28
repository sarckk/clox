#include <stdlib.h>

#include "chunk.h" 

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
<<<<<<< Updated upstream
    chunk->lines = NULL;
    chunk->linesEncCount = 0;
    chunk->linesEncCapacity = 0;
    initValueArray(&chunk->constants);
}

static int addRLELine(Chunk* chunk, int line) {
    // peek the top of the array
    if(chunk->linesEncCount == 0) {
        // nothing in array yet
        chunk->lines[0] = 1;
        chunk->lines[1] = line;
        return 2;
    }

    size_t size = chunk->linesEncCount;
    int top = chunk->lines[size-1];
    if(line == top) {
        // repeated value, let's add 1 to the count
        chunk->lines[size-2]++;
        return 0;
    } else {
        // different value, let's add count and value
        chunk->lines[size] = 1;
        chunk->lines[size + 1] = line;
        return 2;
    }
}

static int requiredEncodings(Chunk* chunk, int line) {
    if(chunk->linesEncCount == 0) return 2;
    int top = chunk->lines[(chunk->linesEncCount)-1];
    if(line == top) {
        return 0;
    } else {
        return 2;
    }
}

=======
    chunk->lineCount = 0;    // <--
  chunk->lineCapacity = 0; // <--
  chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

>>>>>>> Stashed changes
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t,chunk->code,
        oldCapacity, chunk->capacity);
    // Don't grow line array here...
  }

<<<<<<< Updated upstream
    if(chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    if(chunk->linesEncCapacity < chunk->linesEncCount + requiredEncodings(chunk, line)) {
        // chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
        int oldCapacity = chunk->linesEncCapacity;
        chunk->linesEncCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->linesEncCapacity);
    }
=======
  chunk->code[chunk->count] = byte;
  chunk->count++;

  // See if we're still on the same line.
  if (chunk->lineCount > 0 &&
      chunk->lines[chunk->lineCount - 1].line == line) {
    return;
  }
>>>>>>> Stashed changes

  // Append a new LineStart.
  if (chunk->lineCapacity < chunk->lineCount + 1) {
    int oldCapacity = chunk->lineCapacity;
    chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
    chunk->lines = GROW_ARRAY( LineStart,chunk->lines,
                              oldCapacity, chunk->lineCapacity);
  }

<<<<<<< Updated upstream
    chunk->code[chunk->count] = byte;
    // chunk->lines[chunk->count] = line;
    chunk->count++;
    chunk->linesEncCount += addRLELine(chunk, line);
=======
  LineStart* lineStart = &chunk->lines[chunk->lineCount++];
  lineStart->offset = chunk->count - 1;
  lineStart->line = line;
>>>>>>> Stashed changes
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->linesEncCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1; // offset where the constant was stored for future access
}

int getLine(Chunk* chunk, int instruction) {
  int start = 0;
  int end = chunk->lineCount - 1;

  for (;;) {
    int mid = (start + end) / 2;
    LineStart* line = &chunk->lines[mid];
    if (instruction < line->offset) {
      end = mid - 1;
    } else if (mid == chunk->lineCount - 1 ||
        instruction < chunk->lines[mid + 1].offset) {
      return line->line;
    } else {
      start = mid + 1;
    }
  }
}
