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
#include <atomic>

#pragma once

using namespace std;

/*Offset   Размер                Описание
      0        2 байта                  Размер таблицы (tableSize) - количество уникальных символов (1-256)
      2        8 байт                    Размер данных (dataSize) - количество исходных байтов
     10       1 байт                     Дополнение (padding) - количество битов дополнения в конце
     11       переменный          Таблица кодов
     ...       переменный            Закодированные данные (битовый поток)
*/

//Каждая запись таблицы (повторяется tableSize раз):
/*[Символ]      : 1 байт  (unsigned char) - исходный символ
[Длина кода]  : 1 байт  (unsigned char) - длина кода Хаффмана в битах (1-255)
[Код]         : N бит   - сам код Хаффмана (N = длина кода)
*/

class StaticHuffmanManager
{

    // Структура узла дерева Хаффмана
    struct Node
    {
        unsigned char byte; // Символ (только для листьев)
        int frequency; // Частота символа/сумма частот поддерева
        shared_ptr<Node> left;  // Левый потомок (бит 0)
        shared_ptr<Node> right; // Правый потомок (бит 1)

        // Конструктор для листового узла (символ + частота)
        Node(unsigned char byte, int freq) : byte{ byte }, frequency{ freq }, left(nullptr), right{ nullptr } {}

        // Конструктор для внутреннего узла (объединяет два поддерева)
        Node(shared_ptr<Node> l, shared_ptr<Node> r)
            : byte(0), frequency(l->frequency + r->frequency), left(l), right(r)
        {
        }

        // Проверка, является ли узел листом (нет потомков)
        bool isLeaf() const
        {
            return !left && !right;
        }
    };

    // Функтор для сравнения узлов при построении дерева Хаффмана
    // Узлы сортируются по возрастанию частоты (меньшая частота -> выше приоритет)
    struct MyCompare
    {
        bool operator()(const shared_ptr<Node> a, const shared_ptr<Node> b) const
        {
            return a->frequency < b->frequency;
        }
    };

    // Временный вектор для построения кодов (путь от корня до текущего узла)
    vector<bool> code;
    // Таблица кодов Хаффмана: символ -> его двоичный код (вектор битов)
    map<unsigned char, vector<bool>> table;

    // Рекурсивное построение таблицы кодов Хаффмана (обход дерева в глубину)
    void makeTable(shared_ptr<Node> root)
    {
        // Если дерево состоит из одного узла (один символ в файле)
        if (root->isLeaf())
        {
            code.push_back(0);  // добавляем фиктивный бит 0
            table[root->byte] = code; // сохраняем код для символа
            code.pop_back(); // удаляем фиктивный бит
            return;
        }

        // Рекурсивный обход левого поддерева (добавляем бит 0)
        if (root->left != nullptr)
        {
            code.push_back(0);
            makeTable(root->left);
            code.pop_back(); // Возвращаемся назад
        }

        // Рекурсивный обход правого поддерева (добавляем бит 1)
        if (root->right != nullptr)
        {
            code.push_back(1);
            makeTable(root->right);
            code.pop_back(); // Возвращаемся назад
        }

        // Сохраняем код для листового узла
        if (root->isLeaf())
        {
            table[root->byte] = code;
        }
    }

