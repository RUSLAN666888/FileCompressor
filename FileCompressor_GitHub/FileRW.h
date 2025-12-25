#include <fstream>
#include <vector>
#include <array>
#include <bitset>

using namespace std;

#pragma once
class BitWriter
{
private:
    ofstream& out; //âûõîäíîé ïîòîê, êóäà áóäåò ïðîèçâîäèòñÿ çàïèñü
    vector<unsigned char> fileBuffer; // Áóôåð äëÿ ôàéëà
    unsigned char bitBuffer;               // Áóôåð äëÿ áèòîâ
    int bitCount;
    static constexpr size_t BUFFER_SIZE = 4096;
    unsigned char paddingBits;
public:
    BitWriter(std::ofstream& stream) : out(stream), fileBuffer(), bitBuffer(0), bitCount(0), paddingBits(0)
    {
        fileBuffer.reserve(BUFFER_SIZE);
    }

    //ïîëó÷åíèå è ñáðîñ äîïîëíèòåëüíûõ áèòîâ, äîáàâëåííûõ ïîñëå âûçîâà FlushFileBuffer()
    unsigned char GetPaddingBits() { return paddingBits; }
    void ResetPaddingBits() { paddingBits = 0; }

    //ïèøåò îäèí áèò ñíà÷àë â áóôåð
    void WriteBit(bool bit)
    {
        bitBuffer = (bitBuffer << 1) | (bit ? 1 : 0);
        bitCount++;

        if (bitCount == 8)
        {
            // Äîáàâëÿåì áàéò â ôàéëîâûé áóôåð
            fileBuffer.push_back(bitBuffer);
            bitBuffer = 0;
            bitCount = 0;

            // Åñëè áóôåð çàïîëíåí, çàïèñûâàåì â ôàéë
            if (fileBuffer.size() >= BUFFER_SIZE)
            {
                FlushFileBuffer();
            }
        }
    }

    //ïèøåò îäèí áàéò â áóôåð
    void WriteByte(unsigned char b)
    {
        for (int i = 7; i >= 0; i--)
        {
            WriteBit((b >> i) & 1);
        }
    }

    //ïèøåò äàííûå áóôåðà â ôàéë
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
    ifstream& in;// âõîäíîé ïîòîê, îòêóäà ïðîèçâîäèòñÿ ÷òåíèå äàííûõ
    unsigned char currentByte;//òåêóùèé ÷èòàåìûé áàéò
    int bitPos;//êîíêåðåòíûé áèò â òåêóùåì áàéòå
public:
    BitReader(std::ifstream& in) : in{ in }, currentByte{ 0 }, bitPos{ 8 }{}
   
    //÷èòàåò îäèí áèò
    bool ReadBit()
    {
        if (bitPos >= 8)
        {
            // ×èòàåì íîâûé áàéò
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

    //÷èòàåò ïåðåìåííîå ÷èñëî áèò
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

    //÷èòàåò îäèí áàéò
    unsigned char ReadByte()
    {
        return static_cast<unsigned char>(ReadBits(8));
    }

};


