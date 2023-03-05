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

bool tryReadFile(char* fileName, std::vector<uint8_t>* result)
{
    std::ifstream file;
    file.open(fileName, std::ifstream::binary | std::ifstream::in);
    if (!file.good())
    {
        std::cout << "Failed to open file " << fileName << ": " << std::strerror(errno) << "\n";
        return false;
    }

    *result = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    return true;
}

size_t computeEffectiveAddress(const uint8_t* byte, uint8_t mod, uint8_t rm, std::string *effectiveAddress)
{
    // Returns the increment to the instruction pointer that accounts for the size of the displacement.

    size_t iInstruction = 0;

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

                iInstruction += 2;
            }
        }
        break;

        case 0b01:
        {
            // Memory mode, 8-bit displacement follows

            displacement = static_cast<int8_t>(byte[2]);

            iInstruction += 1;
        }
        break;

        case 0b10:
        {
            // Memory mode, 16-bit displacement follows

            displacement = (byte[3] << 8) | byte[2];

            iInstruction += 2;
        }
        break;

        default:
        {
            assert(false);
        }
        break;
    }

    // Print effective address to string

    *effectiveAddress += "[";

    if (baseAndIndex)
    {
        *effectiveAddress += baseAndIndex;

        if (displacement)
        {
            if (displacement > 0)
            {
                *effectiveAddress += " + ";
            }
            else
            {
                *effectiveAddress += " - ";
                displacement = -displacement;
            }
        }
    }

    if (displacement)
    {
        *effectiveAddress += std::to_string(displacement);
    }

    *effectiveAddress += "]";

    return iInstruction;
}

void decode(const std::vector<uint8_t>& input, std::string* output)
{
    *output += "bits 16\n\n";

    for (size_t iInstruction = 0, end = input.size(); iInstruction < end;)
    {
        std::string instructionName;
        std::string destination;
        std::string source;

        const uint8_t* byte = input.data() + iInstruction;

        // Extract instruction fields

        uint8_t opcode = byte[0] >> 2;
        uint8_t d = (byte[0] >> 1) & 1;
        uint8_t w = byte[0] & 1;

        // TODO This breaks if the last instruction is only 1 byte.
        uint8_t mod = byte[1] >> 6;
        uint8_t reg = (byte[1] >> 3) & 7;
        uint8_t rm  = byte[1] & 7;

        // Disassemble instruction

        switch (opcode)
        {
            // MOV register/memory to/from register

            case 0b100010:
            {
                instructionName = "mov";

                if (mod == 0b11)
                {
                    // Register mode (no displacement)

                    const char* regName = c_registerNames[w][reg];
                    const char* rmName  = c_registerNames[w][rm];


                    destination = d ? regName : rmName;
                    source      = d ? rmName  : regName;
                }
                else
                {
                    std::string* registerName     = d ? &destination : &source;
                    std::string* effectiveAddress = d ? &source      : &destination;

                    *registerName = c_registerNames[w][reg];

                    iInstruction += computeEffectiveAddress(byte, mod, rm, effectiveAddress);
                }

                iInstruction += 2;
            }
            break;

            // MOV immediate to register/memory

            case 0b110001:
            {
                assert(d == 1);
                assert(reg == 0);

                instructionName = "mov";

                size_t displacementOffset = 0;

                if (mod == 0b11)
                {
                    destination += c_registerNames[w][rm];
                }
                else
                {
                    displacementOffset += computeEffectiveAddress(byte, mod, rm, &destination);
                }

                source = w ? "word " : "byte ";
                int16_t data = w ?  ((byte[3 + displacementOffset] << 8) | byte[2 + displacementOffset]) :
                                                       static_cast<int8_t>(byte[2 + displacementOffset]);
                source += std::to_string(data);

                iInstruction += 3 + displacementOffset + w;
            }
            break;

            // MOV immediate to register

            case 0b101100:
            case 0b101101:
            case 0b101110:
            case 0b101111:
            {
                instructionName = "mov";

                w = (byte[0] >> 3) & 1;
                reg = byte[0] & 7;
                destination = c_registerNames[w][reg];

                int16_t data = w ? ((byte[2] << 8) | byte[1]) : static_cast<int8_t>(byte[1]);
                source = std::to_string(data);

                iInstruction += 2 + w;
            }
            break;

            // MOV memory to/from accumulator

            case 0b101000:
            {
                instructionName = "mov";

                std::string* effectiveAddress = d ? &destination : &source;
                std::string* accumulator      = d ? &source      : &destination;

                *accumulator = "ax";

                uint16_t addr = (byte[2] << 8) | byte[1];
                *effectiveAddress = "[" + std::to_string(addr) + "]";

                iInstruction += 3;
            }
            break;

            default:
            {
                std::cout << "Error decoding instruction: unknown opcode\n";
                assert(false);
                return;
            }
            break;
        }

        // Print the asm instruction

        *output += instructionName;

        if (!destination.empty())
        {
            *output += " ";
            *output += destination;
        }

        if (!source.empty())
        {
            assert(!destination.empty());

            *output += ", ";
            *output += source;
        }

        *output += "\n";
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
    decode(input, &output);

    // Write output

    std::cout << output << "\n";
}
