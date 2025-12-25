#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <queue>
#include <bitset>
#include <list>
#include "FileRW.h"

#pragma once

namespace AdaptiveHuffman
{
    struct Node
    {
        std::shared_ptr<unsigned char> byte;
        uint64_t weight;
        std::shared_ptr<Node> left;
        std::shared_ptr<Node> right;
        std::shared_ptr<Node> parent;

        Node(int w,  std::shared_ptr<unsigned char> byte = nullptr)
            : byte(byte), weight(w), left(nullptr), right(nullptr), parent(nullptr){ }
       
        bool IsLeaf() const
        {
            return !left && !right;
        }

        bool IsNYT() const
        {
            if (IsLeaf() && weight == 0)
                return true;
            else
                return false;
        }

        bool IsByte(unsigned char& byte) const
        {
            if (IsLeaf() && this->byte != nullptr)
            {
                byte = *(this->byte);
                return true;
            }
            else
            {
                return false;
            }   
        }
    };

    class AdaptiveHuffmanFGK
    {
    private:
        std::shared_ptr<Node> root;
        std::vector<std::shared_ptr<Node>> nodes;
        std::map<unsigned char, std::shared_ptr<Node>> byteNodes;
        std::map<int, std::vector<std::shared_ptr<Node>>> levels;
        std::shared_ptr<Node> NYT;
        std::vector<bool> NYTcode;
        std::vector<bool> tempNYTcode;
        std::vector<bool> BYTEcode;
        std::vector<bool> tempBYTEcode;

        void AddNewByte(unsigned char byte)
        {
            // Создаем новый узел для символа
            auto newByteNode = std::make_shared<Node>(1, std::make_shared<unsigned char>(byte));
            auto newParent = std::make_shared<Node>(0);

            if (NYT->parent)
            {
                newParent->parent = NYT->parent;

                if (NYT->parent->left == NYT)
                {
                    NYT->parent->left = newParent;
                }
                else
                {
                    NYT->parent->right = newParent;
                }
                newParent->left = std::make_shared<Node>(0);
                newParent->left->parent = newParent;
                NYT = newParent->left;

                newParent->right = newByteNode;
                newByteNode->parent = newParent;
            }
            else
            {
                root = newParent;
                root->right = newByteNode;
                newByteNode->parent = root;

                root->left = std::make_shared<Node>(0);
                root->left->parent = root;
                NYT = root->left;
            }

            byteNodes[byte] = newByteNode;
        }

        void GetLevelsInfo(std::shared_ptr<Node> l, std::shared_ptr <Node> r, int cnt = 1)
        {
            levels.clear();

            levels[cnt].push_back(l);
            levels[cnt].push_back(r);

            cnt++;
            if (l->left && l->right)
            {
                GetLevelsInfo(l->left, l->right, cnt);
                cnt--;
            }
            if (r->left && r->right)
            {
                GetLevelsInfo(r->left, r->right, cnt);
                cnt--;
            }
        }

        void ClearWeights(std::shared_ptr<Node> root)
        {
            if (root->left)
            {
                ClearWeights(root->left);
            }
            if (root->right)
            {
                ClearWeights(root->right);
            }

            if (!root->parent)
            {
                root->weight = 0;
                return;
            }

            if (!root->IsNYT() && root->byte == nullptr)
                root->weight = 0;
        }

        std::shared_ptr<Node> currentParent;
        void CalculateWeights(std::shared_ptr<Node> root)
        {
            if (root->left)
            {
                CalculateWeights(root->left);
            }
            if (root->right)
            {
                CalculateWeights(root->right);
            }

            if (!root->parent)
            {
                return;
            }

            currentParent = root->parent;

            currentParent->weight += root->weight;

        }

        bool ProcessSiblingProperty(std::shared_ptr<Node> root)
        {
            std::map<int, std::list<std::shared_ptr<Node>>> blocks;
            GetLevelsInfo(root->left, root->right);
            levels[0].push_back(root);

            int prWeight = 0;

            for (int i = 0; i < levels.size(); i++)
            {
                for (int j = levels[i].size() - 1; j > -1; j--)
                {
                    std::shared_ptr<Node> n = levels[i].operator[](j);

                    if (n == root)
                    {
                        blocks[n->weight].push_front(n);
                        prWeight = n->weight;
                        continue;
                    }

                    if (n->weight > prWeight)
                    {
                        Swap(n, blocks[prWeight].back());
                        return false;
                    }

                }
            }

            return true;
        }

        void Swap(std::shared_ptr<Node> first, std::shared_ptr<Node> second)
        {
            if (!first || !second || first == second) return;

            // Сохраняем исходные состояния
            auto firstParent = first->parent;
            auto firstLeft = first->left;
            auto firstRight = first->right;

            auto secondParent = second->parent;
            auto secondLeft = second->left;
            auto secondRight = second->right;

            // Обновляем родительские ссылки у детей
            if (firstLeft) firstLeft->parent = second;
            if (firstRight) firstRight->parent = second;

            if (secondLeft) secondLeft->parent = first;
            if (secondRight) secondRight->parent = first;

            // Обновляем ссылки родителей
            if (firstParent)
            {
                if (firstParent->left.get() == first.get())
                    firstParent->left = second;
                else if (firstParent->right.get() == first.get())
                    firstParent->right = second;
            }

            if (secondParent)
            {
                if (secondParent->left.get() == second.get())
                    secondParent->left = first;
                else if (secondParent->right.get() == second.get())
                    secondParent->right = first;
            }

            // Обновляем узлы
            first->parent = secondParent;
            first->left = secondLeft;
            first->right = secondRight;

            second->parent = firstParent;
            second->left = firstLeft;
            second->right = firstRight;
        }

