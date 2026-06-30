#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdint>

// =====================
//  TOKENS
// =====================
enum TokenType {
    TOK_LET, TOK_PRINT,
    TOK_IF, TOK_ELSE,
    TOK_FOR, TOK_DO,

    TOK_IDENT, TOK_INT,

    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
    TOK_ASSIGN, TOK_EQUAL, TOK_SEMI,

    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,

    TOK_LESS, TOK_GREATER,

    TOK_WHILE,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_MODULO,

    TOK_CARET,  // '^' exponent operator

    TOK_EOF
};

struct Token {
    TokenType type;
    std::string_view lexeme; // zero-copy string value
};

// =====================
//  OPCODES
// =====================
enum Opcode : uint8_t {
    // FIX: OP_PUSH now pushes a 4-byte int32 (4 bytes follow in bytecode)
    // Old OP_PUSH only stored 1 byte (uint8_t), capping values at 255.
    OP_PUSH,

    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_LESS, OP_GREATER, OP_EQUAL,

    OP_STORE, OP_LOAD,  // operand: 1-byte variable slot index (max 255 vars)
    OP_PRINT,

    // FIX: OP_JUMP and OP_JUMP_IF_FALSE now use a 2-byte uint16 address
    // Old versions used 1 byte, limiting bytecode to 255 bytes total.
    OP_JUMP,
    OP_JUMP_IF_FALSE,

    OP_MOD,
    OP_POW,  // new: exponent (base ^ exp)

    OP_HALT
};

// Utility: translate opcode byte to readable name
inline const char* opcodeToString(uint8_t op) {
    switch (op) {
        case OP_PUSH:          return "OP_PUSH";
        case OP_ADD:           return "OP_ADD";
        case OP_SUB:           return "OP_SUB";
        case OP_MUL:           return "OP_MUL";
        case OP_DIV:           return "OP_DIV";
        case OP_MOD:           return "OP_MOD";
        case OP_POW:           return "OP_POW";
        case OP_LESS:          return "OP_LESS";
        case OP_GREATER:       return "OP_GREATER";
        case OP_EQUAL:         return "OP_EQUAL";
        case OP_STORE:         return "OP_STORE";
        case OP_LOAD:          return "OP_LOAD";
        case OP_PRINT:         return "OP_PRINT";
        case OP_JUMP:          return "OP_JUMP";
        case OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE";
        case OP_HALT:          return "OP_HALT";
        default:               return "OP_UNKNOWN";
    }
}

// =====================
//  BYTECODE HELPERS
//  For encoding/decoding multi-byte values in the bytecode stream
// =====================

// Encode a 32-bit int into 4 bytes (little-endian) and append to bytecode
inline void emitInt32(std::vector<uint8_t>& code, int32_t value) {
    code.push_back((value >>  0) & 0xFF);
    code.push_back((value >>  8) & 0xFF);
    code.push_back((value >> 16) & 0xFF);
    code.push_back((value >> 24) & 0xFF);
}

// Read a 32-bit int from bytecode at position ip, advance ip by 4
inline int32_t readInt32(const std::vector<uint8_t>& code, size_t& ip) {
    int32_t val = 0;
    val |= (int32_t)code[ip + 0] <<  0;
    val |= (int32_t)code[ip + 1] <<  8;
    val |= (int32_t)code[ip + 2] << 16;
    val |= (int32_t)code[ip + 3] << 24;
    ip += 4;
    return val;
}

// Encode a 16-bit address into 2 bytes (little-endian) and append to bytecode
inline void emitUint16(std::vector<uint8_t>& code, uint16_t value) {
    code.push_back((value >> 0) & 0xFF);
    code.push_back((value >> 8) & 0xFF);
}

// Read a 16-bit address from bytecode at position ip, advance ip by 2
inline uint16_t readUint16(const std::vector<uint8_t>& code, size_t& ip) {
    uint16_t val = 0;
    val |= (uint16_t)code[ip + 0] << 0;
    val |= (uint16_t)code[ip + 1] << 8;
    ip += 2;
    return val;
}

// Patch a 16-bit jump address at a previously emitted placeholder position
inline void patchUint16(std::vector<uint8_t>& code, size_t pos, uint16_t value) {
    code[pos + 0] = (value >> 0) & 0xFF;
    code[pos + 1] = (value >> 8) & 0xFF;
}