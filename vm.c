#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void resetStack(){
    vm.stack.capacity = STACK_MAX;
    vm.stack.bot = GROW_ARRAY(Value, vm.stack.bot, 0, vm.stack.capacity);
    vm.stack.top = vm.stack.bot; 
}

void initVM(){
    resetStack();
}

void freeVM(){
}

void push(Value value) {
    if(vm.stack.capacity < (int)(vm.stack.top-vm.stack.bot) + 1) {
        // increase capacity
        int oldCapacity = vm.stack.capacity;
        vm.stack.capacity = oldCapacity * 2;
        vm.stack.bot = GROW_ARRAY(Value, vm.stack.bot, oldCapacity, vm.stack.capacity);
    }

    *vm.stack.top = value;
    vm.stack.top++;
}

Value pop() {
    return *--vm.stack.top;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
        double b = pop(); \
        double a = pop(); \
        push(a op b); \
    } while(false)

    for(;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");

        for(Value* slot = vm.stack.bot; slot < vm.stack.top ; slot++) {
               printf("[ ");
              printValue(*slot);
              printf(" ]");
        }

        printf("\n");

        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;

        switch(instruction = READ_BYTE()) {
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_NEGATE: push(-pop()); break;
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_RETURN: {
                printValue(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }


#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}


