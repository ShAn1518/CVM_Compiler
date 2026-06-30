#pragma once
#include "Common.h"
#include "Parser.h"
#include <iomanip>

class Compiler {
    std::vector<uint8_t> code;
    std::unordered_map<std::string, uint8_t> vars;

    std::vector<std::vector<size_t>> breakStack;
    std::vector<std::vector<size_t>> continueStack;

    uint8_t varCount = 0;

    void emit(uint8_t b) { code.push_back(b); }

    // ── FIX: jump placeholders are now 2 bytes (uint16) ────────────
    // Old code used 1 byte for jump targets, limiting total bytecode
    // to 255 bytes. Now supports up to 65535 bytes of bytecode.
    size_t emitJump(uint8_t op) {
        emit(op);
        size_t pos = code.size();
        emitUint16(code, 0); // 2-byte placeholder
        return pos;          // return position of the placeholder
    }

    void patchJump(size_t pos) {
        patchUint16(code, pos, (uint16_t)code.size());
    }

    void patchJumpTo(size_t pos, size_t target) {
        patchUint16(code, pos, (uint16_t)target);
    }

    void expr(ExprNode* e) {
        if (!e) return;

        if (auto* n = dynamic_cast<IntNode*>(e)) {
            // FIX: OP_PUSH now encodes the value as 4 bytes (int32)
            // Old code: emit(OP_PUSH); emit(n->value);
            //   -> n->value was cast to uint8_t, so anything > 255 wrapped around.
            // New code: emit opcode, then emit 4-byte little-endian int32.
            emit(OP_PUSH);
            emitInt32(code, n->value);
        }
        else if (auto* v = dynamic_cast<VarNode*>(e)) {
            emit(OP_LOAD); emit(vars[std::string(v->name)]);
        }
        else if (auto* a = dynamic_cast<AddNode*>(e))     { expr(a->l); expr(a->r); emit(OP_ADD); }
        else if (auto* s = dynamic_cast<SubNode*>(e))     { expr(s->l); expr(s->r); emit(OP_SUB); }
        else if (auto* m = dynamic_cast<MulNode*>(e))     { expr(m->l); expr(m->r); emit(OP_MUL); }
        else if (auto* d = dynamic_cast<DivNode*>(e))     { expr(d->l); expr(d->r); emit(OP_DIV); }
        else if (auto* m = dynamic_cast<ModNode*>(e))     { expr(m->l); expr(m->r); emit(OP_MOD); }
        else if (auto* p = dynamic_cast<PowNode*>(e))     { expr(p->l); expr(p->r); emit(OP_POW); } // new
        else if (auto* l = dynamic_cast<LessNode*>(e))    { expr(l->l); expr(l->r); emit(OP_LESS); }
        else if (auto* g = dynamic_cast<GreaterNode*>(e)) { expr(g->l); expr(g->r); emit(OP_GREATER); }
        else if (auto* eq = dynamic_cast<EqualNode*>(e))  { expr(eq->l); expr(eq->r); emit(OP_EQUAL); }
    }

