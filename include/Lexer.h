#pragma once
#include "Common.h"
#include <cctype>
#include <iomanip>

class Lexer {
public:
    std::vector<Token> tokenize(const std::string& source) {
        std::vector<Token> tokens;
        size_t i = 0;

        while (i < source.length()) {
            char c = source[i];

            // ── Whitespace ──────────────────────────────────────────
            if (std::isspace(c)) { i++; continue; }

            // ── Single-line comments: skip from // to end of line ──
            // FIX: Previously // would cause a lex error or be tokenized
            // as two TOK_SLASH tokens. Now we detect // and skip the
            // rest of the line entirely before any other checks.
            if (c == '/' && i + 1 < source.length() && source[i + 1] == '/') {
                while (i < source.length() && source[i] != '\n') i++;
                continue;
            }

            // ── Single-char operators ────────────────────────────────
            if (c == '+') { tokens.push_back({TOK_PLUS,    {source.data() + i, 1}}); i++; continue; }
            if (c == '-') { tokens.push_back({TOK_MINUS,   {source.data() + i, 1}}); i++; continue; }
            if (c == '*') { tokens.push_back({TOK_STAR,    {source.data() + i, 1}}); i++; continue; }
            if (c == '/') { tokens.push_back({TOK_SLASH,   {source.data() + i, 1}}); i++; continue; }
            if (c == '%') { tokens.push_back({TOK_MODULO,  {source.data() + i, 1}}); i++; continue; }
            if (c == '^') { tokens.push_back({TOK_CARET,   {source.data() + i, 1}}); i++; continue; } // exponent
            if (c == '<') { tokens.push_back({TOK_LESS,    {source.data() + i, 1}}); i++; continue; }
            if (c == '>') { tokens.push_back({TOK_GREATER, {source.data() + i, 1}}); i++; continue; }

            // ── = vs == ─────────────────────────────────────────────
            if (c == '=') {
                if (i + 1 < source.length() && source[i + 1] == '=') {
                    tokens.push_back({TOK_EQUAL, {source.data() + i, 2}});
                    i += 2;
                } else {
                    tokens.push_back({TOK_ASSIGN, {source.data() + i, 1}});
                    i++;
                }
                continue;
            }

            if (c == ';') { tokens.push_back({TOK_SEMI,   {source.data() + i, 1}}); i++; continue; }
            if (c == '(') { tokens.push_back({TOK_LPAREN, {source.data() + i, 1}}); i++; continue; }
            if (c == ')') { tokens.push_back({TOK_RPAREN, {source.data() + i, 1}}); i++; continue; }
            if (c == '{') { tokens.push_back({TOK_LBRACE, {source.data() + i, 1}}); i++; continue; }
            if (c == '}') { tokens.push_back({TOK_RBRACE, {source.data() + i, 1}}); i++; continue; }

            // ── Integer literals ─────────────────────────────────────
            if (std::isdigit(c)) {
                size_t start = i;
                while (i < source.length() && std::isdigit(source[i])) i++;
                tokens.push_back({TOK_INT, {source.data() + start, i - start}});
                continue;
            }

            // ── Identifiers and keywords ─────────────────────────────
            if (std::isalpha(c) || c == '_') {
                size_t start = i;
                while (i < source.length() && (std::isalnum(source[i]) || source[i] == '_')) i++;
                std::string_view ident(source.data() + start, i - start);

                if      (ident == "let")      tokens.push_back({TOK_LET,      ident});
                else if (ident == "print")    tokens.push_back({TOK_PRINT,    ident});
                else if (ident == "if")       tokens.push_back({TOK_IF,       ident});
                else if (ident == "else")     tokens.push_back({TOK_ELSE,     ident});
                else if (ident == "while")    tokens.push_back({TOK_WHILE,    ident});
                else if (ident == "for")      tokens.push_back({TOK_FOR,      ident});
                else if (ident == "do")       tokens.push_back({TOK_DO,       ident});
                else if (ident == "break")    tokens.push_back({TOK_BREAK,    ident});
                else if (ident == "continue") tokens.push_back({TOK_CONTINUE, ident});
                else                          tokens.push_back({TOK_IDENT,    ident});

                continue;
            }

            // Unknown character — skip silently
            i++;
        }

        tokens.push_back({TOK_EOF, ""});
        std::cout << "[Lexer] Scanned and found " << tokens.size() - 1 << " valid source tokens.\n";
        return tokens;
    }
};