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
#include <unordered_map>

#pragma once

using namespace std;
class LZ78
{
	struct LZ78Token
	{
		uint16_t index;  // 2 байта для индекса (до 65535 записей)
		unsigned char next_byte;  // 1 байт для символа
	};

	/*Хеш-функция ByteVectorHash 
	нужна для использования векторов байтов (последовательностей символов)
	в качестве ключей в unordered_map (хэш-таблице)
	*/
	struct ByteVectorHash
	{
		size_t operator()(const vector<unsigned char>& vec) const
		{
			size_t hash = 0;
			for (unsigned char b : vec)
			{
				hash = hash * 31 + b; //простой хэш для строк
			}
			return hash;
		}
	};

	void WriteToken(LZ78Token token, BitWriter& writer)
	{
		for (int i = 15; i >= 0; i--)
		{
			writer.WriteBit((token.index >> i) & 1);
		}

		writer.WriteByte(token.next_byte);
	}

	LZ78Token ReadToken(BitReader& reader)
	{
		LZ78Token token;

		token.index = 0;
		for (int i = 15; i >= 0; i--)  // от 8 до 0 (старший -> младший)
		{
			bool bit = reader.ReadBit();
			token.index |= (bit << i);  // старший бит в старшую позицию
		}

		token.next_byte = reader.ReadByte();

		return token;
	}

	unordered_map<vector<unsigned char>, uint16_t, ByteVectorHash> dictionary;
	static constexpr int MAX_DICT_SIZE = 65535;

public:

	uint64_t EncodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
	{
		uint64_t compressedSize = 0;

		BitReader reader{ in };
		BitWriter writer{ out };

		streamsize beg = out.tellp();

		unsigned char zeroByte = 0;
		out.write(reinterpret_cast<const char*>(&zeroByte), 8);
		out.write(reinterpret_cast<const char*>(&zeroByte), 2);

		// число записанных токенов
		uint64_t tokenCount = 0;

		// очищаем словарь, после кодирования предыдущих файлов архива
		dictionary.clear();

		vector<unsigned char> current;//текущая последовательность

		//первый байт пишем в файл как есть
		unsigned char byte = reader.ReadByte();
		writer.WriteByte(byte);

		compressedSize += 1;

		//формируем первую последовательность
		current.push_back(byte);

		// в словаре индексирование начинаем с 1
		int next_index = 1;

		//добавляем в словарь первую последовательность и увеличиваем индекс
		dictionary[current] = next_index++;
		tokenCount++;

		processedBytes.fetch_add(1);

		current.clear();

		// начинаем кодирование
		while (true)
		{
			try
			{
				byte = reader.ReadByte();
				processedBytes.fetch_add(1);
			}
			catch (exception e)
			{
				break;
			}

			// Формируем кандидата - текущую последовательность + новый байт
			vector<uint8_t> candidate = current; // Копируем уже найденную часть
			candidate.push_back(byte); // Добавляем новый прочитанный символ

			// Проверяем, есть ли такая последовательность в словаре
			if (dictionary.find(candidate) != dictionary.end())
			{
				// Если последовательность уже существует в словаре,
	            // расширяем текущую и продолжаем поиск более длинного совпадения
				current = candidate; // Сохраняем расширенную последовательность
			}
			else
			{
				// Нашли новую последовательность, которой нет в словаре
	            // Создаем токен для записи в сжатый поток
				LZ78Token token;

				// Определяем индекс в словаре:
				if (current.empty())
				{
					// Если current пуст - это первый символ последовательности
		            // Индекс 0 означает "пустая строка" (символ без префикса)
					token.index = 0;
				}
				else
				{
					// Берем индекс уже существующей в словаре префиксной части
					token.index = dictionary[current];
				}

				// Символ, который делает последовательность новой
				token.next_byte = byte;

				// Записываем токен (2 байта индекс + 1 байт символ)
				WriteToken(token, writer);
				compressedSize += 3;
				
				// Добавляем новую последовательность в словарь
	            // с очередным доступным индексом
			    dictionary[candidate] = next_index++;
			
				// Очищаем текущую последовательность для поиска нового паттерна
				current.clear();

				tokenCount++;
			}

			//если индекс больше размера словаря, то очищаем словарь
			if (next_index >= MAX_DICT_SIZE)
			{
				// если в current  что-то осталось, то пишем это как есть
				if (!current.empty())
				{
					WriteSequance(current, writer);
					tokenCount++;
					break;
				}

				//очищаем словарь
				dictionary.clear();

				// далее повтрояем то, что делали в начале кодирования
				try
				{
					byte = reader.ReadByte();
					processedBytes.fetch_add(1);
				}
				catch (exception e)
				{
					break;
				}

				writer.WriteByte(byte);
				compressedSize += 1;
				current.push_back(byte);
				next_index = 1;
				dictionary[current] = next_index++;

				current.clear();

				tokenCount++;
			}
		}

		// если в current  что-то осталось, то пишем это как есть
		if (!current.empty())
		{
			WriteSequance(current, writer);
		}
		
		// пишем размер оставшийся current для того, чтобы ее потом декодировать
		uint16_t currentSize = current.size();
		compressedSize += currentSize;
		
		writer.FlushFileBuffer();

		streampos encodedDataEnd = out.tellp();
		out.seekp(beg, std::ios::beg);
		
		out.write(reinterpret_cast<char*>(&tokenCount), 8);
		out.write(reinterpret_cast<char*>(&currentSize), 2);

		out.seekp(encodedDataEnd);

		return compressedSize;
	}
	
