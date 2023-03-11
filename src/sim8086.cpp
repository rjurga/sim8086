#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <assert.h>
#include <stdint.h>

const char c_registerNames[2][8][3] =
{
    {
        { "al" },
        { "cl" },
        { "dl" },
        { "bl" },
        { "ah" },
        { "ch" },
        { "dh" },
        { "bh" },
    },
    {
        { "ax" },
        { "cx" },
        { "dx" },
        { "bx" },
        { "sp" },
        { "bp" },
        { "si" },
        { "di" },
    }
};

const char c_effectiveAddresses[8][8] =
{
    { "bx + si" },
    { "bx + di" },
    { "bp + si" },
    { "bp + di" },
    { "si" },
    { "di" },
    { "bp" },
    { "bx" },
};

const char c_instructionNamesFor100000[8][4] =
{
    "add", // reg = 000
    "or",  // reg = 001
    "adc", // reg = 010
    "sbb", // reg = 011
    "and", // reg = 100
    "sub", // reg = 101
    "xor", // reg = 110
    "cmp", // reg = 111
};

bool tryReadFile(char* pFileName, std::vector<uint8_t>* pResult)
{
    std::ifstream file;
    file.open(pFileName, std::ifstream::binary | std::ifstream::in);
    if (!file.good())
    {
        std::cout << "Failed to open file " << pFileName << ": " << std::strerror(errno) << "\n";
        return false;
    }

    *pResult = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    return true;
}

// Fills the effectiveAddress string with the computed asm representation of the effective address.
// Returns the size in bytes of the the displacement field.

int computeEffectiveAddress(
    std::vector<uint8_t>::const_iterator byte,
    uint8_t mod,
    uint8_t rm,
    std::string* pEffectiveAddress)
{
    assert(pEffectiveAddress);

    // Returns the increment to the instruction pointer that accounts for the size of the displacement.

    int displacementSize = 0;

    int16_t displacement = 0;
    const char* baseAndIndex = c_effectiveAddresses[rm];

    switch (mod)
    {
        case 0b00:
        {
            // Memory mode, no displacement follows

            if (rm == 0b110)
            {
                // Except when R/M = 110, then 16-bit displacement follows, direct address

                baseAndIndex = nullptr;
                displacement = (byte[3] << 8) | byte[2];

                displacementSize = 2;
            }
        }
        break;

        case 0b01:
        {
            // Memory mode, 8-bit displacement follows

            displacement = static_cast<int8_t>(byte[2]);
            displacementSize = 1;
        }
        break;

        case 0b10:
        {
            // Memory mode, 16-bit displacement follows

            displacement = (byte[3] << 8) | byte[2];
            displacementSize = 2;
        }
        break;

        case 0b11:
        {
            // Register mode, no effective address

            assert(false);
        }
        break;
    }

    // Write the effective address to string

    *pEffectiveAddress += "[";

    if (baseAndIndex)
    {
        *pEffectiveAddress += baseAndIndex;

        if (displacement)
        {
            if (displacement > 0)
            {
                *pEffectiveAddress += " + ";
            }
            else
            {
                *pEffectiveAddress += " - ";
                displacement = -displacement;
            }
        }
    }

    if (displacement)
    {
        *pEffectiveAddress += std::to_string(displacement);
    }

    *pEffectiveAddress += "]";

    return displacementSize;
}

// Fills the destination and source strings for instructions of the form "reg/memory and register to either".
// Returns the increment that moves the instruction pointer to the next instruction.

int decodeRegOrMemToRegOrMem(
    std::vector<uint8_t>::const_iterator byte,
    uint8_t d,
    uint8_t w,
    std::string* pDestination,
    std::string* pSource)
{
    assert(pDestination);
    assert(pSource);

    int instructionPointerIncrement = 2;

    uint8_t mod =  byte[1] >> 6;
    uint8_t reg = (byte[1] >> 3) & 7;
    uint8_t rm  =  byte[1]       & 7;

    std::string& registerName = d ? *pDestination : *pSource;
    std::string& otherName    = d ? *pSource      : *pDestination;

    registerName = c_registerNames[w][reg];

    if (mod == 0b11)
    {
        // Register mode (no displacement)

        otherName = c_registerNames[w][rm];
    }
    else
    {
        instructionPointerIncrement += computeEffectiveAddress(byte, mod, rm, &otherName);
    }

    return instructionPointerIncrement;
}

// Appends the computed immediate value to the output string.
// Returns the size in bytes of the data field containing the immediate value.

int computeImmediate(std::vector<uint8_t>::const_iterator data, uint8_t s, uint8_t w, std::string* pImmediate)
{
    int dataSize = 1;

    int16_t immediate = 0;

    if (w)
    {
        // Value is word.

        if (s)
        {
            // Sign extend 8-bit immediate data to 16-bits.

            immediate = static_cast<int8_t>(data[0]);
        }
        else
        {
            // No sign extension.

            immediate = (data[1] << 8) | data[0];
            dataSize = 2;
        }
    }
    else
    {
        // Value is 8-bits, no sign extension.

        immediate = static_cast<uint16_t>(data[0]);
    }

    *pImmediate += std::to_string(immediate);

    return dataSize;
}

// Fills the destination and source strings for instructions of the form "immediate to register/memory".
// Returns the increment that moves the instruction pointer to the next instruction.

