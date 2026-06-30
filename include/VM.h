#pragma once
#include "Common.h"
#include <iomanip>
#include <cmath>        // for std::pow
#include <unordered_map>

// ==========================================
//  BRANCH PREDICTOR (BTB & BPB) 2-bit dynamic
// ==========================================
enum PredictorState {
    STRONGLY_NOT_TAKEN = 0,
    WEAKLY_NOT_TAKEN   = 1,
    WEAKLY_TAKEN       = 2,
    STRONGLY_TAKEN     = 3
};

struct BranchEntry {
    size_t targetAddress;
    PredictorState state;
    BranchEntry() : targetAddress(0), state(WEAKLY_NOT_TAKEN) {}
};

class BranchPredictor {
public:
    std::unordered_map<size_t, BranchEntry> table;
    int correct = 0;
    int total   = 0;

    bool predict(size_t pc, size_t& predictedTarget) {
        if (table.find(pc) == table.end()) return false;
        BranchEntry& entry = table[pc];
        bool isTaken = (entry.state == WEAKLY_TAKEN || entry.state == STRONGLY_TAKEN);
        if (isTaken) predictedTarget = entry.targetAddress;
        return isTaken;
    }

    void update(size_t pc, size_t actualTarget, bool actuallyTaken, bool predictedTaken) {
        total++;
        if (actuallyTaken == predictedTaken) correct++;
        if (table.find(pc) == table.end()) table[pc] = BranchEntry();
        BranchEntry& entry = table[pc];
        if (actuallyTaken) entry.targetAddress = actualTarget;
        if (actuallyTaken) {
            if (entry.state != STRONGLY_TAKEN)
                entry.state = static_cast<PredictorState>(entry.state + 1);
        } else {
            if (entry.state != STRONGLY_NOT_TAKEN)
                entry.state = static_cast<PredictorState>(entry.state - 1);
        }
    }

    void printStats() {
        std::cout << "\n--- Hardware Simulation Stats ---\n";
        std::cout << "Branch Predictions: " << total   << "\n";
        std::cout << "Correct Guesses:    " << correct << "\n";
        if (total > 0) {
            std::cout << "Accuracy:           "
                      << std::fixed << std::setprecision(2)
                      << ((float)correct / total * 100.0f) << "%\n";
        }
    }
};

// ==========================================
//  VIRTUAL MACHINE
// ==========================================
class VM {
    int    stack[1024]    = {};
    int    sp             = 0;
    int    variables[256] = {};

    BranchPredictor predictor;
    size_t simulatedCycles = 0;