    // Вспомогательная функция для записи uint64_t в big-endian формате
    void WriteUInt64(ofstream& out, uint64_t value)
    {
        // Преобразуем в big-endian (network byte order)
        // Старшие байты записываются первыми
        unsigned char bytes[8];
        for (int i = 0; i < 8; i++)
        {
            bytes[7 - i] = (value >> (i * 8)) & 0xFF; // Извлекаем i-й байт
        }
        out.write(reinterpret_cast<const char*>(bytes), 8);
    }
public:
    /*
    * Метод кодирования файла (сжатие)
    * Формат выходных данных:
    * [Размер таблицы]  : 2 байта (uint16_t) - количество записей в таблице (1-256)
    * [Размер данных]   : 8 байт (uint64_t) - количество исходных байтов
    * [Дополнение]      : 1 байт (uint8_t) - количество битов дополнения в конце
    * [Таблица кодов]   : переменный размер
    * [Закодированные данные] : битовый поток
    */
    uint64_t EncodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
    {
        // Очищаем предыдущее состояние
        code.clear();
        table.clear();

        uint64_t compressedSize = 0; // Размер сжатых данных в битах

        // пишем таблицу кодов
        array<int, 256> byteFreq = { 0 }; // Массив частот для всех 256 возможных байтов
        
        //читаем весь файл и увеличиваем частоту для каждого очередной раз встретившегося байта 
        unsigned char byte;
        while (in.read(reinterpret_cast<char*>(&byte), 1))
        {
            byteFreq[byte]++; // Инкрементируем счетчик для этого байта
        }

        //Построение начального списка узлов (листьев)
        list<shared_ptr<Node>> nodes; // Список узлов для построения дерева
        MyCompare huffmanComparator; // Компаратор для сортировки

        // Создаем листовые узлы только для символов с ненулевой частотой
        for (int i = 0; i < 256; i++)
        {
            if (byteFreq[i] > 0)
            {
                nodes.push_back(make_shared<Node>(static_cast<unsigned char>(i), byteFreq[i]));
            }
        }

        //Построение дерева Хаффмана (алгоритм слияния узлов)
        while (nodes.size() != 1) // Пока не останется один корневой узел
        {
            // Сортируем узлы по возрастанию частоты
            nodes.sort(huffmanComparator);

            // Берем два узла с наименьшей частотой
            shared_ptr<Node> sonL = nodes.front();
            nodes.pop_front();
            shared_ptr<Node> sonR = nodes.front();
            nodes.pop_front();

            // Создаем родительский узел
            shared_ptr<Node> parent = make_shared<Node>(sonL, sonR);
            nodes.push_back(parent);
        }

        //Построение таблицы кодов Хаффмана (обход дерева)
        makeTable(nodes.back()); // nodes.back() - корневой узел

        // Переоткрываем файл для кодирования
        in.clear();
        in.seekg(0, std::ios::beg); // Возвращаемся в начало файла


        unsigned char zeroByte = 0; // Для временных заглушек

        uint16_t tableSize = table.size(); // Количество уникальных символов

        // Запоминаем позицию для записи метаданных
        streamsize beg = out.tellp();

        //размер таблицы
        out.write(reinterpret_cast<char*>(&tableSize), 2);

        // пишем заглушки для метаданных
        //размер данных
        out.write(reinterpret_cast<char*>(&zeroByte), 8);

        //padding данных
        out.write(reinterpret_cast<char*>(&zeroByte), 1);


        BitWriter writer{ out };

         //Запись таблицы кодов в файл
         // Формат каждой записи таблицы:
         // [Символ] (1 байт) + [Длина кода] (1 байт) + [Код] (переменная длина)
        for (const auto& pair : table)
        {
            unsigned char encodedByte = pair.first;  // Символ
            vector<bool> code = pair.second; // Код Хаффмана

            unsigned char c = code.size(); // Длина кода (1-255)
            writer.WriteByte(encodedByte); //пишем сам байт
            writer.WriteByte(c); //пишем длину присвоенного ему кода

            for (bool bit : code)
            {
                writer.WriteBit(bit); //пишем сам код
            }
        }

        //Создание lookup-таблицы для быстрого доступа к кодам
        vector<vector<bool>> lookupTable(256); // Индекс = символ, значение = код
        for (const auto& pair : table)
        {
            lookupTable[static_cast<unsigned char>(pair.first)] = pair.second;
        }

        //Кодирование данных (второй проход по файлу)
        uint64_t dataSize = 0; // Счетчик исходных байтов

        BitReader reader{ in };

        while (true)
        {
            try
            {
                byte = reader.ReadByte(); // Читаем очередной байт
            }
            catch (const exception& e)
            {
                break; // Достигнут конец файла
            }

            // Получаем код Хаффмана для этого байта
            vector<bool> huffmanCode = lookupTable[byte];

            // Записываем код в выходной поток
            for (int i = 0; i < huffmanCode.size(); i++)
            {
                writer.WriteBit(huffmanCode[i]);
                compressedSize++; // Увеличиваем счетчик битов
            }
            dataSize++; // Увеличиваем счетчик исходных байтов
            processedBytes.fetch_add(1);  // Обновляем прогресс
        }

        // Записываем оставшиеся биты из буфера
        writer.FlushFileBuffer();

        // возвращаемся назад и пишем все метаданные 
        streampos encodedDataEnd = out.tellp();
        out.seekp(beg + 2, std::ios::beg);

        out.write(reinterpret_cast<char*>(&dataSize), 8);
        unsigned char padding = writer.GetPaddingBits();
        out.write(reinterpret_cast<char*>(&padding), 1);

        out.seekp(encodedDataEnd);

        return (compressedSize + static_cast<uint64_t>(padding))/8;
    }

