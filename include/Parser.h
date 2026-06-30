#pragma once
#include "Common.h"

// =====================
//  ARENA ALLOCATOR
// =====================
class Arena {
    std::vector<std::unique_ptr<char[]>> blocks;
    size_t blockSize = 4096;
    char* current = nullptr;
    size_t offset = 0;

public:
    void* allocate(size_t size) {
        if (!current || offset + size > blockSize) {
            blocks.push_back(std::make_unique<char[]>(blockSize));
            current = blocks.back().get();
            offset = 0;
        }
        void* ptr = current + offset;
        offset += size;
        return ptr;
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
};

// =====================
//  AST NODES
// =====================
struct ASTNode  { virtual ~ASTNode() = default; };
struct ExprNode : ASTNode {};

struct IntNode : ExprNode {
    int value;
    IntNode(int v) : value(v) {}
};

struct VarNode : ExprNode {
    std::string_view name;
    VarNode(std::string_view n) : name(n) {}
};

// Macro to declare all binary expression nodes
#define BIN_NODE(name) \
struct name : ExprNode { \
    ExprNode* l; \
    ExprNode* r; \
    name(ExprNode* a, ExprNode* b) : l(a), r(b) {} \
};

BIN_NODE(AddNode)
BIN_NODE(SubNode)
BIN_NODE(MulNode)
BIN_NODE(DivNode)
BIN_NODE(ModNode)
BIN_NODE(PowNode)       // new: base ^ exponent
BIN_NODE(LessNode)
BIN_NODE(GreaterNode)
BIN_NODE(EqualNode)

// =====================
//  STATEMENTS
// =====================
struct StmtNode : ASTNode {};

struct LetNode : StmtNode {
    std::string_view name;
    ExprNode* expr;
    LetNode(std::string_view n, ExprNode* e) : name(n), expr(e) {}
};

struct PrintNode : StmtNode {
    ExprNode* expr;
    PrintNode(ExprNode* e) : expr(e) {}
};

struct BlockNode : StmtNode {
    std::vector<StmtNode*> stmts;
};

struct WhileNode : StmtNode {
    ExprNode* cond;
    BlockNode* body;
};

struct DoWhileNode : StmtNode {
    BlockNode* body;
    ExprNode* cond;
};

struct ForNode : StmtNode {
    StmtNode* init;
    ExprNode* cond;
    StmtNode* inc;
    BlockNode* body;
};

struct BreakNode    : StmtNode {};
struct ContinueNode : StmtNode {};

struct IfNode : StmtNode {
    ExprNode* cond;
    BlockNode* thenBlock;
    BlockNode* elseBlock;
};

// =====================
//  PARSER
// =====================
class Parser {
    std::vector<Token> tokens;
    size_t current = 0;
    Arena arena;

    Token advance() { return tokens[current++]; }
    Token peek()    { return tokens[current]; }

    bool match(TokenType t) {
        if (peek().type == t) { advance(); return true; }
        return false;
    }

    // ── Precedence levels (low → high) ──────────────────────────────
    //   comparison  <  >  ==
    //   additive    +  -
    //   term        *  /  %
    //   power       ^          (right-associative)
    //   primary     int  var  ( expr )

    ExprNode* primary() {
        if (match(TOK_INT))
            return arena.create<IntNode>(std::stoi(std::string(tokens[current - 1].lexeme)));

        if (match(TOK_IDENT))
            return arena.create<VarNode>(tokens[current - 1].lexeme);

        if (match(TOK_LPAREN)) {
            auto* e = expression();
            match(TOK_RPAREN);
            return e;
        }
        return nullptr;
    }

    // Power is right-associative: 2^3^2 == 2^(3^2) == 512
    // We achieve that by recursing on the right side instead of looping.
    ExprNode* power() {
        auto* left = primary();
        if (match(TOK_CARET)) {
            // FIX: recurse here (not loop) so ^ is right-associative
            auto* right = power();
            return arena.create<PowNode>(left, right);
        }
        return left;
    }

    ExprNode* term() {
        auto* left = power();
        while (true) {
            if      (match(TOK_STAR))   left = arena.create<MulNode>(left, power());
            else if (match(TOK_SLASH))  left = arena.create<DivNode>(left, power());
            else if (match(TOK_MODULO)) left = arena.create<ModNode>(left, power());
            else break;
        }
        return left;
    }

