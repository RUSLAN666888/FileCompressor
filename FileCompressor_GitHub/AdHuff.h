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

#pragma once

using namespace std;

	class AdaptiveHuffmanCoder
	{
		// Внутренняя структура для узла дерева Хаффмана
        // Наследуется от enable_shared_from_this для безопасного получения shared_ptr
		struct Node : public std::enable_shared_from_this<Node>
		{
			optional<unsigned char> byte; // Символ (байт), который хранит узел (для листьев)
			int weight;// Вес узла = сумма частот символов в поддереве
			int number;// Уникальный номер узла (1-512)
			shared_ptr<Node> left;// Левый потомок
			shared_ptr<Node> right;// Правый потомок
			shared_ptr<Node> parent;// Родительский узел
			bool isNYT;// Флаг, является ли узел NYT (Not Yet Transmitted)

			// Статические фабричные методы для создания узлов

			// Создание узла без родителя (для корня)
			static shared_ptr<Node> Create()
			{
				auto node = make_shared<Node>();
				node->weight = 0;
				node->isNYT = false;
				return node;
			}

			// Создание узла с родителем (для внутренних узлов)
			static shared_ptr<Node> Create(shared_ptr<Node> parent)
			{
				auto node = make_shared<Node>();
				node->parent = parent;
				node->weight = 0;
				node->isNYT = false;
				return node;
			}

			// Создание листового узла с символом
			static shared_ptr<Node> Create(shared_ptr<Node> parent, unsigned char byte_val)
			{
				auto node = make_shared<Node>();
				node->parent = parent;
				node->byte = byte_val; // Устанавливаем символ
				node->weight = 1; // Начальный вес = 1
				node->isNYT = false; // Это не NYT-узел
				return node;
			}

			/* FindOrDefault
			* Рекурсивно обходит дерево Хаффмана (в глубину)
			* Ищет узел, который содержит заданный байт
			* Возвращает указатель на найденный узел или nullptr, если узел не найден
			*/
			shared_ptr<Node> FindOrDefault(unsigned char byte)
			{
				// Если текущий узел - лист и содержит искомый байт
				if (this->byte.has_value() && this->byte.value() == byte)
					return shared_from_this();  // Возвращаем указатель на себя (через enable_shared_from_this)

				// Рекурсивно ищем в левом поддереве
				if (left)
				{
					auto result = left->FindOrDefault(byte);
					if (result != nullptr)
						return result;
				}

				// Рекурсивно ищем в правом поддереве
				if (right)
				{
					return right->FindOrDefault(byte);
				}

				return nullptr; // Символ не найден в дереве
			}

			// Получение кода Хаффмана для заданного узла
			string GetCode(shared_ptr<Node> searched)
			{
				return GetCode(searched, "");
			}

			// Получение кода для NYT-узла (нужен для кодирования новых символов)
			string GetNYTCode(string code)
			{
				if (isNYT)  // Если текущий узел - NYT
					return code;// Возвращаем накопленный код

				// Если узел - лист (не NYT)
				if (left == nullptr && right == nullptr)
					return "";

				// Рекурсивно ищем NYT в левом поддереве (добавляем '0' к коду)
				string result = left->GetNYTCode(code + "0");
				if (!result.empty())
					return result;

				// Рекурсивно ищем NYT в правом поддереве (добавляем '1' к коду)
				return right->GetNYTCode(code + "1");
			}

			// Проверка, является ли переданный узел левым потомком
			bool IsLeftSon(shared_ptr<Node> son)
			{
				return left == son;
			}

			// Проверка, является ли переданный узел правым потомком
			bool IsRightSon(shared_ptr<Node> son)
			{
				return right == son;
			}

			// Проверка, является ли узел листом (нет потомков)
			bool IsLeaf()
			{
				return !left && !right;
			}

		private:
			// Рекурсивный метод для построения кода Хаффмана для заданного узла
	        // code - накопленный код (путь от корня до текущего узла)
			string GetCode(shared_ptr<Node> searched, string code)
			{
				// Если текущий узел - искомый лист (сравниваем символы)
				if (this->byte.has_value() && searched->byte.has_value()
					&& this->byte == searched->byte)
					return code; // Возвращаем накопленный код

				// Если узел - лист, но не искомый
				if (left == nullptr && right == nullptr)
					return "";

				// Рекурсивно ищем в левом поддереве (добавляем '0' к коду)
				string result = left->GetCode(searched, code + "0");
				if (!result.empty())
					return result;

				// Рекурсивно ищем в правом поддереве (добавляем '1' к коду)
				return right->GetCode(searched, code + "1");
			}
		};


	public:
		shared_ptr<Node> root; // Корень дерева Хаффмана
		shared_ptr<Node> NYT;  // Указатель на текущий NYT-узел

		/*
   * Массив всех узлов дерева (513 элементов):
   * - 256 листьев (по одному на каждый возможный байт 0-255)
   * - 255 внутренних узлов (в полном бинарном дереве с N листьями всегда N-1 внутренних узлов)
   * - 1 корневой узел
   * - 1 NYT-узел (специальный узел для новых символов)
   *
   * Нумерация узлов:
   * - Корень имеет номер 512 (максимальный)
   * - Номера уменьшаются от корня к листьям (511, 510, ...)
   * - Каждому узлу присваивается уникальный номер от 1 до 512
   * - Номера используются для быстрого поиска узлов с одинаковым весом
   */
		shared_ptr<Node> nodes[513];

		int nextNum; // Следующий свободный номер для нового узла

		AdaptiveHuffmanCoder() { Reset(); }

		// Сброс состояния кодера (инициализация нового дерева)
		void Reset()
		{
			// Создаем корневой узел (он же начальный NYT)
			root = Node::Create();
			root->number = 512;  // Корень получает максимальный номер
			NYT = root;  // Вначале корень является NYT-узлом
			nodes[root->number] = root; // Сохраняем корень в массиве
			nextNum = 511; // Следующий узел получит номер 511
		}

		// Преобразование байта в строку из 8 бит (бинарное представление)
		string ByteToString(unsigned char byte)
		{
			string result = "";

			// Проходим по всем 8 битам, начиная со старшего (7-й бит)
			for (int i = 7; i >= 0; i--)
				result += ((byte >> i) & 1) ? "1" : "0"; // Извлекаем i-й бит

			return result;
		}

		/*
   * Чтение байта из потока битов при декодировании
   * Алгоритм:
   * 1. Начинаем с корня дерева
   * 2. Читаем биты и движемся по дереву (0 - влево, 1 - вправо)
   * 3. Если достигли NYT-узла -> возвращаем nullopt (символ новый)
   * 4. Если достигли листа -> возвращаем символ из узла
   */
		optional<unsigned char> ReadByte(BitReader& reader)
		{
			shared_ptr<Node> current = root; // Начинаем с корня

			while (true)
			{
				if (current->isNYT) // Если достигли NYT-узла
					return std::nullopt; // Возвращаем nullopt - символ новый

				if (current->IsLeaf())  // Если достигли листа
					return current->byte; // Возвращаем символ

				// Читаем следующий бит из потока
				bool bit;
				try
				{
					bit = reader.ReadBit();
				}
				catch (std::exception e)
				{
					// Если достигнут конец файла - прерываем цикл
					if (std::string(e.what()) == "End of file")
						break;
				}

				// Двигаемся по дереву в зависимости от прочитанного бита
				if (bit == false)// 0 -> идем влево
				{
					if (!current->left)
						throw std::runtime_error("Left child is null");
					current = current->left;
				}
				else// 1 -> идем вправо
				{
					if (!current->right)
						throw std::runtime_error("Right child is null");
					current = current->right;
				}
			}		
		}

		/*
   * Метод декодирования файла
   * Формат сжатых данных для адаптивного Хаффмана:
   * [Размер данных]     : 8 байт (uint64_t) - количество исходных байтов
   * [Дополнение]        : 1 байт (uint8_t) - количество битов дополнения в конце
   * [Закодированные данные] : битовый поток
   */
		void DecodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
		{
			// Инициализируем читателя и писателя битов
			BitReader reader{ in };
			BitWriter writer{ out };

			// Читаем метаданные:
	        // 1. Размер исходных данных (сколько байтов нужно восстановить)
			uint64_t dataSize = 0;
			in.read(reinterpret_cast<char*>(&dataSize), 8);

			// 2. Количество битов дополнения в конце потока
			uint8_t padding = 0;
			in.read(reinterpret_cast<char*>(&padding), 1);

			uint64_t decodedCount = 0; // Счетчик раскодированных байтов

		    // Основной цикл декодирования
			while (decodedCount < dataSize)
			{
				shared_ptr<Node> node;
				int count;
				optional<unsigned char> byte = ReadByte(reader);

				//Новый символ (через NYT)
				if (!byte.has_value())
				{
					unsigned char newSymbol;
					try
					{
						// Читаем следующий байт напрямую(8 бит) - это новый символ
						newSymbol = reader.ReadByte();
					}
					catch (std::exception e)
					{
						if (std::string(e.what()) == "End of file")
							break;
					}
					// Добавляем новый символ в дерево через NYT
					node = AddToNYT(newSymbol);
					byte = newSymbol;// Это символ для записи
				}
				else //существующий символ
				{
					// Находим узел с этим символом в дереве
					node = root->FindOrDefault(byte.value());
					node->weight++; // Увеличиваем частоту символа
				}

				// Обновляем веса в дереве (поддерживаем свойство Хаффмана)
				UpdateAll(node->parent);

				// Записываем раскодированный байт в выходной поток
				writer.WriteByte(byte.value());
				decodedCount++;

				// Обновляем счетчик обработанных байтов (для прогресс-бара)
				processedBytes.fetch_add(1);
			}

			// Записываем оставшиеся биты из буфера
			writer.FlushFileBuffer();

			// Пропускаем биты дополнения (выравнивание до байта)
			for (int i = 0; i < padding; i++)
			{
				reader.ReadBit();
			}
		}

		/*
	* Метод кодирования файла
	* Возвращает размер сжатых данных в байтах
	* Формат выходных данных:
	* [Заглушка под размер]    : 8 байт (позже будет заменено реальным размером)
	* [Заглушка под дополнение]: 1 байт (позже будет заменено реальным значением)
	* [Закодированные данные]  : битовый поток
	*/
		uint64_t EncodeFile(ifstream& in, ofstream& out, atomic<uint64_t>& processedBytes)
		{
			uint64_t compressedSize = 0; // Размер сжатых данных в битах

			// Инициализируем читателя и писателя битов
			BitReader reader{ in };
			BitWriter writer{ out };

			// Запоминаем позицию для записи метаданных
			streamsize beg = out.tellp();

			//пишем заглушки для метаданных
			unsigned char zeroByte = 0;
			out.write(reinterpret_cast<const char*>(&zeroByte), 8); // Под размер данных
			out.write(reinterpret_cast<const char*>(&zeroByte), 1); // Под дополнение

			uint64_t dataSize = 0; // Количество исходных байтов
			unsigned char byte;

			// Основной цикл кодирования
			while (true)
			{
				try
				{
					// Читаем очередной байт из входного файла
					byte = reader.ReadByte();
					processedBytes.fetch_add(1); // Обновляем прогресс
				}
				catch (exception e)
				{
					// Если достигнут конец файла - прерываем цикл
					if (string(e.what()) == "End of file")
						break;
				}

				//получаем код байта в строковом виде
				string code = Encode(byte);

				//просто записывам этот код в файл
				for (int i = 0; i < code.length(); i++)
				{
					if (code[i] == '1')
						writer.WriteBit(1);
					else
						writer.WriteBit(0);

					compressedSize++; // Увеличиваем счетчик битов
				}
				dataSize++; // Увеличиваем счетчик исходных байтов
			}

			// Записываем оставшиеся биты из буфера
			writer.FlushFileBuffer();

			// Получаем количество битов дополнения (выравнивание до байта)
			uint8_t padding = writer.GetPaddingBits();

			// Запоминаем позицию конца закодированных данных
			std::streampos encodedDataEnd = out.tellp();
			out.seekp(beg, std::ios::beg);

			// Возвращаемся к началу файла, чтобы записать реальные метаданные
			out.write(reinterpret_cast<char*>(&dataSize), 8);
			out.write(reinterpret_cast<char*>(&padding), 1);

			// Возвращаемся в конец файла
			out.seekp(encodedDataEnd);


			// Возвращаем размер сжатых данных в байтах
	        // (compressedSize в битах + padding битов) / 8 = байты
			return (compressedSize + static_cast<uint64_t>(padding)) / 8;
		}

		/*
   * Основной метод кодирования одного символа
   * Алгоритм:
   * 1. Проверяем, есть ли символ в дереве
   * 2. Если есть - получаем его текущий код и увеличиваем вес
   * 3. Если нет - получаем код NYT + 8 бит символа и добавляем символ в дерево
   * 4. Обновляем дерево (поддерживаем свойство Хаффмана)
   */
		string Encode(unsigned char byte)
		{
			shared_ptr<Node> node = root->FindOrDefault(byte); //проверяем, встречался ли этот байт в дереве
			string code = "";

			if (node) //байт уже есть в дереве
			{
				code = root->GetCode(node); // Получаем текущий код байта
				node->weight++; // Увеличиваем частоту
			}
			else //байт - новый
			{
				code = root->GetNYTCode(""); // Код для NYT-узла
				code += ByteToString(byte); // Добавляем сам байт (8 бит)
				node = AddToNYT(byte); // Добавляем байт в дерево
			}

			UpdateAll(node->parent);

			return code;
		}

		/*
   * Добавляет новый символ в дерево через NYT-узел
   * Алгоритм:
   * 1. Создаем новый лист с символом как правый потомок текущего NYT
   * 2. Создаем новый NYT-узел как левый потомок текущего NYT
   * 3. Обновляем указатель NYT на новый NYT-узел
   * 4. Старый NYT становится внутренним узлом
   */
		shared_ptr<Node> AddToNYT(unsigned char byte)
		{
			// Создаем новый лист с символом (правый потомок текущего NYT)
			auto node = Node::Create(NYT, byte);
			node->number = nextNum; // Присваиваем номер
			NYT->right = node; // Делаем правым потомком
			nodes[nextNum--] = node;  // Сохраняем в массиве

			// Создаем новый NYT-узел (левый потомок текущего NYT)
			auto _NYT = Node::Create(NYT);
			_NYT->number = nextNum;  // Присваиваем следующий номер
			_NYT->isNYT = true;   // Помечаем как NYT

			NYT->isNYT = false; // Старый NYT больше не NYT
			NYT->left = _NYT;  // Делаем левым потомком
			nodes[nextNum--] = _NYT;  // Сохраняем в массиве

			// Обновляем указатель на текущий NYT
			NYT = _NYT;

			return node; // Возвращаем созданный лист с символом
		}

		// Обновляет веса всех узлов от заданного узла до корня
		void UpdateAll(shared_ptr<Node> node)
		{
			while (node)
			{
				Update(node);  // Обновляем текущий узел
				node = node->parent; // Переходим к родителю
			}
		}

		// Обновление одного узла (увеличение веса и перебалансировка при необходимости)
		void Update(shared_ptr<Node> node)
		{
			// Ищем узел с таким же весом, но с бОльшим номером (ближе к корню)
		    // Свойство Хаффмана: узлы с одинаковым весом должны быть как можно дальше от корня
			shared_ptr<Node> toReplace = NodeToReplace(node->number, node->weight);

			// Если нашли узел для замены и это не родитель текущего узла
			if (toReplace && node->parent != toReplace)
				Replace(node, toReplace);  // Меняем узлы местами

			node->weight++; // Увеличиваем вес узла
		}

		/*
   * Ищет узел с таким же весом, но с бОльшим номером
   * Больший номер = узел ближе к корню (номера уменьшаются от корня)
   * Свойство Хаффмана: узлы с меньшим весом должны быть дальше от корня
   * Если узел с таким же весом ближе к корню - нарушение свойства
   */
		shared_ptr<Node> NodeToReplace(int startIndex, int weight)
		{
			startIndex++; // Ищем начиная со следующего номера (ближе к корню)
			shared_ptr<Node> found = nullptr;

			// Проходим по массиву узлов (от ближайших к корню)
			for (int i = startIndex; i < 513; i++)
			{
				if (nodes[i]->weight == weight)
					found = nodes[i]; // Нашли узел с таким же весом
			}

			return found;
		}

		// Замена двух узлов местами (перебалансировка дерева)
	    void Replace(shared_ptr<Node> a, shared_ptr<Node> b)
		{
			ReplaceNumbers(a, b); // Меняем номера узлов
			ReplaceSons(a, b);  // Меняем связи между узлами
		}

		// Замена номеров узлов в массиве
		void ReplaceNumbers(shared_ptr<Node> a, shared_ptr<Node> b)
		{
			// Меняем указатели в массиве nodes
			shared_ptr<Node> temp = nodes[a->number];
			nodes[a->number] = nodes[b->number];
			nodes[b->number] = temp;

			// Меняем номера у самих узлов
			int tempNum = a->number;
			a->number = b->number;
			b->number = tempNum;
		}

		// Замена связей между узлами (кто чей потомок)
		void ReplaceSons(shared_ptr<Node> a, shared_ptr<Node> b)
		{
			// Запоминаем, с какой стороны b был у своего родителя
			bool bIsLeftSon = b->parent->IsLeftSon(b);

			// Заменяем a на b у родителя a
			if (a->parent->IsLeftSon(a))
				a->parent->left = b; // Было a, стало b
			else
				a->parent->right = b;  // Было a, стало b

			// Меняем родителей местами
			shared_ptr<Node> temp = b->parent;
			b->parent = a->parent; // b теперь указывает на родителя a
			a->parent = temp; // a теперь указывает на родителя b

			// Заменяем b на a у бывшего родителя b
			if (bIsLeftSon)
				temp->left = a; // На месте b теперь a
			else
				temp->right = a; // На месте b теперь a
		}
	};
