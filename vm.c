#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "vm.h"
#include "memory.h"

VM vm;

/* // test */
/* void getVMObjectsSize(){ */
/* #ifdef DEBUG_GC_LOG */
/*     int count = 0; */
/*     Obj* current = vm.objects; */
/*     while(current !=NULL) { */
/*         printObject(OBJ_VAL(current)); */
/*         printf("{ \n"); */
/*         printf("\tCURRENT POINTER: %p \n", current); */
/*         printf("\tNEXT POINTER: %p \n", current->next); */
/*         printf("} \n"); */
/*         count++; */
/*         current = current->next; */
/*     } */

/*     printf("========= VM.OBJECTS CURRENTLY HAS %d objects ==========\n", count); */
/* #endif */
/* } */

static Value clockNative(int argCount, Value* args) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack(){
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for(int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if(function->name == NULL) {
            // top level script
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM(){
    resetStack();
    vm.objectCount = 0;
    vm.objects = NULL;

    vm.grayStack = NULL;
    vm.grayCount = 0;
    vm.grayCapacity = 0;

    vm.bytesAllocated = 0;
    vm.nextGCAt = 1024 * 1024;

    initTable(&vm.strings);
    initTable(&vm.globals);
    vm.initString = NULL;
    vm.initString = copyString("init", 4);


    defineNative("clock", clockNative);
}

void freeVM(){
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value) {
    // *vm.stackTop++ = value;
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    //return *--vm.stackTop;
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
    if(closure->function->arity != argCount) {
        runtimeError("Expected %d arguments but got %d", closure->function->arity, argCount);
        return false;
    }
    
    if(vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++]; // new frame
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if(IS_OBJ(callee)) {
        switch(OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount-1] = bound->receiver;
                return call(bound->method, argCount);
            }
            case OBJ_CLASS: {
                ObjClass* klass = AS_CLASS(callee);
                vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
                // technically above line is same as: vm.stack[vm.stackTop-argCount-1-vm.stack] = ...

                // init function runs
                Value value;
                if(tableGet(&klass->methods, vm.initString, &value)) {
                    return call(AS_CLOSURE(value), argCount);
                } else if(argCount != 0) {
                    runtimeError("Expected 0 arguments but got %d", argCount);
                    return false;
                }

                return true;
            }
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
             }
            case OBJ_CLOSURE: 
                return call(AS_CLOSURE(callee), argCount);
            default: break; // non-callable 
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

static bool isFalsy(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
    // concatenates two strings at the top of the stack and pushes a new string 
    ObjString* second = AS_STRING(peek(0));
    ObjString* first = AS_STRING(peek(1));
    int length = first->length + second->length;
    char* concatString = ALLOCATE(char, length + 1);
    strcpy(concatString, first->chars);
    strcat(concatString, second->chars);

    ObjString* result = takeString(concatString, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static void closeUpvalues(Value* last) {
    while(vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static ObjUpvalue* captureUpvalue(Value* local) {
    // singly linked list traversal
    ObjUpvalue* prev = NULL;
    ObjUpvalue* current = vm.openUpvalues;
    while(current != NULL && current->location > local) {
        prev = current;
        current = current->next;
    }

    // Reason #1: current is not null and we found a duplicate 
    if(current != NULL && current->location == local) {
        // we found a duplicate, let's return that
        // this let's us share memory for two closures referring to the same local variable.
        return current;
    }

    ObjUpvalue* upvalue = newUpvalue(local);
    // here current is either NULL or current->location < local, either way it is the next thing after upvalue
    upvalue->next = current;

    if(prev == NULL) {
        vm.openUpvalues = upvalue;
    } else {
        prev->next = upvalue; 
    }

    return upvalue;
}

static void defineMethod(ObjString* name) {
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, peek(0));
    pop();
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
      Value method;

     if(!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
     }

     ObjClosure* methodClosure = AS_CLOSURE(method);
     ObjBoundMethod* bound = newBoundMethod(peek(0), methodClosure);

     pop();
     push(OBJ_VAL(bound));
     return true;
}

static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if(!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount);

    if(!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);
    
    Value value;
    if(tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount-1] = value;
        return callValue(value, argCount);
    }

    return invokeFromClass(instance->klass, name, argCount);
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount-1];
#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define BINARY_OP(valueType, op) \
    do { \
        if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while(false) 

    for(;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");

        for(Value* slot = vm.stack; slot < vm.stackTop ; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }

        printf("\n");

        disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        uint8_t instruction;

        switch(instruction = READ_BYTE()) {
            case OP_SUPER_INVOKE: {
                                      ObjClass* super = AS_CLASS(pop());
                                      ObjString* method = READ_STRING();
                                      int argCount = READ_BYTE();
                                      if(!invokeFromClass(super, method, argCount)) {
                                          return INTERPRET_RUNTIME_ERROR;
                                      }
                                      frame = &vm.frames[vm.frameCount - 1];
                                      break;
                                  }
            case OP_GET_SUPER: {
                                   ObjString* method = READ_STRING();
                                   ObjClass* super = AS_CLASS(pop());

                                   if(!bindMethod(super, method)) {
                                       return INTERPRET_RUNTIME_ERROR;
                                   }
                                   break;
                               }
            case OP_INHERIT: {
                                 Value super = peek(1);

                                 if(!IS_CLASS(super)) {
                                     runtimeError("Can only inherit from classes.");
                                     return INTERPRET_RUNTIME_ERROR;
                                 }

                                 ObjClass* klass = AS_CLASS(peek(0));
                                    
                                 tableAddAll(&AS_CLASS(super)->methods, &klass->methods);

                                 pop();

                                 break;
                             }
            case OP_INVOKE: {
                              ObjString* method = READ_STRING();
                              int argCount = READ_BYTE();

                              if(!invoke(method, argCount)) {
                                  return INTERPRET_RUNTIME_ERROR;
                              }
                              frame = &vm.frames[vm.frameCount - 1];
                              break;
                            }
            case OP_METHOD: {
                                defineMethod(READ_STRING());
                                break;
                            }
            case OP_SET_PROPERTY: {
                                      if(!IS_INSTANCE(peek(1))) {
                                          runtimeError("Only instances have properties.");
                                          return INTERPRET_RUNTIME_ERROR;
                                      }

                                      ObjInstance* instance = AS_INSTANCE(peek(1));
                                      tableSet(&instance->fields, READ_STRING(), peek(0));
                                      Value newValue = pop();
                                      pop();
                                      push(newValue);
                                      break;
                                  }
            case OP_GET_PROPERTY: {
                                      if(!IS_INSTANCE(peek(0))) {
                                          runtimeError("Only instances have properties.");
                                          return INTERPRET_RUNTIME_ERROR;
                                      }

                                      ObjInstance* instance = AS_INSTANCE(peek(0));
                                      ObjString* name = READ_STRING();
                                      Value value;
                                      if(tableGet(&instance->fields, name, &value)) {
                                          pop();
                                          push(value);
                                          break;
                                      }

                                      if(!bindMethod(instance->klass, name)) {
                                          return INTERPRET_RUNTIME_ERROR;
                                      }

                                      break;
                                  }
            case OP_CLASS: {
                                ObjClass* klass = newClass(READ_STRING());
                                Value val = OBJ_VAL(klass);
                                push(val);
                                break;
                            }
            case OP_CLOSE_UPVALUE: {
                                       // the local value we want is currently at the top of the stack
                                       closeUpvalues(vm.stackTop - 1);
                                       pop();
                                       break;
                                   }
            case OP_SET_UPVALUE: {
                                     uint8_t index = READ_BYTE();
                                     *frame->closure->upvalues[index]->location = peek(0); // an expression; don't pop
                                     break;
                                 }
            case OP_GET_UPVALUE: {
                                     uint8_t index = READ_BYTE();
                                     push(*frame->closure->upvalues[index]->location);
                                     break;
                                 }
            case OP_CALL: {
                              int argCount = READ_BYTE();
                              if(!callValue(peek(argCount), argCount)) {
                                  return INTERPRET_RUNTIME_ERROR;
                              }
                              frame = &vm.frames[vm.frameCount -1];
                              break;
                          }
            case OP_LOOP: {
                              uint16_t offset = READ_SHORT();
                              frame->ip -= offset;
                              break;
                          }
            case OP_JUMP: {
                              uint16_t offset = READ_SHORT();
                              frame->ip += offset;
                              break;
                          }
            case OP_JUMP_IF_FALSE: {
                                       uint16_t offset = READ_SHORT();
                                       if(isFalsy(peek(0))) frame->ip += offset;
                                       break;
                                   }
            case  OP_GET_LOCAL: {
                                    uint8_t slot = READ_BYTE();
                                    push(frame->slots[slot]);
                                    break;
                                }
            case OP_SET_LOCAL: {
                                   uint8_t slot = READ_BYTE();
                                   frame->slots[slot] = peek(0);
                                   break;
                               }
            case OP_SET_GLOBAL: {
                                     ObjString* name = READ_STRING();
                                     if(tableSet(&vm.globals, name, peek(0))) {
                                         tableDelete(&vm.globals, name);
                                         runtimeError("Undefined variable '%s'.", name->chars);
                                         return INTERPRET_RUNTIME_ERROR;
                                     }
                                     break;
                                }
            case OP_GET_GLOBAL:  {
                                     ObjString* name = READ_STRING();
                                     Value value;
                                     if(!tableGet(&vm.globals, name, &value)) {
                                         runtimeError("Undefined variable '%s'.", name->chars);
                                         return INTERPRET_RUNTIME_ERROR;
                                     }

                                     push(value);
                                     break;
                                 }
            case OP_DEFINE_GLOBAL: {
                                       ObjString* name = READ_STRING();
                                       tableSet(&vm.globals, name, peek(0));
                                       pop();
                                       break;
                                   }
            case OP_POP: pop(); break;
            case OP_PRINT: {
                               printValue(pop());
                               printf("\n");
                               break;
                           }
            case OP_EQUAL: {
                               Value b = pop();
                               Value a = pop();
                               push(BOOL_VAL(valuesEqual(a,b)));
                               break;
                           }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_NOT: push(BOOL_VAL(isFalsy(pop()))); break;
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_ADD:{
                            if(IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                                double b = AS_NUMBER(pop()); 
                                double a = AS_NUMBER(pop()); 
                                push(NUMBER_VAL(a + b)); 
                            } else if(IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                                concatenate();
                            } else {
                                runtimeError("Operands must be two numbers or two strings.");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            break;
                        }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NEGATE: 
                            if(!IS_NUMBER(peek(0))) {
                                runtimeError("Operand must be a number.");
                                return INTERPRET_RUNTIME_ERROR;
                            } 
                            push(NUMBER_VAL(-AS_NUMBER(pop())));
                            break;
            case OP_CLOSURE: {
                                 ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                                 ObjClosure* closure = newClosure(function);
                                 push(OBJ_VAL(closure));

                                 for(int i = 0; i < function->upvalueCount; i++) {
                                     uint8_t isLocal = READ_BYTE();
                                     uint8_t index = READ_BYTE();
                                     if(isLocal) {
                                         closure->upvalues[i] = captureUpvalue(frame->slots + index);
                                     } else {
                                         closure->upvalues[i] = frame->closure->upvalues[index];
                                     }
                                 }

                                 break;
                             }
            case OP_CONSTANT: {
                                  Value constant = READ_CONSTANT();
                                  push(constant);
                                  break;
                              }
            case OP_RETURN: {
                                Value result = pop();
                                closeUpvalues(frame->slots);
                                vm.frameCount--;
                                if(vm.frameCount == 0) {
                                    pop(); // pops <script> object
                                    return INTERPRET_OK;
                                }

                                vm.stackTop = frame->slots;
                                push(result);
                                frame = &vm.frames[vm.frameCount - 1];
                                break;
                            }
        }
    }


#undef READ_BYTE
#undef READ_SHORT
#undef READ_STRING
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if(function == NULL) { return INTERPRET_COMPILE_ERROR; }

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop(); // GC hack
    push(OBJ_VAL(closure));
    call(closure, 0);

    InterpretResult result = run();

    return result;
}


