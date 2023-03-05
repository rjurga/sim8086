#include <fstream>
#include <iostream>
#include <string>

#include <assert.h>
#include <stdint.h>

const char registerNames[2][8][3] =
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

bool tryReadFile(char* fileName, std::string* result)
{
    std::ifstream file;
    file.open(fileName, std::ifstream::binary | std::ifstream::in);
    if (!file.good())
    {
        std::cout << "Failed to open file " << fileName << ": " << std::strerror(errno) << "\n";
        return false;
    }

    std::string s;
    while (true)
    {
        char c = file.get();
        if (c == EOF)
        {
            if (file.bad())
            {
                std::cout << "Error reading file " << fileName << ": " << std::strerror(errno) << "\n";
                return false;
            }

            break;
        }

        s += c;
    }

    *result = s;
    return true;
}

const char* getRegName(uint8_t reg, uint8_t w)
{
    assert(reg < 8);
    const char* name = registerNames[w][reg];
    return name;
}

void decode(const std::string& input, std::string* output)
{
    *output += "bits 16\n\n";

    for (size_t i = 0, end = input.size(); i < end; i += 2)
    {
        // Extract fields

        uint8_t byte1 = input[i];

        uint8_t opcode = byte1 >> 2;
        uint8_t d = (byte1 >> 1) & 1;
        uint8_t w = byte1 & 1;

        uint8_t byte2 = input[i+1];

        uint8_t mod = byte2 >> 6;
        uint8_t reg = (byte2 >> 3) & 7;
        uint8_t rm  = byte2 & 7;

        // Disassemble

        switch (opcode)
        {
            case 0b100010:
            {
                *output += "mov ";
            }
            break;

            default:
            {
                assert(false);
                return;
            }
            break;
        }

        const char* regName = registerNames[w][reg];
        const char* rmName  = registerNames[w][rm];

        const char* destination = d ? regName : rmName;
        const char* source      = d ? rmName  : regName;

        *output += destination;
        *output += ", ";
        *output += source;
        *output += '\n';
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

    std::string input;
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
