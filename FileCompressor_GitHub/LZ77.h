#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <list>
#include <memory>
#include <array>
#include <bitset>
#include <cstdint>
#include "FileRW.h"
#include <optional>
#include <algorithm>

#pragma once

using namespace std;

//Структура файла: 
/*Offset          Размер                       Описание
      0               1 байт                  Дополнение (padding) - количество битов дополнения в конце
      1                переменный      Последовательность токенов LZ77 (по 25 бит каждый)
*/

//Формат токена LZ77 (25 бит):
/*Биты    Поле                Описание
0-8     offset (9 бит)      Смещение назад в поисковом буфере (1-511, 0 = литерал)
9-14    length (6 бит)      Длина совпадающей последовательности (0-63)
15-22   next_char (8 бит)   Следующий символ после совпадения (литерал)
23      isEOF (1 бит)       Флаг конца файла (1 = последний токен)
24      isValidNextChar     Флаг валидности next_char (0 = токен без литерала)
*/

class LZ77
{
    // Константы для размеров скользящего окна:
    static constexpr int SEARCH_SIZE = 511; // Размер словаря(поискового буфера) : 511 байт
    static constexpr int LOOKAHEAD_SIZE = 63; // Размер буфера предпросмотра: 63 байта

    // Максимальные значения для токенов (определяются размерами буферов):
   // offset: 0-511 (9 бит) - смещение в поисковом буфере
   // length: 0-63 (6 бит) - длина совпадающей последовательности

    // Структура токена LZ77 - основная единица сжатых данных
    struct LZ77Token
    {
        uint16_t offset; // Смещение назад в поисковом буфере (0-511)
        uint8_t length; // Длина совпадающей последовательности (0-63)
        unsigned char next_char; // Следующий символ после совпадения (литерал)
        bool isEOF = false; // Флаг конца файла
        bool isValidNextChar = true; // Флаг валидности next_char (нужен для последнего токена)
    };

    /*
   * Запись токена LZ77 в битовый поток
   * Формат токена (25 бит):
   * [offset]   : 9 бит  (0-511) - смещение в поисковом буфере
   * [length]   : 6 бит  (0-63)  - длина совпадения
   * [next_char]: 8 бит  (0-255) - следующий символ (литерал)
   * [isEOF]    : 1 бит  (0/1)   - флаг конца файла
   * [isValidNextChar]: 1 бит (0/1) - флаг валидности next_char
   * Итого: 9 + 6 + 8 + 1 + 1 = 25 бит
   */

    void WriteToken(LZ77Token token, BitWriter& writer)
    {
        // Записываем offset (9 бит) - старшие биты первыми
        for (int i = 8; i >= 0; i--)
        {
            bool bit = (token.offset >> i) & 1;
            writer.WriteBit((token.offset >> i) & 1);
        }

        // Записываем length (6 бит)
        for (int i = 5; i >= 0; i--)
        {
            bool bit = (token.length >> i) & 1;
            writer.WriteBit((token.length >> i) & 1);
        }

        // Записываем next_char (8 бит = 1 байт)
        writer.WriteByte(token.next_char);

        // Записываем флаги
        if (token.isEOF)
            writer.WriteBit(1);
        else
            writer.WriteBit(0);

        if (token.isValidNextChar)
            writer.WriteBit(1);
        else
            writer.WriteBit(0);
    }

    // Чтение токена LZ77 из битового потока
    LZ77Token ReadToken(BitReader& reader)
    {
        LZ77Token token;

        // Читаем offset (9 бит)
        token.offset = 0;
        for (int i = 8; i >= 0; i--)
        {
            bool bit = reader.ReadBit();
            token.offset |= (bit << i);
        }

        // Читаем length (6 бит)
        token.length = 0;
        for (int i = 5; i >= 0; i--)
        {
            bool bit = reader.ReadBit();
            token.length |= (bit << i);
        }

        // Читаем next_char (1 байт)
        token.next_char = reader.ReadByte();

        // Читаем флаги
        token.isEOF = reader.ReadBit();
        token.isValidNextChar = reader.ReadBit();

        return token;
    }