        void UpdateTree(std::shared_ptr<Node> root)
        {
            ClearWeights(root);
            CalculateWeights(root);

            while (!ProcessSiblingProperty(root))
            {
                ClearWeights(root);
                CalculateWeights(root);
            }
        }

        void ClearCurrentNYTcode()
        {
            NYTcode.clear();
            tempNYTcode.clear();
        }

        void ClearCurrentBYTEcode()
        {
            BYTEcode.clear();
            tempBYTEcode.clear();
        }

        void FindNYT(std::shared_ptr<Node> root)
        {
            if (root->left)
            {
                tempNYTcode.push_back(0);
                FindNYT(root->left);      
                tempNYTcode.pop_back();
            }
            if (root->right)
            {
                tempNYTcode.push_back(1);
                FindNYT(root->right);
                tempNYTcode.pop_back();
            }
            if (root->IsLeaf() && root->byte == nullptr)
            {
                NYTcode = tempNYTcode;
                return;
            }
        }

        void FindByteNode(std::shared_ptr<Node> root, unsigned char byte)
        {
            if (root->left)
            {
                tempBYTEcode.push_back(0);
                FindByteNode(root->left, byte);
                tempBYTEcode.pop_back();
            }
            if (root->right)
            {
                tempBYTEcode.push_back(1);
                FindByteNode(root->right, byte);
                tempBYTEcode.pop_back();
            }
            if ((root->byte != nullptr) && *(root->byte) == byte)
            {
                BYTEcode = tempBYTEcode;
                return;
            }
        }

    public:
        AdaptiveHuffmanFGK()
        {
            NYT = std::make_shared<Node>(0);
            root = NYT;
        }

        void DecodeFile(const char* originFileName, const char* decodedFileName)
        {
            std::ifstream in;
            in.open(originFileName, std::ios::binary);

            std::ofstream out;
            out.open(decodedFileName, std::ios::binary);

            BitReader reader{ in };
            BitWriter writer{ out };

            uint64_t dataSize = reader.ReadBits(64);

            unsigned char firstByte = reader.ReadByte();
            writer.WriteByte(firstByte);

            AddNewByte(firstByte);
            UpdateTree(root);

            bool bit;
            std::shared_ptr<Node> currentNode = root;
            unsigned char encodedByte;

            int cnt = 1;
            while (cnt < dataSize)
            {
                try
                {
                    bit = reader.ReadBit();
                }
                catch (std::exception e)
                {
                    if (std::string(e.what()) == "End of file")
                        break;
                }

                if (bit == 1)
                {
                    currentNode = currentNode->right;
                }
                else
                {
                    currentNode = currentNode->left;
                }

                if (currentNode->IsNYT())
                {
                    unsigned char unEncodedByte;
                    try
                    {
                        unEncodedByte = reader.ReadByte();
                    }
                    catch (std::exception e)
                    {
                        if (std::string(e.what()) == "End of file")
                            break;
                    }
                    AddNewByte(unEncodedByte);
                    writer.WriteByte(unEncodedByte);
                    cnt++;
                    UpdateTree(root);
                    currentNode = root;
                }
                else if (currentNode->IsByte(encodedByte))
                {
                    writer.WriteByte(encodedByte);
                    cnt++;
                    byteNodes[encodedByte]->weight++;
                    UpdateTree(root);
                    currentNode = root;
                }
             
             
            }

            writer.FlushFileBuffer();
        }

        void WriteUInt64(std::ofstream& out, uint64_t value)
        {
            // Преобразуем в big-endian (network byte order)
            unsigned char bytes[8];
            for (int i = 0; i < 8; i++)
            {
                bytes[7 - i] = (value >> (i * 8)) & 0xFF;
            }
            out.write(reinterpret_cast<const char*>(bytes), 8);
        }

        void EncodeFile(const char* originFileName, const char* encodedFileName)
        {
            std::ifstream in;
            in.open(originFileName, std::ios::binary);

            std::ofstream out;
            out.open(encodedFileName, std::ios::binary);

            BitReader reader{ in };
            BitWriter writer{ out };

            unsigned char zeroByte = 0;
            out.write(reinterpret_cast<const char*>(&zeroByte), 8);

            unsigned char firstByte = reader.ReadByte();
            writer.WriteByte(firstByte);

            AddNewByte(firstByte);
            UpdateTree(root);

            unsigned char byte;

            uint64_t dataSize = 1;

            while (true)
            {
                try
                {
                    byte = reader.ReadByte();
                }
                catch (std::exception e)
                {
                    if (std::string(e.what()) == "End of file")
                        break;
                }

                if (byteNodes.find(byte) == byteNodes.end())
                {

                    ClearCurrentNYTcode();
                    FindNYT(root);
                    for (int i = 0; i < NYTcode.size(); i++)
                    {
                        writer.WriteBit(NYTcode[i]);
                    }

                    writer.WriteByte(byte);
                    AddNewByte(byte);
                }
                else if (byteNodes.find(byte) != byteNodes.end())
                {
                    
                    ClearCurrentBYTEcode();
                    
                    FindByteNode(root, byte);
                    for (int i = 0; i < BYTEcode.size(); i++)
                    {
                        writer.WriteBit(BYTEcode[i]);
                    }
                    byteNodes[byte]->weight++;
                }
                UpdateTree(root);
                dataSize++;
            }
            writer.FlushFileBuffer();

            std::streampos encodedDataEnd = out.tellp();
            out.seekp(0, std::ios::beg);
            WriteUInt64(out, dataSize);
            out.seekp(encodedDataEnd);
        }
    };
}