    inline void push(int val)  { stack[sp++] = val; }
    inline int  pop()          { return stack[--sp]; }

public:
    void run(const std::vector<uint8_t>& bytecode) {
        size_t ip = 0;

        std::cout << "\n--- Step-by-Step Bytecode Execution Trace ---\n";
        std::cout << std::left
                  << std::setw(6)  << "PC"
                  << std::setw(18) << "Instruction"
                  << std::setw(12) << "Operand"
                  << "Stack State\n";
        std::cout << std::string(60, '-') << "\n";

        for (;;) {
            simulatedCycles++;
            size_t current_ip = ip;

            uint8_t instruction = bytecode[ip++];

            // ── Trace line prefix ──────────────────────────────────
            std::cout << std::left << std::setw(6)  << current_ip;
            std::cout << std::setw(18) << opcodeToString(instruction);

            // Print operand for instructions that have one (before executing)
            std::string opStr;
            if (instruction == OP_PUSH) {
                // peek 4-byte int32 without advancing ip yet
                int32_t val = (int32_t)(bytecode[ip] | (bytecode[ip+1]<<8)
                                      | (bytecode[ip+2]<<16) | (bytecode[ip+3]<<24));
                opStr = std::to_string(val);
            } else if (instruction == OP_STORE || instruction == OP_LOAD) {
                opStr = "slot=" + std::to_string((int)bytecode[ip]);
            } else if (instruction == OP_JUMP || instruction == OP_JUMP_IF_FALSE) {
                uint16_t addr = (uint16_t)(bytecode[ip] | (bytecode[ip+1] << 8));
                opStr = "addr=" + std::to_string(addr);
            }
            std::cout << std::setw(12) << opStr;

            // Print stack state before executing this instruction
            std::cout << "[";
            for (int s = 0; s < sp; ++s)
                std::cout << stack[s] << (s == sp - 1 ? "" : ", ");
            std::cout << "]\n";

            // ── Execute ────────────────────────────────────────────
            switch (instruction) {

                case OP_PUSH: {
                    // FIX: read 4-byte int32 instead of 1-byte uint8
                    // Old: push(bytecode[ip++])  -> max value 255
                    // New: read full 32-bit signed integer
                    int32_t val = readInt32(bytecode, ip);
                    push(val);
                    break;
                }

                case OP_ADD: { int b = pop(); int a = pop(); push(a + b); break; }
                case OP_SUB: { int b = pop(); int a = pop(); push(a - b); break; }
                case OP_MUL: { int b = pop(); int a = pop(); push(a * b); break; }

                case OP_DIV: {
                    int b = pop(); int a = pop();
                    if (b == 0) {
                        std::cerr << "\nRuntime Error: Division by zero at PC " << current_ip << "\n";
                        return;
                    }
                    push(a / b);
                    break;
                }

                case OP_MOD: {
                    int b = pop(); int a = pop();
                    if (b == 0) {
                        std::cerr << "\nRuntime Error: Modulo by zero at PC " << current_ip << "\n";
                        return;
                    }
                    push(a % b);
                    break;
                }

                case OP_POW: {
                    // new: integer exponentiation using std::pow
                    int b = pop(); int a = pop();
                    push((int)std::pow((double)a, (double)b));
                    break;
                }

                case OP_LESS:    { int b = pop(); int a = pop(); push(a <  b); break; }
                case OP_GREATER: { int b = pop(); int a = pop(); push(a >  b); break; }
                case OP_EQUAL:   { int b = pop(); int a = pop(); push(a == b); break; }

                case OP_STORE: {
                    uint8_t slot = bytecode[ip++];
                    variables[slot] = pop();
                    std::cout << "       (Var Slot " << (int)slot
                              << " updated to: " << variables[slot] << ")\n";
                    break;
                }

                case OP_LOAD: {
                    uint8_t slot = bytecode[ip++];
                    push(variables[slot]);
                    break;
                }

                case OP_PRINT: {
                    std::cout << ">>> VM OUTPUT: " << pop() << " <<<\n";
                    break;
                }

                case OP_HALT: {
                    std::cout << "\nExecution finished cleanly in "
                              << simulatedCycles << " simulated CPU cycles.\n";
                    predictor.printStats();
                    return;
                }

                case OP_JUMP: {
                    // FIX: read 2-byte uint16 address instead of 1-byte uint8
                    ip = readUint16(bytecode, ip);
                    break;
                }

                case OP_JUMP_IF_FALSE: {
                    int cond = pop();
                    // FIX: read 2-byte uint16 address
                    uint16_t target = readUint16(bytecode, ip);

                    size_t predictedTarget = 0;
                    bool predictedTaken  = predictor.predict(current_ip, predictedTarget);
                    bool actuallyTaken   = (cond == 0);

                    std::cout << "       (Predictor: "
                              << (predictedTaken ? "TAKEN" : "NOT_TAKEN")
                              << " | Actual: "
                              << (actuallyTaken  ? "TAKEN" : "NOT_TAKEN");

                    if (predictedTaken != actuallyTaken) {
                        simulatedCycles += 5;
                        std::cout << " -> Pipeline Flush! +5 Cycle Penalty";
                    }
                    std::cout << ")\n";

                    if (actuallyTaken) ip = target;
                    predictor.update(current_ip, target, actuallyTaken, predictedTaken);
                    break;
                }

                default:
                    std::cerr << "Runtime Error: Unknown opcode "
                              << (int)instruction << " at PC " << current_ip << "\n";
                    return;
            }
        }
    }
};