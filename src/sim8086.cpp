#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

char inputFileName[]  = "external/computer_enhance/perfaware/part1/listing_0038_many_register_mov";
char outputFileName[] = "out/0038.asm";

constexpr size_t c_bufferSize = 1024;
char g_input[c_bufferSize];
char g_output[c_bufferSize];

const char registerNamesByte[8][2] =
{
    { 'a', 'l' },
    { 'c', 'l' },
    { 'd', 'l' },
    { 'b', 'l' },
    { 'a', 'h' },
    { 'c', 'h' },
    { 'd', 'h' },
    { 'b', 'h' },
};

const char registerNamesWord[8][2] =
{
    { 'a', 'x' },
    { 'c', 'x' },
    { 'd', 'x' },
    { 'b', 'x' },
    { 's', 'p' },
    { 'b', 'p' },
    { 's', 'i' },
    { 'd', 'i' },
};

const char* getRegName(uint8_t reg, uint8_t w)
{
    assert(reg < 8);

    const char* name = w ? registerNamesWord[reg] : registerNamesByte[reg];
    return name;
}

size_t decode(long inputSize)
{
    char* writeCursor = g_output;

    strcpy(g_output, "bits 16\n\n");
    writeCursor += 9;

    for (int i = 0; i < inputSize; i += 2)
    {
        assert(writeCursor - g_output < c_bufferSize - 12);

        // Extract fields

        uint8_t byte1 = g_input[i];

        uint8_t opcode = byte1 >> 2;
        uint8_t d = (byte1 >> 1) & 1;
        uint8_t w = byte1 & 1;

        uint8_t byte2 = g_input[i+1];

        uint8_t mod = byte2 >> 6;
        uint8_t reg = (byte2 >> 3) & 7;
        uint8_t rm  = byte2 & 7;

        // Disassemble

        switch (opcode)
        {
        case 0b100010:
        {
            strcpy(writeCursor, "mov ");
            writeCursor += 4;
        } break;

        default:
        {
            assert(false);
        } break;
        }

        const char* regName = getRegName(reg, w);
        const char* rmName  = getRegName(rm, w);

        const char* destination = d ? regName : rmName;
        const char* source      = d ? rmName  : regName;

        *writeCursor++ = destination[0];
        *writeCursor++ = destination[1];
        *writeCursor++ = ',';
        *writeCursor++ = ' ';
        *writeCursor++ = source[0];
        *writeCursor++ = source[1];
        *writeCursor++ = '\n';
    }

    size_t outputSize = writeCursor - g_output;
    return outputSize;
}

int main()
{
    // Read input

    FILE* pFile = fopen(inputFileName, "rb");
    assert(pFile);

    int failed = fseek(pFile, 0, SEEK_END);
    assert(!failed);

    long fileSize = ftell(pFile);
    assert(fileSize != -1);
    assert(fileSize <= c_bufferSize);

    failed = fseek(pFile, 0, SEEK_SET);
    assert(!failed);

    size_t bytesRead = fread(g_input, 1, fileSize, pFile);
    assert(bytesRead == fileSize);

    fclose(pFile);

    // Do the work

    size_t outputSize = decode(fileSize);

    // Write output

    pFile = fopen(outputFileName, "w");
    assert(pFile);

    size_t bytesWritten = fwrite(g_output, 1, outputSize, pFile);
    assert(bytesWritten == outputSize);

    fclose(pFile);
}