    void stmt(StmtNode* s) {
        if (!s) return;

        if (auto* l = dynamic_cast<LetNode*>(s)) {
            expr(l->expr);
            std::string nameStr = std::string(l->name);
            if (!vars.count(nameStr)) vars[nameStr] = varCount++;
            emit(OP_STORE); emit(vars[nameStr]);
        }

        else if (auto* p = dynamic_cast<PrintNode*>(s)) {
            expr(p->expr);
            emit(OP_PRINT);
        }

        else if (auto* i = dynamic_cast<IfNode*>(s)) {
            expr(i->cond);
            size_t jFalse = emitJump(OP_JUMP_IF_FALSE);
            for (auto* st : i->thenBlock->stmts) stmt(st);
            size_t jEnd = emitJump(OP_JUMP);
            patchJump(jFalse);
            if (i->elseBlock)
                for (auto* st : i->elseBlock->stmts) stmt(st);
            patchJump(jEnd);
        }

        else if (auto* w = dynamic_cast<WhileNode*>(s)) {
            size_t loopStart = code.size();
            breakStack.push_back({});
            continueStack.push_back({});

            expr(w->cond);
            size_t jFalse = emitJump(OP_JUMP_IF_FALSE);
            for (auto* st : w->body->stmts) stmt(st);

            // unconditional back-jump — also 2 bytes now
            emit(OP_JUMP);
            emitUint16(code, (uint16_t)loopStart);

            patchJump(jFalse);
            for (auto pos : breakStack.back())    patchJump(pos);
            for (auto pos : continueStack.back()) patchJumpTo(pos, loopStart);
            breakStack.pop_back();
            continueStack.pop_back();
        }

        else if (auto* d = dynamic_cast<DoWhileNode*>(s)) {
            size_t loopStart = code.size();
            breakStack.push_back({});
            continueStack.push_back({});

            for (auto* st : d->body->stmts) stmt(st);

            size_t condStart = code.size();
            expr(d->cond);
            size_t jFalse = emitJump(OP_JUMP_IF_FALSE);

            emit(OP_JUMP);
            emitUint16(code, (uint16_t)loopStart);

            patchJump(jFalse);
            for (auto pos : breakStack.back())    patchJump(pos);
            for (auto pos : continueStack.back()) patchJumpTo(pos, condStart);
            breakStack.pop_back();
            continueStack.pop_back();
        }

        else if (auto* f = dynamic_cast<ForNode*>(s)) {
            breakStack.push_back({});
            continueStack.push_back({});

            if (f->init) stmt(f->init);

            size_t loopStart = code.size();
            size_t jFalse    = (size_t)-1;
            if (f->cond) {
                expr(f->cond);
                jFalse = emitJump(OP_JUMP_IF_FALSE);
            }

            for (auto* st : f->body->stmts) stmt(st);

            size_t incStart = code.size();
            if (f->inc) stmt(f->inc);

            emit(OP_JUMP);
            emitUint16(code, (uint16_t)loopStart);

            if (jFalse != (size_t)-1) patchJump(jFalse);
            for (auto pos : breakStack.back())    patchJump(pos);
            for (auto pos : continueStack.back()) patchJumpTo(pos, incStart);
            breakStack.pop_back();
            continueStack.pop_back();
        }

        else if (dynamic_cast<BreakNode*>(s)) {
            size_t j = emitJump(OP_JUMP);
            breakStack.back().push_back(j);
        }
        else if (dynamic_cast<ContinueNode*>(s)) {
            size_t j = emitJump(OP_JUMP);
            continueStack.back().push_back(j);
        }
    }

public:
    void dumpBytecode() const {
        std::cout << "\n--- Generated Bytecode Blueprint ---\n";
        for (size_t i = 0; i < code.size(); ) {
            std::cout << "  Offset " << std::setw(3) << std::setfill('0') << i << ": ";
            uint8_t op = code[i];
            std::cout << std::left << std::setw(18) << opcodeToString(op);

            if (op == OP_PUSH) {
                // 4-byte int32 operand
                int32_t val = (int32_t)(code[i+1] | (code[i+2]<<8) | (code[i+3]<<16) | (code[i+4]<<24));
                std::cout << " [Operand: " << val << "]";
                i += 5;
            } else if (op == OP_STORE || op == OP_LOAD) {
                // 1-byte slot
                std::cout << " [Slot: " << (int)code[i+1] << "]";
                i += 2;
            } else if (op == OP_JUMP || op == OP_JUMP_IF_FALSE) {
                // 2-byte uint16 address
                uint16_t addr = (uint16_t)(code[i+1] | (code[i+2] << 8));
                std::cout << " [Addr: " << addr << "]";
                i += 3;
            } else {
                i += 1;
            }
            std::cout << "\n";
        }
        std::cout << "------------------------------------\n";
    }

    std::vector<uint8_t> compile(const std::vector<StmtNode*>& ast) {
        code.clear();
        vars.clear();
        varCount = 0;
        code.reserve(1024);

        for (auto* s : ast) stmt(s);
        emit(OP_HALT);

        dumpBytecode();
        return code;
    }
};