#include <string>
#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstdint>

#pragma once

using namespace std;

bool IsValidArchiveExtansion(string path)
{
    size_t dotPos = path.rfind('.');

    if (dotPos == string::npos || dotPos == path.length() - 1)
        return false;

    string extension = path.substr(dotPos);
    return extension == ".arch";
}

streamsize GetFileSize(ifstream& file)
{
    // Запоминаем текущую позицию
    streampos currentPos = file.tellg();

    // Перемещаемся в конец файла
    file.seekg(0, std::ios::end);
    streamsize size = file.tellg();

    // Возвращаем позицию на начало
    file.seekg(currentPos);

    return size;
}

std::wstring ShowFolderDialog(HWND hwnd, const std::wstring& title = L"Выберите папку для сохранения")
{
    BROWSEINFOW bi = { 0 };
    wchar_t szDir[MAX_PATH] = L"";

    bi.hwndOwner = hwnd;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = NULL;
    bi.lParam = 0;
    bi.pszDisplayName = szDir;

    std::wstring selectedFolder;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL)
    {
        if (SHGetPathFromIDListW(pidl, szDir))
        {
            selectedFolder = szDir;
        }
        CoTaskMemFree(pidl);
    }

    return selectedFolder;
}

wstring ShowOpenFileDialog(HWND hwnd, const wchar_t* filter = L"All Files\0*.*\0")
{
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    wstring fullPath;
    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        fullPath = ofn.lpstrFile;
    }

    return fullPath;
}

wstring GetNameFromPath(const wstring& fullPath)
{
    wstring selectedFile;

    // Находим последний слеш или обратный слеш
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != wstring::npos)
    {
        selectedFile = fullPath.substr(lastSlash + 1);
    }
    else
    {
        selectedFile = fullPath;  // Если слешей нет - возвращаем как есть
    }

    return selectedFile;
}

string  GetNameFromPath(const string& fullPath)
{
    string selectedFile;

    // Находим последний слеш или обратный слеш
    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != string::npos)
    {
        selectedFile = fullPath.substr(lastSlash + 1);
    }
    else
    {
        selectedFile = fullPath;  // Если слешей нет - возвращаем как есть
    }

    return selectedFile;
}

std::wstring StringToWstring(const std::string& str)
{
    if (str.empty()) return std::wstring();

    // Используем системную кодовую страницу для имен файлов
    int size_needed = MultiByteToWideChar(CP_ACP, 0,
        str.c_str(), static_cast<int>(str.size()),
        nullptr, 0);

    if (size_needed <= 0)
    {
        return std::wstring();
    }

    std::wstring wstr(size_needed, 0);
    int result = MultiByteToWideChar(CP_ACP, 0,
        str.c_str(), static_cast<int>(str.size()),
        &wstr[0], size_needed);

    if (result <= 0)
    {
        return std::wstring();
    }

    return wstr;
}

std::string WstringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();

    // Используем системную кодовую страницу для имен файлов
    int size_needed = WideCharToMultiByte(CP_ACP, 0,
        wstr.c_str(), static_cast<int>(wstr.size()),
        nullptr, 0, nullptr, nullptr);

    if (size_needed <= 0)
    {
        return std::string();
    }

    std::string str(size_needed, 0);
    int result = WideCharToMultiByte(CP_ACP, 0,
        wstr.c_str(), static_cast<int>(wstr.size()),
        &str[0], size_needed, nullptr, nullptr);

    if (result <= 0)
    {
        return std::string();
    }

    return str;
}

std::wstring GetTextBoxTextW(HWND hTextBox)
{
    int length = GetWindowTextLength(hTextBox);
    if (length == 0) return L"";

    std::vector<wchar_t> buffer(length + 1);
    GetWindowTextW(hTextBox, buffer.data(), length + 1);
    return std::wstring(buffer.data());
}

std::wstring DoubleToWstring(double value, int precision = 2)
{
    std::wstringstream wss;
    wss << std::fixed << std::setprecision(precision) << value;
    return wss.str();
}
