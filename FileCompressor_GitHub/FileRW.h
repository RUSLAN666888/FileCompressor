#include <fstream>
#include <vector>
#include <array>
#include <bitset>

#pragma once
class BitWriter
{
private:
    ofstream& out; //выходной поток, куда будет производится запись
    vector<unsigned char> fileBuffer; // Буфер для файла
    unsigned char bitBuffer;               // Буфер для битов
    int bitCount;
    static constexpr size_t BUFFER_SIZE = 4096;
    unsigned char paddingBits;
public:
    BitWriter(std::ofstream& stream) : out(stream), fileBuffer(), bitBuffer(0), bitCount(0), paddingBits(0)
    {
        fileBuffer.reserve(BUFFER_SIZE);
    }

    //получение и сброс дополнительных битов, добавленных после вызова FlushFileBuffer()
    unsigned char GetPaddingBits() { return paddingBits; }
    void ResetPaddingBits() { paddingBits = 0; }

    //пишет один бит сначал в буфер
    void WriteBit(bool bit)
    {
        bitBuffer = (bitBuffer << 1) | (bit ? 1 : 0);
        bitCount++;

        if (bitCount == 8)
        {
            // Добавляем байт в файловый буфер
            fileBuffer.push_back(bitBuffer);
            bitBuffer = 0;
            bitCount = 0;

            // Если буфер заполнен, записываем в файл
            if (fileBuffer.size() >= BUFFER_SIZE)
            {
                FlushFileBuffer();
            }
        }
    }

    //пишет один байт в буфер
    void WriteByte(unsigned char b)
    {
        for (int i = 7; i >= 0; i--)
        {
            WriteBit((b >> i) & 1);
        }
    }

    //пишет данные буфера в файл
    void FlushFileBuffer()
    {
        if (!fileBuffer.empty())
        {
            while (bitCount != 0)
            {
                WriteBit(0);
                paddingBits++;
            }

            out.write(reinterpret_cast<const char*>(fileBuffer.data()), fileBuffer.size());
            fileBuffer.clear();
        }
    }
};

class BitReader
{
private:
    ifstream& in;// входной поток, откуда производится чтение данных
    unsigned char currentByte;//текущий читаемый байт
    int bitPos;//конкеретный бит в текущем байте
public:
    BitReader(std::ifstream& in) : in{ in }, currentByte{ 0 }, bitPos{ 8 }{}
   
    //читает один бит
    bool ReadBit()
    {
        if (bitPos >= 8)
        {
            // Читаем новый байт
            if (!in.read(reinterpret_cast<char*>(&currentByte), 1))
            {
                throw std::runtime_error("End of file");
            }
            bitPos = 0;
        }

        bool bit = (currentByte >> (7 - bitPos)) & 1;
        bitPos++;
        return bit;
    }

    //читает переменное число бит
    uint64_t ReadBits(int numBits)
    {
        if (numBits > 64)
            throw std::invalid_argument("Too many bits");

        uint64_t result = 0;
        for (int i = 0; i < numBits; i++)
        {
            result = (result << 1) | (ReadBit() ? 1 : 0);
        }

        return result;
    }

    //читает один байт
    unsigned char ReadByte()
    {
        return static_cast<unsigned char>(ReadBits(8));
    }

};