    ExprNode* expression() {
        auto* left = term();
        while (true) {
            if      (match(TOK_PLUS))    left = arena.create<AddNode>(left, term());
            else if (match(TOK_MINUS))   left = arena.create<SubNode>(left, term());
            else if (match(TOK_LESS))    left = arena.create<LessNode>(left, term());
            else if (match(TOK_GREATER)) left = arena.create<GreaterNode>(left, term());
            else if (match(TOK_EQUAL))   left = arena.create<EqualNode>(left, term());
            else break;
        }
        return left;
    }

    BlockNode* block() {
        auto* b = arena.create<BlockNode>();
        match(TOK_LBRACE);
        while (!match(TOK_RBRACE) && peek().type != TOK_EOF) {
            auto* s = statement();
            if (s) b->stmts.push_back(s);
            else {
                std::cerr << "Syntax Error near: " << peek().lexeme << "\n";
                advance();
            }
        }
        return b;
    }

    StmtNode* statement() {
        // let x = expr;
        if (match(TOK_LET)) {
            std::string_view name = advance().lexeme;
            match(TOK_ASSIGN);
            auto* e = expression();
            match(TOK_SEMI);
            return arena.create<LetNode>(name, e);
        }

        // x = expr;  (reassignment without let)
        if (peek().type == TOK_IDENT &&
            current + 1 < tokens.size() &&
            tokens[current + 1].type == TOK_ASSIGN) {
            std::string_view name = advance().lexeme;
            match(TOK_ASSIGN);
            auto* e = expression();
            match(TOK_SEMI);
            return arena.create<LetNode>(name, e);
        }

        // print expr;
        if (match(TOK_PRINT)) {
            auto* e = expression();
            match(TOK_SEMI);
            return arena.create<PrintNode>(e);
        }

        // while (cond) { ... }
        if (match(TOK_WHILE)) {
            match(TOK_LPAREN);
            auto* cond = expression();
            match(TOK_RPAREN);
            auto* body = block();
            auto* node = arena.create<WhileNode>();
            node->cond = cond;
            node->body = body;
            return node;
        }

        // do { ... } while (cond);
        if (match(TOK_DO)) {
            auto* body = block();
            match(TOK_WHILE);
            match(TOK_LPAREN);
            auto* cond = expression();
            match(TOK_RPAREN);
            match(TOK_SEMI);
            auto* node = arena.create<DoWhileNode>();
            node->body = body;
            node->cond = cond;
            return node;
        }

        // for (init; cond; inc) { ... }
        if (match(TOK_FOR)) {
            match(TOK_LPAREN);
            auto* init = statement();
            auto* cond = expression();
            match(TOK_SEMI);

            StmtNode* inc = nullptr;
            if (peek().type == TOK_IDENT &&
                current + 1 < tokens.size() &&
                tokens[current + 1].type == TOK_ASSIGN) {
                std::string_view name = advance().lexeme;
                match(TOK_ASSIGN);
                auto* e = expression();
                inc = arena.create<LetNode>(name, e);
            }

            match(TOK_RPAREN);
            auto* body = block();
            auto* node = arena.create<ForNode>();
            node->init = init;
            node->cond = cond;
            node->inc  = inc;
            node->body = body;
            return node;
        }

        if (match(TOK_BREAK)) {
            match(TOK_SEMI);
            return arena.create<BreakNode>();
        }

        if (match(TOK_CONTINUE)) {
            match(TOK_SEMI);
            return arena.create<ContinueNode>();
        }

        // if (cond) { ... } else { ... }
        if (match(TOK_IF)) {
            match(TOK_LPAREN);
            auto* cond = expression();
            match(TOK_RPAREN);
            auto* thenBlock = block();
            BlockNode* elseBlock = nullptr;
            if (match(TOK_ELSE)) elseBlock = block();
            auto* node = arena.create<IfNode>();
            node->cond      = cond;
            node->thenBlock = thenBlock;
            node->elseBlock = elseBlock;
            return node;
        }

        return nullptr;
    }

public:
    std::vector<StmtNode*> parse(std::vector<Token> t) {
        tokens  = std::move(t);
        current = 0;

        std::vector<StmtNode*> out;
        while (peek().type != TOK_EOF) {
            auto* s = statement();
            if (s) out.push_back(s);
            else {
                std::cerr << "Syntax Error near: " << peek().lexeme << "\n";
                advance();
            }
        }
        std::cout << "[Parser] Constructed AST containing " << out.size() << " base statements.\n";
        return out;
    }
};