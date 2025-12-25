#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include <exception>
#include "Utilities.h"
#include <iostream>
#include <fstream>
#include "StaticHuffman.h"
#include "AdHuff.h"
#include "LZ77.h"
#include "LZ78.h"

#pragma once

using namespace std;

// Перечисление алгоритмов сжатия
// Используется uint8_t для экономии памяти при сохранении в файл
enum class CompressAlg : uint8_t { StaticHuffman = 0, AdaptiveHuffman = 1, LZ77 = 2, LZ78 = 3 };

// Структура для хранения статистики по сжатию
struct Statistics
{
    vector<double> timeElapsed;// Время сжатия для каждого файла (в секундах)
    vector<pair<uint64_t, uint64_t>> sizes; // Пары (исходный размер, сжатый размер) для каждого файла
};

/*[Сигнатура]     : 4 байта ('a','r','c','h')
[Кол-во файлов] : 4 байта (uint32_t)
[Алгоритм]      : 1 байт (uint8_t)
[Метаданные]    : для каждого файла:
                  - Длина имени: 4 байта
                  - Имя файла: переменная длина
                  - Исходный размер: 8 байт
[Данные]        : сжатые данные каждого файла
*/

// Класс для управления созданием и распаковкой архивов
class ArchiveManager
{
    //поля для управления многопоточностью и хранения состояния
    thread workerThread;// Поток для асинхронной работы
    atomic<uint64_t> processedBytes{ 0 }; // Количество обработанных байт (атомарное для потокобезопасности)
    atomic<uint64_t> totalBytes{ 0 }; // Общий размер обрабатываемых данных
    atomic<int> currentProgress{ 0 }; // Текущий прогресс 0-100% (атомарное)
    Statistics stats;// Статистика сжатия
    double decompressingTime; // Время распаковки
    exception_ptr workerException;// Указатель на исключение из рабочего потока

public:
    // Метод сброса состояния менеджера
    void Reset()
    {
        processedBytes = 0;
        totalBytes = 0;
        currentProgress = 0;

        stats.sizes.clear();// Очищаем статистику размеров
        stats.timeElapsed.clear();// Очищаем статистику времени
    }

    // Геттер для времени распаковки
    double GetDecompressingTime() const { return decompressingTime; }

