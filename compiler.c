#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define MAX_CASES 256

typedef struct {
    Token name;
    int depth;
} Local;

typedef struct {
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
} Compiler;

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

int innermostLoopStart = -1;
int innermostLoopScopeDepth = 0;

static Chunk* currentChunk() {
    return compilingChunk;
}

static void errorAt(Token* token, const char* message) {
    if(parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if(token->type == TOKEN_EOF) {
        fprintf(stderr, " at the end");
    } else if(token->type == TOKEN_ERROR){
        // do nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for(;;) {
        parser.current = scanToken();
        if(parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if(parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if(!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if(offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int index = addConstant(currentChunk(), value);
    if(index > UINT8_MAX) {
        error("Too many constants in one chunk.");
    }

    return (uint8_t)index;  
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    // -2 to account for the 2 offset bytes
    int jump = currentChunk()->count - offset - 2;

    if(jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset+1] =  jump & 0xff;
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE

    if(!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }

#endif
}

static void expression();
static void declaration();
static void statement();
static ParseRule* getRule(TokenType type);
static void parsePrecendence(Precedence precedence);

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static void addLocal(Token name) {
    if(current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

static bool identifiersEqual(Token* a, Token* b) {
    return a->length == b->length && memcmp(a->start, b->start, a->length) == 0;
}

static void declareVariable() {
    if(current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    // check if same initalized variable exists in same depth
    for(int i = current->localCount-1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if(local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if(identifiersEqual(name, &local->name))  {
            error("Already variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

static uint8_t parseVariable(const char* errMessage) {
    consume(TOKEN_IDENTIFIER, errMessage);

    declareVariable();
    if(current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    current->locals[current->localCount-1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if(current->scopeDepth > 0) {
        // initialize it as being defined
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void and_(bool canAssign){
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecendence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecendence(PREC_OR);

    patchJump(endJump);
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    parsePrecendence((Precedence)(rule->precedence + 1));

    switch(operatorType) {
        case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_GREATER:       emitByte(OP_GREATER); break;
        case TOKEN_LESS:          emitByte(OP_LESS); break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break; 
        case TOKEN_PLUS:          emitByte(OP_ADD); break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void grouping(bool canAssign){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign){
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void unary(bool canAssign){
    TokenType operatorType = parser.previous.type;

    parsePrecendence(PREC_UNARY);

    switch(operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // unreachable
    }
}

static void literal(bool canAssign) {
    switch(parser.previous.type) {
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        default: return;
    }
}

static void string(bool canAssign) { 
    emitConstant(OBJ_VAL(copyString(parser.previous.start+1, parser.previous.length-2)));
}

static int resolveLocal(Compiler* compiler, Token* name){
    for(int i = compiler->localCount - 1; i>=0; i--) {
        Local* local = &compiler->locals[i];
        if(identifiersEqual(name, &local->name)) {
            if(local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t setOp, getOp;
    int arg = resolveLocal(current, &name);
    if(arg != -1) {
        setOp = OP_SET_LOCAL;
        getOp = OP_GET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        setOp = OP_SET_GLOBAL;
        getOp = OP_GET_GLOBAL;
    }

    if(canAssign && match(TOKEN_EQUAL)) {
        // it's a setter statement
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

ParseRule rules[] = {
      [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
      [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
      [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
      [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
      [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
      [TOKEN_MINUS]         = {unary,     binary,   PREC_TERM},
      [TOKEN_PLUS]          = {NULL,     binary,   PREC_TERM},
      [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
      [TOKEN_SLASH]         = {NULL, binary, PREC_FACTOR},
      [TOKEN_STAR]          = {NULL, binary, PREC_FACTOR},
      [TOKEN_BANG]          = {unary,     NULL,   PREC_NONE},
      [TOKEN_BANG_EQUAL]    = {NULL,     binary,   PREC_EQUALITY},
      [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,   PREC_EQUALITY},
      [TOKEN_GREATER]       = {NULL,     binary,   PREC_COMPARISON},
      [TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
      [TOKEN_LESS]          = {NULL,     binary,   PREC_COMPARISON},
      [TOKEN_LESS_EQUAL]    = {NULL,     binary,   PREC_COMPARISON},
      [TOKEN_IDENTIFIER]    = {variable,     NULL,   PREC_NONE},
      [TOKEN_STRING]        = {string,     NULL,   PREC_NONE},
      [TOKEN_NUMBER]        = {number, NULL, PREC_NONE},
      [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
      [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
      [TOKEN_FALSE]         = {literal,     NULL,   PREC_NONE},
      [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
      [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
      [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
      [TOKEN_NIL]           = {literal,     NULL,   PREC_NONE},
      [TOKEN_OR]            = {NULL,     or_,   PREC_OR},
      [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
      [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
      [TOKEN_TRUE]          = {literal,     NULL,   PREC_NONE},
      [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
      [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
      [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parsePrecendence(Precedence precedence) {
    // prefix
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if(prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    // infix 
    while(precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if(canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression(){
    parsePrecendence(PREC_ASSIGNMENT);
}

static void varDeclaration() { 
    uint8_t global = parseVariable("Expect variable name.");

    if(match(TOKEN_EQUAL)) {
        // there's some kind of initalizer
        expression();
    } else {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    defineVariable(global);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect semicolon after expression statement");
    emitByte(OP_POP);
}

static void ifStatement(){
    consume(TOKEN_LEFT_PAREN, "Expect '(' before 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);

    emitByte(OP_POP);
    if(match(TOKEN_ELSE)) statement();

    patchJump(elseJump);
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    // get rid of all local variables
    printf("%d\n", current->localCount);
    while(current->localCount > 0 && current->locals[current->localCount-1].depth > current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect semicolon after print statement");
    emitByte(OP_PRINT);
}

static void forStatement() {
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' before 'for'.");

    if(match(TOKEN_SEMICOLON)) {
        // do nothing
    }else if(match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int surroundingLoopStart = innermostLoopStart;
    int surroundingScopeDepth = innermostLoopScopeDepth;
    innermostLoopStart = currentChunk()->count;
    innermostLoopScopeDepth = current->scopeDepth;

    int exitJump = -1;
    if(!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }

    if(!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);

        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after 'for' clauses.");

        emitLoop(innermostLoopStart);
        innermostLoopStart = incrementStart;

        patchJump(bodyJump);
    }

    declaration();

    emitLoop(innermostLoopStart);

    if(exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    innermostLoopStart = surroundingLoopStart;
    innermostLoopScopeDepth = surroundingScopeDepth;

    endScope();
}

static void switchStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' before 'switch'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch value.");

    consume(TOKEN_LEFT_BRACE, "Expect switch statements to start with '{'");
    
    int notEqualJump = -1;
    int currentCase = 0;
    int caseEnds[MAX_CASES];

    while(match(TOKEN_CASE)) {
        if(notEqualJump != -1) patchJump(notEqualJump);
        expression();
        consume(TOKEN_COLON, "Expect ':' after case expression.");
        notEqualJump = emitJump(OP_JUMP_IF_NOT_EQUAL);

        while(!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) && !check(TOKEN_EOF)) {
            statement();
        }

        caseEnds[currentCase++] = emitJump(OP_JUMP);
    }

    if(match(TOKEN_DEFAULT)) {
        if(notEqualJump != -1) {
            patchJump(notEqualJump);
            notEqualJump = -1;
        }

        consume(TOKEN_COLON, "Expect ':' after case expression.");

        while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
            statement();
        }
    }

    consume(TOKEN_RIGHT_BRACE, "Expect switch statements to end with '}'");

    if(notEqualJump != -1) patchJump(notEqualJump);

    for(int i = 0; i < currentCase; i++) {
        patchJump(caseEnds[i]);
    }
    emitByte(OP_POP);
}

static void continueStatement() {
    if(innermostLoopStart == -1) {
        error("Expect 'continue' to appear within a loop");
        return;
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'");

    // clear local variables
    for (int i = current->localCount - 1;
       i >= 0 && current->locals[i].depth > innermostLoopScopeDepth;
       i--) {
        emitByte(OP_POP);
    }

    emitLoop(innermostLoopStart);
}

static void whileStatement() {
    int loopStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expect '(' before 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    statement();

    emitLoop(loopStart);

    patchJump(exitJump);

    emitByte(OP_POP);
    loopStart = -1;
}

static void synchronize() {
    parser.panicMode = false;

    while(parser.current.type != TOKEN_EOF) {
        if(parser.previous.type == TOKEN_SEMICOLON) break;

        switch(parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                break;
            default:
                ;
        }

        advance();
    }
}


static void block() {
    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' at the end of block.");
}

static void declaration() {
    if(match(TOKEN_VAR)) {
        varDeclaration();
    }  else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        statement();
    }
    
    if(parser.panicMode) synchronize();
}

static void statement() {
    if(match(TOKEN_PRINT)) {
        printStatement();
    } else if(match(TOKEN_CONTINUE)) {
        continueStatement();
    } else if(match(TOKEN_SWITCH)) {
        switchStatement();
    } else if(match(TOKEN_IF)){ 
        ifStatement();
    } else if(match(TOKEN_WHILE)) {
        whileStatement();
    } else if(match(TOKEN_FOR)) {
        forStatement();
    } else {
        expressionStatement();
    }
}

static void initCompiler(Compiler* compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);

    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while(!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();

    return !parser.hadError;
}