int decodeImmToRegOrMem(
    std::vector<uint8_t>::const_iterator byte,
    uint8_t s,
    uint8_t w,
    std::string* pDestination,
    std::string* pSource)
{
    assert(pDestination);
    assert(pSource);

    uint8_t mod = byte[1] >> 6;
    uint8_t rm  = byte[1] & 7;

    int displacementSize = 0;

    if (mod == 0b11)
    {
        *pDestination = c_registerNames[w][rm];
    }
    else
    {
        *pSource = w ? "word " : "byte ";
        displacementSize += computeEffectiveAddress(byte, mod, rm, pDestination);
    }

    const auto data = byte + 2 + displacementSize;
    int dataSize = computeImmediate(data, s, w, pSource);

    int instructionPointerIncrement = 2 + displacementSize + dataSize;
    return instructionPointerIncrement;
}

// Decodes the machine code instructions in input and writes the disassembly in pOutput.

void decodeInstructions(const std::vector<uint8_t>& input, std::string* pOutput)
{
    *pOutput += "bits 16\n\n";

    for (auto iInstruction = input.begin(), end = input.end(); iInstruction < end;)
    {
        std::string instructionName;
        std::string destination;
        std::string source;

        const auto byte = iInstruction;

        // Extract instruction fields

        uint8_t opcode =  byte[0] >> 2;
        uint8_t d      = (byte[0] >> 1) & 1;
        uint8_t w      =  byte[0]       & 1;

        // Disassemble instruction

        switch (opcode)
        {
            // Instructions of the form "reg/memory and register to either"

            case 0b000000: [[fallthrough]];
            case 0b000010: [[fallthrough]];
            case 0b000100: [[fallthrough]];
            case 0b000110: [[fallthrough]];
            case 0b001000: [[fallthrough]];
            case 0b001010: [[fallthrough]];
            case 0b001100: [[fallthrough]];
            case 0b001110:
            {
                int iInstructionName = (opcode >> 1) & 7;
                instructionName = c_instructionNamesFor100000[iInstructionName];

                iInstruction += decodeRegOrMemToRegOrMem(byte, d, w, &destination, &source);
            }
            break;

            // Instructions of the form "immediate to accumulator"

            case 0b000001: [[fallthrough]];
            case 0b000011: [[fallthrough]];
            case 0b000101: [[fallthrough]];
            case 0b000111: [[fallthrough]];
            case 0b001001: [[fallthrough]];
            case 0b001011: [[fallthrough]];
            case 0b001101: [[fallthrough]];
            case 0b001111:
            {
                int iInstructionName = (opcode >> 1) & 7;
                instructionName = c_instructionNamesFor100000[iInstructionName];

                destination = w ? "ax" : "al";

                const auto data = byte + 1;
                int dataSize = computeImmediate(data, d, w, &source);

                iInstruction += 1 + dataSize;
            }
            break;

            // MOV register/memory to/from register

            case 0b100010:
            {
                instructionName = "mov";
                iInstruction += decodeRegOrMemToRegOrMem(byte, d, w, &destination, &source);
            }
            break;

            // Instructions of the form "immediate to register/memory"

            case 0b100000:
            {
                uint8_t reg = (byte[1] >> 3) & 7;
                instructionName = c_instructionNamesFor100000[reg];
                iInstruction += decodeImmToRegOrMem(byte, d, w, &destination, &source);
            }
            break;

            // MOV memory to/from accumulator

            case 0b101000:
            {
                instructionName = "mov";

                std::string& effectiveAddress = d ? destination : source;
                std::string& accumulator      = d ? source      : destination;

                accumulator = w ? "ax" : "al";

                uint16_t addr = (byte[2] << 8) | byte[1];
                effectiveAddress = "[" + std::to_string(addr) + "]";

                iInstruction += 3;
            }
            break;

            // MOV immediate to register

            case 0b101100: [[fallthrough]];
            case 0b101101: [[fallthrough]];
            case 0b101110: [[fallthrough]];
            case 0b101111:
            {
                instructionName = "mov";

                w = (byte[0] >> 3) & 1;
                uint8_t reg = byte[0] & 7;

                destination = c_registerNames[w][reg];

                const auto data = byte + 1;
                int dataSize = computeImmediate(data, 0, w, &source);

                iInstruction += 1 + dataSize;
            }
            break;

            // MOV immediate to register/memory

            case 0b110001:
            {
                instructionName = "mov";
                iInstruction += decodeImmToRegOrMem(byte, d, w, &destination, &source);
            }
            break;

            default:
            {
                std::cerr << "Error decoding instruction: unknown opcode\n";
                return;
            }
            break;
        }

        // Print the asm instruction

        *pOutput += instructionName;

        if (!destination.empty())
        {
            *pOutput += " ";
            *pOutput += destination;
        }

        if (!source.empty())
        {
            assert(!destination.empty());

            *pOutput += ", ";
            *pOutput += source;
        }

        *pOutput += "\n";
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: sim8086 [file]\n";
        return 0;
    }

    // Read input

    std::vector<uint8_t> input;
    if (!tryReadFile(argv[1], &input))
    {
        return 0;
    }

    // Do the work

    std::string output;
    decodeInstructions(input, &output);

    // Write output

    std::cout << output;
}