    // Основной метод создания архива (работает в рабочем потоке)
    void CreateArchive(vector<string> fileNames, string archivepath, CompressAlg alg)
    {
        // Вектор для хранения размеров файлов (чтобы не переоткрывать файлы позже)
        vector<uint64_t> fileSizes;

        // Первый проход: вычисляем общий размер всех файлов
        for (int i = 0; i < fileNames.size(); i++)
        {
            // Открываем файл в бинарном режиме
            ifstream file{ fileNames[i], std::ios::binary };

            // Получаем размер файла
            uint64_t fileSize = static_cast<uint64_t>(GetFileSize(file));

            // Сохраняем размер файла и добавляем к общему размеру
            fileSizes.push_back(fileSize);
            totalBytes.fetch_add(fileSize); // Атомарное добавление

            // Закрываем файл после чтения размера
            file.close();
        }

        // Создаем файл архива в бинарном режиме
        ofstream archive;
        archive.open(archivepath, std::ios::binary);

        // Записываем сигнатуру архива (4 байта: 'a','r','c','h')
        char sig[4] = { 'a', 'r', 'c', 'h' };
        archive.write(sig, 4);

        // Записываем количество файлов в архиве (4 байта)
        uint32_t fileCount = fileNames.size();
        archive.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

        // Записываем идентификатор алгоритма сжатия (1 байт)
        uint8_t algValue = static_cast<uint8_t>(alg);
        archive.write(reinterpret_cast<const char*>(&algValue), sizeof(algValue));

        // Записываем метаданные для каждого файла:
        // - длина имени файла (4 байта)
        // - имя файла (переменная длина)
        // - исходный размер файла (8 байт)
        for (int i = 0; i < fileNames.size(); i++)
        {
            // Получаем только имя файла без пути
            string name = GetNameFromPath(fileNames[i]);
            uint32_t length = name.length();

            // Записываем длину имени файла
            archive.write(reinterpret_cast<const char*>(&length), sizeof(uint32_t));

            // Записываем само имя файла
            archive.write(name.c_str(), length);

            //  Используем ПРЕДВАРИТЕЛЬНО вычисленный размер
            archive.write(reinterpret_cast<const char*>(&fileSizes[i]), sizeof(fileSizes[i]));
        }

        // Второй проход: сжатие каждого файла выбранным алгоритмом
        switch (alg) //ПРИНЦИП РАБОТЫ КАЖДОГО КЕЙСА ОДИНАКОВ
        {
        case CompressAlg::StaticHuffman:
            for (int i = 0; i < fileNames.size(); i++)
            {
                // Добавляем запись для статистики (исходный размер известен, сжатый = 0 пока)
                stats.sizes.push_back(make_pair(fileSizes[i], 0));

                // Открываем файл для сжатия
                ifstream file{ fileNames[i], std::ios::binary };

                // Засекаем время начала сжатия
                auto start = chrono::steady_clock::now();

                // Создаем менеджер статического Хаффмана и сжимаем файл
                StaticHuffmanManager sh;
                stats.sizes[i].second = sh.EncodeFile(file, archive, processedBytes);

                // Засекаем время окончания и вычисляем длительность
                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                stats.timeElapsed.push_back(time.count());

                file.close();// Закрываем файл
            }
            break;
        case CompressAlg::LZ77:
            for (int i = 0; i < fileNames.size(); i++)
            {       

                stats.sizes.push_back(make_pair(fileSizes[i], 0));

                ifstream file{ fileNames[i], std::ios::binary };

                auto start = chrono::steady_clock::now();

                LZ77 lz77;
                stats.sizes[i].second = lz77.EncodeFile(file, archive, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                stats.timeElapsed.push_back(time.count());

                file.close();

            }
            break;
        case CompressAlg::LZ78:
            for (int i = 0; i < fileNames.size(); i++)
            {
                stats.sizes.push_back(make_pair(fileSizes[i], 0));

                ifstream file{ fileNames[i], std::ios::binary };

                auto start = chrono::steady_clock::now();

                LZ78 lz78;
                stats.sizes[i].second = lz78.EncodeFile(file, archive, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                stats.timeElapsed.push_back(time.count());

                file.close();
            }
            break;
        case CompressAlg::AdaptiveHuffman:
            for (int i = 0; i < fileNames.size(); i++)
            {
                stats.sizes.push_back(make_pair(fileSizes[i], 0));

                ifstream file{ fileNames[i], std::ios::binary };

                auto start = chrono::steady_clock::now();

                AdaptiveHuffmanCoder coder;
                stats.sizes[i].second = coder.EncodeFile(file, archive, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                stats.timeElapsed.push_back(time.count());

                file.close();
            }
            break;
        }

        archive.close();
    }

    // Метод распаковки архива
    void UnboxArchive(string archivePath, string unboxTo)
    {
        // Открываем архив для чтения в бинарном режиме
        ifstream archive{ archivePath, std::ios::binary };

        // Читаем сигнатуру архива (4 байта)
        char signature[4];
        archive.read(signature, 4);

        // Проверяем сигнатуру (должно быть "arch")
        if (memcmp(signature, "arch", 4) != 0)
        {
            throw runtime_error("Invalid archive signature");
        }

        // Читаем количество файлов в архиве (4 байта)
        uint32_t fileCount;
        archive.read(reinterpret_cast<char*>(&fileCount), 4);  

        // Читаем идентификатор алгоритма сжатия (1 байт)
        uint8_t algValue;
        archive.read(reinterpret_cast<char*>(&algValue), 1);  

        // Преобразуем байт в перечисление алгоритмов
        CompressAlg alg = static_cast<CompressAlg>(algValue);

        // Вектор для хранения имен файлов из архива
        vector<string> fileNames;

        // Читаем метаданные для каждого файла
        for (uint32_t i = 0; i < fileCount; i++)
        {
            // Читаем длину имени файла (4 байта)
            uint32_t length = 0;
            archive.read(reinterpret_cast<char*>(&length), sizeof(uint32_t));

            // Читаем имя файла (переменная длина)
            string fileName(length, '\0');
            archive.read(&fileName[0], length);

            fileNames.push_back(fileName);

            // Читаем исходный размер файла (8 байт) и добавляем к общему размеру
            uint64_t size = 0;
            archive.read(reinterpret_cast<char*>(&size), 8);
            totalBytes.fetch_add(size);
        }

        // Распаковываем файлы в зависимости от алгоритма сжатия
        switch (alg)  //ПРИНЦИП РАБОТЫ КАЖДОГО КЕЙСА ОДИНАКОВ
        {
        case CompressAlg::StaticHuffman:

            for (uint32_t i = 0; i < fileCount; i++)
            {
                // Создаем файл для распаковки (путь + имя файла)
                ofstream file{ unboxTo + fileNames[i], std::ios::binary };
                StaticHuffmanManager st;

                auto start = chrono::steady_clock::now();

                // Распаковываем файл
                st.DecodeFile(archive, file, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                decompressingTime = time.count();

                file.close();
            }
            break;
        case CompressAlg::LZ77:
            for (uint32_t i = 0; i < fileCount; i++)
            {
                ofstream file{ unboxTo + fileNames[i], std::ios::binary };

                LZ77 lz77;

                auto start = chrono::steady_clock::now();

                lz77.DecodeFile(archive, file, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                decompressingTime = time.count();

                file.close();
            }
            break;
        case CompressAlg::LZ78:
            for (uint32_t i = 0; i < fileCount; i++)
            {
                ofstream file{ unboxTo + fileNames[i], std::ios::binary };

                LZ78 lz78;

                auto start = chrono::steady_clock::now();

                lz78.DecodeFile(archive, file, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                decompressingTime = time.count();

                file.close();
            }
            break;
        case CompressAlg::AdaptiveHuffman:
            for (uint32_t i = 0; i < fileCount; i++)
            {
                ofstream file{ unboxTo + fileNames[i], std::ios::binary };

                AdaptiveHuffmanCoder coder;

                auto start = chrono::steady_clock::now();

                coder.DecodeFile(archive, file, processedBytes);

                auto end = chrono::steady_clock::now();
                chrono::duration<double> time = end - start;
                decompressingTime = time.count();

                file.close();
            }
            break;
        }
        archive.close();
    }
public:

    // Геттер для статистики
    Statistics GetStatistics() { return stats; }

    // Флаг, указывающий, что менеджер занят работой
    atomic<bool> isWorking{ false };

    // Запуск асинхронного создания архива
    void StartArchiveCreatingAsync(vector<string> fileNames, string archivePath, CompressAlg alg)
    {
        if (isWorking)// Если уже работает - выходим
            return;

        isWorking = true;// Устанавливаем флаг работы

        // Запускаем рабочий поток с лямбда-функцией
        workerThread = thread([this, fileNames, archivePath, alg]()
            {
                try
                {
                    // Выполняем создание архива
                    CreateArchive(fileNames, archivePath, alg);
                }
                catch (...)
                {
                    // Сохраняем исключение, если оно возникло
                    workerException = current_exception();
                }
                isWorking = false; // Сбрасываем флаг работы
            });
    }

    // Запуск асинхронной распаковки архива
    void StartArchiveUnboxingAsync(string archivePath, string unboxTo)
    {
        if (isWorking)// Если уже работает - выходим
            return;

        isWorking = true;
        workerThread = thread([this, archivePath, unboxTo]()
            {
                try
                {
                    // Выполняем распаковку архива
                    UnboxArchive(archivePath, unboxTo);
                }
                catch (...)
                {
                    // Сохраняем исключение
                    workerException = current_exception();
                }

                isWorking = false;
            });
    }

    // Метод обновления состояния менеджера (вызывается из основного потока)
    void Update()
    {
        // Проверяем, завершился ли рабочий поток
        if (workerThread.joinable() &&
            WaitForSingleObject(workerThread.native_handle(), 0) == WAIT_OBJECT_0)
        {
            workerThread.join(); // Ожидаем завершения потока

            // Если было исключение в рабочем потоке
            if (workerException)
            {
                try
                {
                    // Пробрасываем исключение в основной поток
                    std::rethrow_exception(workerException);
                }
                catch (const std::exception& e)
                {
                    // Показываем сообщение об ошибке
                    MessageBoxA(nullptr, e.what(), "Ошибка", MB_ICONERROR);

                    workerException = nullptr;
                    isWorking = false;
                }
            }
            else
            {
                isWorking = false; // Сбрасываем флаг работы
            }
        }
    }

    // Получение текущего прогресса в процентах
    int GetNewProgress()
    {
        if (totalBytes.load() != 0)
            // Вычисляем процент: (обработанные байты * 100) / общие байты
            return static_cast<int>((processedBytes.load() * 100) / totalBytes.load());
        else
            return 0;// Если общий размер равен 0 - прогресс 0%
    }

};