    /*
   * Сдвиг скользящего окна
   * Алгоритм:
   * 1. Переносит processedAmount байт из lookAheadBuffer в searchBuffer
   * 2. Удаляет эти байты из lookAheadBuffer
   * 3. Дозаполняет lookAheadBuffer из входного потока до LOOKAHEAD_SIZE
   * 4. Обрезает searchBuffer до SEARCH_SIZE (удаляет самые старые байты)
   */
    void SlideWindow(BitReader& reader, vector<unsigned char>& searchBuffer,
        vector<unsigned char>& lookAheadBuffer, size_t shiftAmount)
    {
        // Ограничиваем сдвиг размером буфера предпросмотра
        shiftAmount = (shiftAmount < lookAheadBuffer.size()) ? shiftAmount : lookAheadBuffer.size();
        if (shiftAmount == 0) return;

        // 1. Переносим байты из буфера предпросмотра в поисковый буфер
        searchBuffer.insert(searchBuffer.end(),
            lookAheadBuffer.begin(),
            lookAheadBuffer.begin() + shiftAmount);

        // 2. Удаляем перенесенные байты из буфера предпросмотра
        lookAheadBuffer.erase(lookAheadBuffer.begin(),
            lookAheadBuffer.begin() + shiftAmount);

        // 3. Обрезаем поисковый буфер до максимального размера 
        if (searchBuffer.size() > SEARCH_SIZE)
        {
            size_t toRemove = searchBuffer.size() - SEARCH_SIZE;
            searchBuffer.erase(searchBuffer.begin(),
                searchBuffer.begin() + toRemove);
        }

        // 4. Дозаполняем буфер предпросмотра из входного потока
        while (lookAheadBuffer.size() < LOOKAHEAD_SIZE)
        {
            try
            {
                unsigned char byte = reader.ReadByte();
                lookAheadBuffer.push_back(byte);

            }
            catch (const std::exception& e)
            {
                break; // Достигнут конец файла
            }
        }
    }

public:
    /*
   * Метод декодирования (распаковки) файла
   * Формат сжатых данных для LZ77:
   * [Дополнение]    : 1 байт (uint8_t) - количество битов дополнения в конце
   * [Токены LZ77]   : последовательность токенов по 25 бит каждый
   */
    void DecodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
    {
        BitReader reader{ in };
        BitWriter writer{ out };

        vector<unsigned char> outputBuffer;

        // Выходной буфер для декодированных данных
        // Нужен для доступа к уже декодированным данным при копировании
        unsigned char padding = 0;
        in.read(reinterpret_cast<char*>(&padding), 1);

        try
        {
            while (true)
            {
                // Читаем очередной токен
                LZ77Token token = ReadToken(reader);

                // Обработка токена конца файла
                if (token.isEOF)
                {
                    if (token.offset == 0) // если смещение равно 0, то это токен литерального символа
                    {
                        // Литеральный символ
                        outputBuffer.push_back(token.next_char);
                        writer.WriteByte(token.next_char);
                        processedBytes.fetch_add(1);
                    }
                    else // если нет -> пишем последовательность
                    {
                        // стартовая позиция в буфере, откуда будет начинатся копирование
                        size_t start_pos = outputBuffer.size() - token.offset;

                        // Копируем совпадающую последовательность
                        // для этого поочередно пишем каждый символ (байт)
                        for (int i = 0; i < token.length; i++)
                        {
                            unsigned char decoded_char = outputBuffer[start_pos + i];
                            outputBuffer.push_back(decoded_char);
                            writer.WriteByte(decoded_char);
                            processedBytes.fetch_add(1);
                        }

                        // будет истинно только, если длина совпадения была равна длине буфера предпросмотра
                        if (token.isValidNextChar)
                            writer.WriteByte(token.next_char);
                    }
                    break; // завершаем декодирование
                }

                if (token.offset == 0)
                {
                    // Литеральный символ
                    outputBuffer.push_back(token.next_char);
                    writer.WriteByte(token.next_char);
                    processedBytes.fetch_add(1);
                }
                else
                {
                    // стартовая позиция в буфере, откуда будет начинатся копирование
                    size_t start_pos = outputBuffer.size() - token.offset;

                    // Копируем совпадающую последовательность
                    // для этого поочередно пишем каждый символ (байт)
                    for (int i = 0; i < token.length; i++)
                    {
                        unsigned char decoded_char = outputBuffer[start_pos + i];
                        outputBuffer.push_back(decoded_char);
                        writer.WriteByte(decoded_char);
                        processedBytes.fetch_add(1);
                    }

                    // Всегда добавляем следующий символ
                    outputBuffer.push_back(token.next_char);
                    writer.WriteByte(token.next_char);
                    processedBytes.fetch_add(1);

                }

                // Ограничиваем буфер (для экономии памяти)
                // Важно: стираются только те данные, которые уже вне SEARCH_SIZE
                if (outputBuffer.size() > SEARCH_SIZE)
                {
                    outputBuffer.erase(outputBuffer.begin(),
                        outputBuffer.begin() + (outputBuffer.size() - SEARCH_SIZE));
                }
            }
        }
        catch (const std::exception& e)
        {
            // Конец файла - нормально
            if (std::string(e.what()) != "End of file")
            {
                throw;
            }
        }

        writer.FlushFileBuffer();

        // если есть padding просто читаем его (нужно для декодирования архива)
        for (int i = 0; i < padding; i++)
        {
            reader.ReadBit();
        }
    }

