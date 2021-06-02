#ifndef clox_value_h
#define clox_value_h

#include <string.h>
#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

#ifdef NAN_BOXING

typedef uint64_t Value;

#define SIG_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1 // 01
#define TAG_FALSE 2 // 10 
#define TAG_TRUE  3 // 11 

#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))

#define IS_BOOL(value)   ((value | 1) == TRUE_VAL)
#define IS_NIL(value)    (value == NIL_VAL)
#define IS_NUMBER(value) ((value & QNAN) != QNAN)
#define IS_OBJ(value)    ((value & (QNAN | SIG_BIT)) == (QNAN | SIG_BIT))

#define AS_BOOL(value)   (value == TRUE_VAL)
#define AS_NUMBER(value) valToNum(value)
#define AS_OBJ(value)    \
    ((Obj*)(uintptr_t)((value) & ~(SIG_BIT | QNAN)))

#define OBJ_VAL(object)   \
    (Value)(SIG_BIT | QNAN | (uint64_t)(uintptr_t)(object)) 
#define BOOL_VAL(b)       ((b) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(value) numToVal(value)  

static inline Value numToVal(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

static inline double valToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

#else 

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number; 
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.obj)

#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj*)object}})
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})

#endif

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;


bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);


#endif