	// вспомогательная функция для записи последовательности
	void WriteSequance(vector<unsigned char> sequance, BitWriter& writer)
	{
		for (int i = 0; i < sequance.size(); i++)
		{
			writer.WriteByte(sequance[i]);
		}
	}

	void DecodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
	{
		BitReader reader{ in };
		BitWriter writer{ out };

		uint64_t tokenCount = 0; 
		uint64_t decodedCount = 0;

		in.read(reinterpret_cast<char*>(&tokenCount), 8);

		uint16_t currentSize = 0;
		in.read(reinterpret_cast<char*>(&currentSize), 2);

		// создаем словарь
		vector<vector<unsigned char>> dictionary;
		LZ78Token token;

		int next_index = 2;

		// первый байт пишем как есть
		unsigned char byte = reader.ReadByte();
		writer.WriteByte(byte);

		//делаем первую запись в словаре
		dictionary.push_back({ byte });

		decodedCount++;
		processedBytes.fetch_add(1);

		while (decodedCount < tokenCount)
		{
			token = ReadToken(reader);

			// создаем последовательность
			vector<unsigned char> sequance;

			// если это символ без префикса
			if (token.index == 0)
			{
				sequance = { token.next_byte };
			}
			else // иначе добавляем последовательность из словаря
			{
				sequance = dictionary[token.index - 1];
				sequance.push_back(token.next_byte);
			}

			//добавляем новую последовательнось в словарь
			dictionary.push_back(sequance);

			// пишем ее в файл
			WriteSequance(sequance, writer);
			processedBytes.fetch_add(sequance.size());

			decodedCount++;
			next_index++;

			//обработка тех моментов, когда словарь в кодере сбрасывался
			if (next_index >= MAX_DICT_SIZE && decodedCount != tokenCount - 1)
			{
				dictionary.clear();
				byte = reader.ReadByte();
				writer.WriteByte(byte);
				dictionary.push_back({ byte });
				decodedCount++;
				processedBytes.fetch_add(1);
				next_index = 2;
			}
		}

		for (int i = 0; i < currentSize; i++)
		{
			try
			{
				byte = reader.ReadByte();
				writer.WriteByte(byte);
				processedBytes.fetch_add(1);
			}
			catch (exception e)
			{
				break;
			}
		}

		writer.FlushFileBuffer();
	}
};