    /*
   * Метод кодирования (сжатия) файла
   * Возвращает размер сжатых данных в байтах
   * Формат выходных данных:
   * [Дополнение]    : 1 байт (uint8_t) - записывается в конце
   * [Токены LZ77]   : последовательность токенов по 25 бит каждый
   */
    uint64_t EncodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
    {
        uint64_t compressedSize = 0;

        BitReader reader{ in };
        BitWriter writer{ out };

        // Инициализация скользящего окна:
        vector<unsigned char> searchBuffer;
        vector<unsigned char> lookAheadBuffer;

        // Запоминаем позицию для записи метаданных
        streamsize beg = out.tellp();

        unsigned char zeroByte = 0;
        out.write(reinterpret_cast<const char*>(&zeroByte), 1);

        uint64_t dataSize = 0;

        // Инициализация буфера предпросмотра
        while (lookAheadBuffer.size() < LOOKAHEAD_SIZE)
        {
            try
            {
                unsigned char byte = reader.ReadByte();
                lookAheadBuffer.push_back(byte); //пишет первый байт в буфер предпросмотра
                processedBytes.fetch_add(1);
            }
            catch (const std::exception& e)
            {
                break;
            }
        }

        //если буфер предпросмотра пуст -> файл закончился
        while (!lookAheadBuffer.empty())
        {
            //если есть совпадения, пишем самое длинное
            int bestOffset = 0;
            int bestLength = 0;

            // Ищем совпадения в searchBuffer
            for (int offset = 1; offset <= searchBuffer.size(); offset++)
            {
                int searchStart = searchBuffer.size() - offset;
                int currentLength = 0;

                // offset не может быть больше SEARCH_SIZE
                if (offset > SEARCH_SIZE) continue;

                // Сравниваем символы пока они совпадают
                while (currentLength < lookAheadBuffer.size() &&
                    searchStart + currentLength < searchBuffer.size() &&
                    searchBuffer[searchStart + currentLength] == lookAheadBuffer[currentLength])
                {
                    currentLength++;
                }

                // Обновляем лучшее совпадение
                if (currentLength > bestLength)
                {
                    bestLength = currentLength;
                    bestOffset = offset;
                }
            }

            LZ77Token token;

            // Если нашли хорошее совпадение (минимум 3 символа)
            if (bestLength >= 3)
            {
                token.offset = bestOffset;
                token.length = bestLength;

                //если длинна совпадения меньше размера буфера предпросмотра
                if (bestLength < lookAheadBuffer.size())
                {
                    // в этом случае next_char будет находится в буфере предпросмотра
                    token.next_char = lookAheadBuffer[bestLength];
                    SlideWindow(reader, searchBuffer, lookAheadBuffer, bestLength + 1);

                    // если буфер предпросмотра пуст, то обрабатываем конец файла
                    if (lookAheadBuffer.empty())
                    {
                        // помечаем токен как EOF и пишем в файл
                        token.isEOF = true;
                        WriteToken(token, writer);

                        compressedSize += 25;
                        dataSize++;
                        break;
                    }
                }
                else // если длина совпадения равна длине буфера предпросмотра
                {
                    // так как не пишем next_char, то сдвигаем только на bestLength
                    SlideWindow(reader, searchBuffer, lookAheadBuffer, bestLength);
                    if (lookAheadBuffer.empty())
                    {
                        //если это конец файла, то декодер не будет читать next_char, но для этого нужно указать isValidNextChar = false;
                        token.next_char = 0;
                        token.isEOF = true;
                        token.isValidNextChar = false;
                        WriteToken(token, writer);
                        compressedSize += 25;
                        dataSize++;
                        break;
                    }
                    //если не конец файла, то стандартная обработка - читаем next_char и сдивгаем буфер на 1 (так как прочитали next_char)
                    token.next_char = lookAheadBuffer[0];
                    SlideWindow(reader, searchBuffer, lookAheadBuffer, 1);
                }
                processedBytes.fetch_add(bestLength + 1);
            }
            else // если не нашли совпадений
            {
                // Литеральный символ
                token.offset = 0;
                token.length = 0;
                token.next_char = lookAheadBuffer[0];
                SlideWindow(reader, searchBuffer, lookAheadBuffer, 1);

                // если после cдвига дошли до конца файла
                if (lookAheadBuffer.empty())
                {
                    dataSize++;
                    token.isEOF = true;
                    WriteToken(token, writer);
                    compressedSize += 25;

                    break;
                }

                processedBytes.fetch_add(1);
            }

            // пишем сформированый токен
            WriteToken(token, writer);
            compressedSize += 24;
            dataSize++;
        }

        writer.FlushFileBuffer();

        unsigned char padding = writer.GetPaddingBits();

        streampos encodedDataEnd = out.tellp();
        out.seekp(beg, std::ios::beg);

    
        out.write(reinterpret_cast<const char*>(&padding), 1);

        out.seekp(encodedDataEnd);


        return compressedSize / 8;
    }
};