    /*
    * Метод декодирования файла (распаковка)
    * Формат входных данных:
    * [Размер таблицы]  : 2 байта (uint16_t)
    * [Размер данных]   : 8 байт (uint64_t)
    * [Дополнение]      : 1 байт (uint8_t)
    * [Таблица кодов]   : переменный размер
    * [Закодированные данные] : битовый поток
    */
    void DecodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
    {
        BitReader reader{ in };

        // Шаг 1: Чтение метаданных
        uint16_t tableSize = 0;
        in.read(reinterpret_cast<char*>(&tableSize), 2);  // Размер таблицы

        uint64_t dataSize = 0;
        in.read(reinterpret_cast<char*>(&dataSize), 8); // Размер исходных данных

        unsigned char padding = 0;
        in.read(reinterpret_cast<char*>(&padding), 1); // Биты дополнения

        // Шаг 2: Построение дерева Хаффмана из таблицы кодов
        shared_ptr<Node> root = make_shared<Node>(0, 0); // Создаем корень

        // читаем таблицу и строим дерево
        for (int i = 0; i < tableSize; i++)
        {
            // Читаем символ и длину его кода
            unsigned char encodedByte = reader.ReadByte();  // Символ
            unsigned char codeLength = reader.ReadByte();  // Длина кода

            // Начинаем с корня
            shared_ptr<Node> current = root;

            // Строим путь в дереве согласно коду
            // Бит = 1 ->идем вправо
            // Бит = 0 ->идем влево
            for (int j = 0; j < codeLength; j++)
            {
                if (reader.ReadBit())
                {
                    if (!current->right)
                    {
                        current->right = make_shared<Node>(0, 0);
                    }
                    current = current->right;
                }
                else
                {
                    if (!current->left)
                    {
                        current->left = make_shared<Node>(0, 0);
                    }
                    current = current->left;
                }
            }
            // В конце пути записываем символ в листовой узел
            current->byte = encodedByte;
        }

        // Шаг 3: Декодирование данных
        BitWriter writer{ out };

        // Декодируем dataSize байтов
        for (uint64_t i = 0; i < dataSize; i++)
        {
            shared_ptr<Node> current = root; // Начинаем с корня

            // Читаем биты и движемся по дереву до листа
            while (!current->isLeaf()) 
            {
                bool bit = reader.ReadBit(); // Читаем очередной бит
                if (bit)
                {
                    current = current->right; // 1 -> вправо
                }
                else
                {
                    current = current->left; // 0 → влево
                }
            }

            // Достигли листа → записываем символ
            writer.WriteByte(current->byte);
            processedBytes++; // Обновляем прогресс
        }

        // Шаг 4: Пропускаем биты дополнения (выравнивание до байта)
        for (int i = 0; i < padding; i++)
            reader.ReadBit();

        // Записываем оставшиеся биты из буфера
        writer.FlushFileBuffer();
    }
};
