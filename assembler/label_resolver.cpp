#include "label_resolver.h"

#include "../error.h"

void resolveLabel(
    Instruction& inst,
    uint32_t pc,
    const SymbolTable& symbols,
    int lineNumber
) {
    if (inst.label.empty()) {
        return;
    }

    auto it = symbols.find(inst.label);

    if (it == symbols.end()) {
        error(inst.label, "Undefined label", lineNumber);
    }

    uint32_t targetAddress = it->second;

    if (inst.mnemonic == "beq" ||
        inst.mnemonic == "bne") {

        int64_t target =
            static_cast<int64_t>(targetAddress);

        int64_t nextPc =
            static_cast<int64_t>(pc) + 4;

        int64_t offset =
            (target - nextPc) / 4;

        if (offset < -32768 || offset > 32767) {
            error(
                inst.label,
                "Branch target out of range",
                lineNumber
            );
        }

        inst.immediate =
            static_cast<int32_t>(offset);
    }

    else if (inst.mnemonic == "j" ||
        inst.mnemonic == "jal") {

        if (targetAddress % 4 != 0) {
            error(
                inst.label,
                "Jump target is not word aligned",
                lineNumber
            );
        }

        uint32_t nextPc = pc + 4;

        if ((targetAddress & 0xF0000000) !=
            (nextPc & 0xF0000000)) {

            error(
                inst.label,
                "Jump target is outside current 256 MB region",
                lineNumber
            );
        }

        inst.target =
            (targetAddress >> 2) & 0x03FFFFFF;
    }
}