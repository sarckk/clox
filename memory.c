#include <stdlib.h>
#include <stdio.h>

#include "memory.h"
#include "vm.h"
#include "compiler.h"
#include "table.h"

#ifdef DEBUG_GC_LOG
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;

    if(newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
        if(vm.bytesAllocated > vm.nextGCAt) {
            collectGarbage();
        }
    }

    if(newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* ptr = realloc(pointer, newSize);
    if(ptr == NULL) exit(1);
    return ptr;
}

static void freeObject(Obj* object) {

#ifdef DEBUG_GC_LOG
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch(object->type) {
        case OBJ_BOUND_METHOD: {
                                   FREE(ObjBoundMethod, object);
                                   break;
                               }
        case OBJ_INSTANCE: {
                               ObjInstance* instance = (ObjInstance*)object;
                               freeTable(&instance->fields);
                               FREE(ObjInstance, object);
                               break;
                           }
        case OBJ_CLASS: {
                            ObjClass* klass = (ObjClass*)object;
                            freeTable(&klass->methods);
                            FREE(ObjClass, object);
                            break;
                        }
        case OBJ_UPVALUE: {
                              FREE(ObjUpvalue, object);
                              break;
                          }
        case OBJ_CLOSURE: {
                              ObjClosure* closure = (ObjClosure*)object;
                              FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
                              FREE(ObjClosure, object);
                              break;
                          }
        case OBJ_NATIVE: {
                             FREE(ObjNative, object);
                             break;
                         }
        case OBJ_FUNCTION: {
                               ObjFunction* function = (ObjFunction*)object;
                               freeChunk(&function->chunk);
                               FREE(ObjFunction, object);
                               break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
    }
}

void freeObjects() {
    Obj* object = vm.objects; 
    while(object != NULL) {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }

    free(vm.grayStack);
}

void markObject(Obj* object) {
    if(object == NULL) return;
    if(object->isMarked) return;

#ifdef DEBUG_GC_LOG
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    // single entrypoint for marking
    object->isMarked = true;

    if(vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
        if(vm.grayStack == NULL) exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
    if(IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markRoots() {

    // 1. Mark vm stack values
    for(Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // 3. Mark vm frame closures
    for(int i = 0; i < vm.frameCount; i++) {
        markObject((Obj*)vm.frames[i].closure);
    }

    // 4. Mark vm openUpvalues
    for(ObjUpvalue* upvalue = vm.openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject((Obj*) upvalue);
    }


    // 2. Mark vm.globals table 
    markTable(&vm.globals);

    // 5. Mark compiler function 
    markCompilerRoots();

    markObject((Obj*)vm.initString);
} 

static void markValueArray(ValueArray* varray) {
    for(int i = 0; i < varray->count; i++) {
        markValue(varray->values[i]);
    }
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_GC_LOG
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch(object->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* boundMethod = (ObjBoundMethod*)object;
            markValue(boundMethod->receiver);
            markObject((Obj*)boundMethod->method);
            break;
        }
        case OBJ_INSTANCE:  {
            ObjInstance* instance = (ObjInstance*)object;
            markObject((Obj*)instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJ_CLASS:  {
            ObjClass* klass = (ObjClass*)object;
            markTable(&klass->methods);
            markObject((Obj*)klass->name);
            break;
        }
        case OBJ_UPVALUE: {
            markValue(((ObjUpvalue*)object)->closed);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for(int i = 0; i < closure->upvalueCount; i++) {
                markObject((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*)object;
            markObject((Obj*)fn->name);
            markValueArray(&fn->chunk.constants);
            break;

        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            // there isn't any external ref
            break;
    }
}

static void traceReferences() {
    while(vm.grayCount > 0) {
        Obj* obj = vm.grayStack[--vm.grayCount];
        blackenObject(obj);
    }
}

static void sweep() {
    Obj* prev = NULL;
    Obj* current = vm.objects;
    while(current != NULL) {
        if(current->isMarked) {
            current->isMarked = false;
            prev = current;
            current = current->next;
        } else {
            Obj* unreached = current;
            current = current->next;

            if(prev != NULL) {
                prev->next = current;
            } else {
                vm.objects = current;
            }

            freeObject(unreached);
        }
    }
}

void collectGarbage() {
#ifdef DEBUG_GC_LOG
    printf("-- gc begin \n");
    size_t beforeGC = vm.bytesAllocated;
#endif

    // mark and sweep
    markRoots();


    traceReferences();

    tableRemoveWhite(&vm.strings);

    sweep();

    // schedule next GC 
    vm.nextGCAt = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_GC_LOG
    printf("-- gc stopped \n");
    size_t memoryFreed = beforeGC - vm.bytesAllocated;
    printf(" collected %zu bytes(from %zu to %zu) | next GC at %zu\n", memoryFreed, beforeGC, vm.bytesAllocated, vm.nextGCAt);
#endif
}

