#include <windows.h>
#include <wingdi.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <atomic>
#include <thread>
#include <utility>
#include "Utilities.h"
#include "ArchiveManager.h"
#include <set>


using namespace std;

class Button
{
public:
    enum class ButtonState// перечисление, описывающие состояние кнопки
    {
        Normal = 0,
        Hovered = 1,
        Pressed = 2
    };

private:
    wstring content = L"";//текст на кнопке
    RECT body;//вся область, занимаемая кнопкой
    RECT top, bottom, left, right;// границы кнопки
    ButtonState state = ButtonState::Normal;
    int borderThickness;
    COLORREF hovered, noHovered, pressed;

public:
    bool isTargeted = false;
    bool preIsTargeted = false;
    string name; //имя, назначаемое кнопке при создании, для дальнейшего обращение к ней в коде (задается в конструкторе)
    Button(string name) : name{ name } {}

    // установка размеров кнопки
    void SetGeometry(POINT leftTop, POINT rightBottom, wstring text, int borderThickness)
    {
        body = { leftTop.x, leftTop.y, rightBottom.x, rightBottom.y };
        content = text;
        this->borderThickness = borderThickness;
    }

    void SetColor(COLORREF hovered, COLORREF normal, COLORREF pressed)
    {
        this->hovered = hovered;
        this->noHovered = normal;
        this->pressed = pressed;
    }

    void Draw(HDC hdc)
    {
        // внутренняя заливка кнопки
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &body, black);
        DeleteObject(black);

        COLORREF borderColor;

        switch (state)
        {
        case ButtonState::Normal:
            borderColor = noHovered;
            break;
        case ButtonState::Hovered:
            borderColor = hovered;
            break;
        case ButtonState::Pressed:
            borderColor = pressed;
            break;
        }

        HBRUSH border = CreateSolidBrush(borderColor);

        //отрисовка границ кнопки
        top = { body.left, body.top, body.right, body.top + borderThickness };
        FillRect(hdc, &top, border);

        bottom = { body.left, body.bottom - borderThickness, body.right, body.bottom };
        FillRect(hdc, &bottom, border);

        left = { body.left, body.top, body.left + borderThickness, body.bottom };
        FillRect(hdc, &left, border);

        right = { body.right - borderThickness, body.top, body.right, body.bottom };
        FillRect(hdc, &right, border);

        DeleteObject(border);

        SetBkMode(hdc, TRANSPARENT);//установка прозрачного фона для текста
        SetTextColor(hdc, borderColor);
        SetText(content, hdc);
    }

    void SetText(std::wstring text, HDC hdc)
    {

        HFONT hFont = CreateFont(
            16,                           // Высота 16px
            0, 0, 0,                     // Ширина, угол и т.д. по умолчанию
            FW_NORMAL,                    // Нормальное начертание
            FALSE, FALSE, FALSE,         // Не курсив, не подчеркнутый, не зачеркнутый
            DEFAULT_CHARSET,              // Поддержка всех символов
            OUT_DEFAULT_PRECIS,           // Качество вывода
            CLIP_DEFAULT_PRECIS,          // Обрезка
            DEFAULT_QUALITY,              // Качество
            DEFAULT_PITCH | FF_DONTCARE,  // Шрифт пропорциональный
            L"Arial"                      // Семейство шрифтов
        );

        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        // пишем текст по центру в одну строку
        DrawText(hdc, text.c_str(), -1, &body, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }

    void SetState(ButtonState newState)
    {
        this->state = newState;
    }

    ButtonState GetState() const
    {
        return state;
    }

    RECT GetBodyInfo()
    {
        return body;
    }
};

class RadioButton
{

private:
    wstring content = L""; // Текст, отображаемый рядом с радиокнопкой
    RECT body;  // Прямоугольник, охватывающий всю область элемента (кнопка + текст)
    RECT squareRect; // Прямоугольник, охватывающий только квадрат радиокнопки
    int squareSize; // Размер стороны квадрата радиокнопки в пикселях
    bool isChecked = false; // Состояние радиокнопки : true - выбрана, false - не выбрана

public:
    string name;// / Имя / идентификатор радиокнопки для логики программы

    RadioButton(string name) : name{name}{}

    // Геттеры для получения геометрии элемента
    RECT GetSquareRECT() const { return squareRect; }
    RECT GetBody() const { return body; }

    // Методы для управления состоянием радиокнопки
    void SetState(bool newState) { isChecked = newState; }
    bool GetState() const { return isChecked; }

    // Метод для установки геометрии и текста радиокнопки
    void SetGeometry(POINT leftTop, int size, std::wstring text)
    {
        squareSize = size;// Сохраняем размер квадрата
        int textWidth = CalculateTextWidth(text);  // Рассчитываем ширину текста для правильного позиционирования

        // Вычисляем общую область элемента:
        // leftTop.x, leftTop.y - начальная точка
        // leftTop.x + size + textWidth + 10 - ширина квадрата + текст + отступ
        // leftTop.y + size - высота элемента равна высоте квадрата
        body = { leftTop.x, leftTop.y, leftTop.x + size + textWidth + 10, leftTop.y + size };

        // Вычисляем область квадрата радиокнопки:
       // Квадрат начинается в leftTop и имеет размер size x size
        squareRect = { leftTop.x, leftTop.y, leftTop.x + size, leftTop.y + size };

        content = text;// Сохраняем текст для отображения
    }

    // Метод для вычисления ширины текста в пикселях
    int CalculateTextWidth(const std::wstring& text)
    {
        // Получаем контекст устройства экрана для расчета размеров текста
       // GetDC(NULL) получает HDC для всего экрана
        HDC hdc = GetDC(NULL);

        // Рассчитываем размер шрифта пропорционально размеру квадрата
        // Минимальный размер шрифта - 12 пикселей
        int fontSize = max(12, squareSize * 2 / 3);

        // Создаем шрифт Arial с рассчитанным размером
        HFONT hFont = CreateFont(
            fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
        );

        // Выбираем созданный шрифт в контекст устройства, сохраняя старый
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        // Структура для хранения размеров текста
        SIZE textSize;
        // Рассчитываем размеры текста в пикселях
        GetTextExtentPoint32(hdc, text.c_str(), (int)text.length(), &textSize);

        // Восстанавливаем старый шрифт и удаляем созданный
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);

        // Освобождаем контекст устройства
        ReleaseDC(NULL, hdc);

        // Возвращаем ширину текста
        return textSize.cx;
    }

    // Метод для установки и отрисовки текста
    void SetText(HDC hdc)
    {
        // Создаем шрифт Arial пропорциональный размеру квадрата
        int fontSize = max(12, squareSize * 2 / 3);
        HFONT hFont = CreateFont(
            fontSize,                    // Высота шрифта
            0, 0, 0,                    // Ширина, угол и т.д. по умолчанию
            FW_NORMAL,                   // Нормальное начертание
            FALSE, FALSE, FALSE,        // Не курсив, не подчеркнутый, не зачеркнутый
            DEFAULT_CHARSET,             // Поддержка всех символов
            OUT_DEFAULT_PRECIS,          // Качество вывода
            CLIP_DEFAULT_PRECIS,         // Обрезка
            DEFAULT_QUALITY,             // Качество
            DEFAULT_PITCH | FF_DONTCARE, // Шрифт пропорциональный
            L"Arial"                     // Семейство шрифтов
        );

        // Выбираем созданный шрифт в контекст устройства
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        // Создаем прямоугольник для текста (справа от квадрата радиокнопки)
         // textRect.right = body.right - граница совпадает с правой границей всего элемента
        RECT textRect = {
            squareRect.right + 5, // Начинаем через 5 пикселей от правого края квадрата
            squareRect.top, // Верхняя граница совпадает с верхом квадрата
            body.right, // Правая граница - правая граница всего элемента
            body.bottom // Нижняя граница - низ всего элемента
        };

        // Отрисовываем текст в заданном прямоугольнике
       // DT_LEFT - выравнивание по левому краю
       // DT_VCENTER - вертикальное центрирование
       // DT_SINGLELINE - текст в одну строку
        DrawText(hdc, content.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Восстанавливаем старый шрифт и удаляем созданный
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }

    // Основной метод отрисовки радиокнопки
    void Draw(HDC hdc)
    {
        if (isChecked)
        {
            // Активное состояние - оранжевый квадрат с белой рамкой
            HBRUSH orangeBrush = CreateSolidBrush(RGB(255, 165, 0)); // Оранжевый цвет
            FillRect(hdc, &squareRect, orangeBrush);
            DeleteObject(orangeBrush);

            // Белая рамка толщиной 1px
            HBRUSH whiteBorder = CreateSolidBrush(RGB(255, 255, 255));
            FrameRect(hdc, &squareRect, whiteBorder);
            DeleteObject(whiteBorder);
        }
        else
        {
            // Неактивное состояние - черный фон
            HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &squareRect, blackBrush);
            DeleteObject(blackBrush);

            // Белая рамка толщиной 1px
            HBRUSH borderBrush = CreateSolidBrush(RGB(255, 255, 255));
            FrameRect(hdc, &squareRect, borderBrush);
            DeleteObject(borderBrush);
        }
        // Рисуем текст
        SetBkMode(hdc, TRANSPARENT);

        SetTextColor(hdc, RGB(255,255,255));
        SetText(hdc);
    }
};

class ProgressBar
{
    RECT body; // Прямоугольник всей области прогресс-бара (включая рамку)
    RECT progress; // Прямоугольник заполненной части прогресс-бара
    bool state = false; // Состояние активности прогресс-бара (включен/выключен)
    int currentProgress; // Текущее значение прогресса (0..maxProgress)
    int maxProgress; // Максимальное значение прогресса (по умолчанию 100)
public:
    // Конструктор: инициализирует прогресс-бар значениями по умолчанию
    ProgressBar() : currentProgress(0), maxProgress(100) {}

    // Геттер для получения области отрисовки прогресс-бара
    RECT GetBody() { return body; }


    // Метод установки геометрии прогресс-бара
    // b - прямоугольник, определяющий положение и размер элемента
    void SetGeometry(RECT b)
    {
        body = b;
        // Инициализируем прямоугольник прогресса:
       // Начинается с отступа в 1 пиксель от границ body
       // Изначально имеет нулевую ширину (left = right)
        progress = {
            b.left + 1,   // Начинаем с отступа 1px от левой границы
            b.top + 1,    // Отступ 1px от верхней границы
            b.left + 1,   // Правая граница равна левой (нулевая ширина)
            b.bottom - 1  // Отступ 1px от нижней границы
        };
    }
    // Метод установки максимального значения прогресса
    void SetMaxProgress(int max)
    {
        // Защита от некорректных значений: если max <= 0, устанавливаем 100
        maxProgress = (max > 0) ? max : 100;

        UpdateProgressRect(); // Обновляем отображение после изменения максимума
    }

    // Методы управления состоянием прогресс-бара
    void SetState(bool newState) { state = newState; }
    bool GetState() { return state; }

    // Метод установки текущего значения прогресса
    void SetProgress(int value)
    {
        // Ограничиваем значение в диапазоне [0, maxProgress]
        currentProgress = (value < 0) ? 0 : (value > maxProgress) ? maxProgress : value;

        // Обновляем прямоугольник заполнения
        UpdateProgressRect();
    }

    // Геттеры для получения значений прогресса
    int GetProgress() { return currentProgress; }
    int GetMaxProgress() { return maxProgress; }

    // Основной метод отрисовки прогресс-бара
    void Draw(HDC hdc)
    {
        // Создаем белую перо для контура (толщина 1px, сплошная линия)
        HPEN white = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        HPEN hOldPen = (HPEN)SelectObject(hdc, white);

        // Создаем черную кисть для заливки фона
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, black);

        // Рисуем прямоугольник прогресс-бара (рамка + заливка)
        // Rectangle() рисует прямоугольник с текущими пером и кистью
        Rectangle(hdc, body.left, body.top, body.right, body.bottom);

        // Восстанавливаем старые GDI-объекты
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);

        // Удаляем созданные объекты для избежания утечек ресурсов GDI
        DeleteObject(white);
        DeleteObject(black);

        //Отрисовка заполненной части(если прогресс - бар активен и есть прогресс)
        if (state && currentProgress > 0)
        {
            // Создаем оранжевую кисть для заливки прогресса
            HBRUSH orange = CreateSolidBrush(RGB(255, 165, 0));
            hOldBrush = (HBRUSH)SelectObject(hdc, orange);

            // Заливаем прямоугольник прогресса оранжевым цветом
            FillRect(hdc, &progress, orange);

            SelectObject(hdc, hOldBrush);
            DeleteObject(orange); 
        }
    }

    // Метод увеличения прогресса на 1 единицу
    void IncreaseProgress()
    {
        // Увеличиваем только если не достигнут максимум
        if (currentProgress < maxProgress)
        {
            currentProgress++;
            UpdateProgressRect(); // Обновляем отображение
        }
    }

    // Перегруженный метод увеличения прогресса на заданное количество
    void IncreaseProgress(int amount)
    {
        // Увеличиваем текущий прогресс
        currentProgress += amount;

        // Проверяем границы диапазона
        if (currentProgress > maxProgress)
            currentProgress = maxProgress;// Не превышаем максимум
        else if (currentProgress < 0)
            currentProgress = 0; // Не опускаемся ниже нуля

        UpdateProgressRect();// Обновляем отображение
    }

    // Метод сброса прогресса в ноль
    void Reset()
    {
        currentProgress = 0;
        UpdateProgressRect();
    }

private:
    // Приватный метод обновления прямоугольника заполненной части
    void UpdateProgressRect()
    {
        // Защита от деления на ноль
        if (maxProgress == 0) return;

        // Рассчитываем доступную ширину для заполнения (исключая границы)
       // -2 пикселя: по 1 пикселю с каждой стороны для рамки
        int bodyWidth = body.right - body.left - 2; 

        // Рассчитываем ширину заполненной части пропорционально прогрессу
        // Формула: (текущий_прогресс * общая_ширина) / максимум_прогресса
        int progressWidth = (currentProgress * bodyWidth) / maxProgress;

        // Обновляем координаты прямоугольника прогресса
        progress.left = body.left + 1; // Отступ 1px от левой границы
        progress.right = body.left + 1 + progressWidth;// Правая граница = левая + ширина прогресса
        progress.top = body.top + 1; // Отступ 1px от верхней границы
        progress.bottom = body.bottom - 1; // Отступ 1px от нижней границы
    }
};

// Класс TextBox - кастомный элемент управления для отображения текста с поддержкой скроллинга
class TextBox
{
    RECT rect;// Прямоугольник всей области текстового поля
    vector<wstring> linesNoPaths; // Текст для отображения (без путей, форматированный)
    vector<wstring> linesWithPaths;// Полный текст с путями (оригинальные данные)
    int scrollPos = 0; // Текущая позиция прокрутки (индекс первой видимой строки)
    int maxVisibleLines = 0;// Максимальное количество строк, помещающихся в видимой области
    int lineHeight = 20; // Высота одной строки текста в пикселях
    int margin = 5; // Отступ от краев для текста
    bool hasScrollbar = false;// Флаг наличия вертикальной полосы прокрутки
    string name;// Имя/идентификатор текстового поля

    // Цветовая схема элемента
    COLORREF textColor = RGB(255, 255, 255);// Цвет текста (белый)
    COLORREF bgColor = RGB(0, 0, 0); // Цвет фона (черный)
    COLORREF borderColor = RGB(255, 255, 255);// Цвет рамки (белый)
    COLORREF scrollbarColor = RGB(200, 200, 200);// Цвет фона скроллбара (серый)
    COLORREF scrollbarThumbColor = RGB(255, 0, 255);// Цвет бегунка скроллбара (пурпурный)


public:
    // Конструктор: инициализирует текстовое поле с заданным именем
    TextBox(string name) : name{ name } {}

    // Метод установки геометрии текстового поля
    void SetGeometry(RECT newRect)
    {
        rect = newRect; // Сохраняем новый прямоугольник области
        CalculateMaxVisibleLines();// Пересчитываем количество видимых строк
    }

    // Геттер для получения количества строк в тексте
    int GetLineCount() const
    {
        return linesNoPaths.size();
    }

    // Геттер для получения полного текста с путями
    vector<wstring> GetLines()
    {
        return linesWithPaths;
    }

    // Возвращает количество видимых строк (вмещающихся в область)
    int GetVisibleLines() const
    {
        return maxVisibleLines;
    }

    // Основной метод отрисовки текстового поля
    void Draw(HDC hdc)
    {
        // Проверяем, нужна ли полоса прокрутки
        HasScrollBar();

        //рамка
        HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);// Создаем перо для рамки
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH)); // Выбираем прозрачную кисть для рамки (чтобы не заливать фон)
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);// Рисуем прямоугольник рамки
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);

        //фон (внутренняя область)
         // Создаем внутренний прямоугольник с отступом в 1 пиксель от рамки
        RECT bgRect = {
            rect.left + 1,    // +1 чтобы не перекрывать рамку
            rect.top + 1,
            rect.right - 1,
            rect.bottom - 1
        };
        HBRUSH bgBrush = CreateSolidBrush(bgColor);// Создаем кисть цвета фона
        FillRect(hdc, &bgRect, bgBrush);  //Заливаем только внутреннюю область
        DeleteObject(bgBrush);


        // Область для текста (исключая скроллбар)
        RECT textRect = rect;
        if (hasScrollbar)
        {
            textRect.right -= 15; // Отступаем 15 пикселей справа под скроллбар
        }
        // Добавляем внутренние отступы для текста
        textRect.left += margin;
        textRect.top += margin;
        textRect.right -= margin;
        textRect.bottom -= margin;

        // Создаем область отсечения (clipping region) для текста
       // Это гарантирует, что текст не выйдет за пределы textRect
        HRGN clipRegion = CreateRectRgn(textRect.left, textRect.top, textRect.right, textRect.bottom);
        SelectClipRgn(hdc, clipRegion);

        // Текст
        SetBkMode(hdc, TRANSPARENT);// Устанавливаем прозрачный фон для текста
        SetTextColor(hdc, textColor);// Устанавливаем цвет текста

        HFONT hFont = CreateFont(
            16,                           // Высота 16px
            0, 0, 0,                     // Ширина, угол и т.д. по умолчанию
            FW_NORMAL,                    // Нормальное начертание
            FALSE, FALSE, FALSE,         // Не курсив, не подчеркнутый, не зачеркнутый
            DEFAULT_CHARSET,              // Поддержка всех символов
            OUT_DEFAULT_PRECIS,           // Качество вывода
            CLIP_DEFAULT_PRECIS,          // Обрезка
            DEFAULT_QUALITY,              // Качество
            DEFAULT_PITCH | FF_DONTCARE,  // Шрифт пропорциональный
            L"Arial"                      // Семейство шрифтов
        );

        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

        // Рисуем видимые строки
        int y = textRect.top;// Начальная вертикальная позиция для отрисовки текста

        // Проходим по строкам, начиная с текущей позиции прокрутки
        for (int i = scrollPos; i < linesNoPaths.size() && y < rect.bottom - margin; i++)
        {
            // Создаем прямоугольник для текущей строки
            RECT lineRect = { textRect.left, y, textRect.right, y + lineHeight };

            // Подготавливаем текст для отображения
            wstring displayText = linesNoPaths[i];

            // Проверяем, помещается ли текст в ширину текстового поля
            if (GetTextWidth(hdc, displayText) > (textRect.right - textRect.left))
            {
                // Если текст не помещается, обрезаем его и добавляем многоточие
                displayText = FitTextToWidth(hdc, displayText, textRect.right - textRect.left);
            }

            // Отрисовываем строку текста
            DrawText(hdc, displayText.c_str(), -1, &lineRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Переходим к следующей строке
            y += lineHeight;
        }

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        SelectClipRgn(hdc, NULL);
        DeleteObject(clipRegion);

        // Скроллбар если нужно
        if (hasScrollbar)
        {
            DrawScrollbar(hdc);
        }
    }

    // Метод прокрутки текста
    // delta: положительное значение - вниз, отрицательное - вверх
    void Scroll(int delta)
    {
        // Вычисляем новую позицию прокрутки
        int newPos = scrollPos + delta;

        // Ограничиваем позицию сверху (нельзя прокрутить выше первой строки)
        if (newPos < 0)
            newPos = 0;

        // Ограничиваем позицию снизу (нельзя прокрутить ниже последней видимой строки)
        if (newPos > linesNoPaths.size() - maxVisibleLines)
        {
            newPos = max(0, (int)linesNoPaths.size() - maxVisibleLines);
        }

        // Обновляем позицию прокрутки если она изменилась
        if (newPos != scrollPos)
        {
            scrollPos = newPos;
        }
    }

    // Метод прокрутки в самый низ текста
    void ScrollToBottom()
    {
        scrollPos = max(0, (int)linesNoPaths.size() - maxVisibleLines);
    }

    // Проверка, находится ли точка в области скроллбара
    bool IsPointInScrollbar(int x, int y)
    {
        if (!hasScrollbar) return false;// Если скроллбара нет, точка точно не в нем

        RECT scrollRect = GetScrollbarRect();// Получаем прямоугольник скроллбара

        // Проверяем, попадает ли точка в прямоугольник скроллбара
        return (x >= scrollRect.left && x <= scrollRect.right &&
            y >= scrollRect.top && y <= scrollRect.bottom);
    }

    // Обработка клика по скроллбару
    void HandleScrollbarClick(int y)
    {
        RECT scrollRect = GetScrollbarRect();// Область всего скроллбара
        RECT thumbRect = GetScrollbarThumbRect();// Область бегунка скроллбара

        if (y < thumbRect.top)
        {
            // Клик выше бегунка - прокрутка на одну страницу вверх
            Scroll(-maxVisibleLines);
        }
        else if (y > thumbRect.bottom)
        {
            // Клик ниже бегунка - прокрутка на одну страницу вниз
            Scroll(maxVisibleLines);
        }
        // Клик по самому бегунку обрабатывается отдельно (перетаскивание)
    }

    // Геттер для получения области текстового поля
    RECT GetBody() const
    {
        return rect;
    }

    // Методы для работы с текстом
    void AddLineToVisible(const wstring& newLine)
    {
        linesNoPaths.push_back(newLine); // Добавляем строку в отображаемый текст
    }

    void AddLineToBase(const wstring& newLine)
    {
        linesWithPaths.push_back(newLine); // Добавляем строку в полный текст
    }

    void DeleteLineFromVisible()
    {
        linesNoPaths.pop_back(); // Удаляем последнюю строку из отображаемого текста
    }

    void DeleteLineFromBase()
    {
        linesWithPaths.pop_back(); // Удаляем последнюю строку из полного текста
    }

    // Геттер и сеттер для позиции прокрутки
    int GetScrollPos() const
    {
        return scrollPos;  // Возвращает текущую позицию прокрутки
    }
    
    void SetScrollPos(int newScrollPos)
    {
        scrollPos = newScrollPos; // Устанавливает новую позицию прокрутки
    }

    // Проверка необходимости отображения скроллбара
    void HasScrollBar()
    {
        hasScrollbar = (linesNoPaths.size() > maxVisibleLines);
    }

    // Расчет максимального количества видимых строк
    void CalculateMaxVisibleLines()
    {
        // Вычисляем высоту области для текста (высота поля минус отступы)
        int contentHeight = (rect.bottom - rect.top) - (2 * margin);

        // Делим на высоту строки, получаем максимальное количество строк
        maxVisibleLines = contentHeight / lineHeight;

        // Проверяем, нужен ли скроллбар
        hasScrollbar = (linesNoPaths.size() > maxVisibleLines);
    }

    // Вспомогательный метод для получения ширины текста в пикселях
    int GetTextWidth(HDC hdc, const wstring& text)
    {
        SIZE size;
        GetTextExtentPoint32(hdc, text.c_str(), text.length(), &size);
        return size.cx;
    }

    // Метод для обрезки текста с добавлением многоточия если он не помещается
    wstring FitTextToWidth(HDC hdc, const wstring& text, int maxWidth)
    {
        wstring result = text;
        wstring ellipsis = L"...";

        // Рассчитываем ширину многоточия
        int ellipsisWidth = GetTextWidth(hdc, ellipsis);
        int availableWidth = maxWidth - ellipsisWidth;// Доступная ширина для текста

        // Находим, сколько символов помещается в доступную ширину
        int charsThatFit = 0;
        for (int i = 1; i <= text.length(); i++)
        {
            wstring substring = text.substr(0, i); // Берем первые i символов
            if (GetTextWidth(hdc, substring) <= availableWidth)
            {
                charsThatFit = i; // Запоминаем максимальное количество символов, которые помещаются
            }
            else
            {
                break;// Прерываем цикл когда текст перестает помещаться
            }
        }

        // Формируем результат
        if (charsThatFit > 0)
        {
            // Берем charsThatFit символов и добавляем многоточие
            result = text.substr(0, charsThatFit) + ellipsis;
        }
        else
        {
            // Если даже один символ не помещается, оставляем только многоточие
            result = ellipsis;
        }

        return result;
    }

    // Метод отрисовки скроллбара
    void DrawScrollbar(HDC hdc)
    {
        RECT scrollRect = GetScrollbarRect(); // Область всего скроллбара
        RECT thumbRect = GetScrollbarThumbRect();// Область бегунка

        // Фон скроллбара
        HBRUSH scrollBg = CreateSolidBrush(scrollbarColor);
        FillRect(hdc, &scrollRect, scrollBg);
        DeleteObject(scrollBg);

        // Бегунок
        HBRUSH thumbBrush = CreateSolidBrush(scrollbarThumbColor);
        FillRect(hdc, &thumbRect, thumbBrush);
        DeleteObject(thumbBrush);
    }

    // Метод получения прямоугольника бегунка скроллбара
    RECT GetScrollbarThumbRect()
    {
        RECT scrollRect = GetScrollbarRect();  // Весь скроллбар
        int scrollHeight = scrollRect.bottom - scrollRect.top;// Высота скроллбара

        // Вычисляем высоту бегунка пропорционально видимой области
        // Формула: (видимые_строки * высота_скроллбара) / общее_количество_строк
        int thumbHeight = (maxVisibleLines * scrollHeight) / linesNoPaths.size();

        // Ограничиваем минимальную высоту бегунка
        thumbHeight = max(20, thumbHeight); // Минимальный размер 20px

        // Вычисляем позицию бегунка на основе текущей позиции прокрутки
         // Формула: (scrollPos * (scrollHeight - thumbHeight)) / (всего_строк - видимых_строк)
        int thumbPos = (scrollPos * (scrollHeight - thumbHeight)) /
            (linesNoPaths.size() - maxVisibleLines);

        // Возвращаем прямоугольник бегунка
        return {
            scrollRect.left, // Левый край
            scrollRect.top + thumbPos, // Верхний край (с учетом позиции)
            scrollRect.right,// Правый край
            scrollRect.top + thumbPos + thumbHeight  // Нижний край
        };
    }

    // Метод получения прямоугольника всего скроллбара
    RECT GetScrollbarRect()
    {
        // Скроллбар располагается справа, ширина 15 пикселей
        return { rect.right - 15, rect.top, rect.right, rect.bottom };
    }
};

// Класс Text - кастомный элемент управления для отображения текста с поддержкой изменения шрифта
class Text
{
    RECT body;// Прямоугольник, ограничивающий область отрисовки текста
    wstring text;// Текст для отображения (в формате Unicode)
    string name;// Имя/идентификатор текстового элемента
    HFONT font = nullptr;// Дескриптор шрифта (nullptr - шрифт не создан)
    COLORREF textColor;// Цвет текста
    int fontHeight = 0; // Высота шрифта в логических единицах
public:
    // Флаг, указывающий на необходимость перерисовки элемента
    bool isDirty = false;

    // Конструктор: инициализирует текстовый элемент с заданным именем
    Text(const string& newName) { name = newName; }

    // Деструктор: освобождает ресурсы GDI (шрифт)
    ~Text()
    {
        DeleteObject(font);
    }

    // Метод установки высоты шрифта
    void SetFontHeight(int height)
    {
        // Сохраняем новую высоту шрифта
        fontHeight = height;

        // Если шрифт уже создан - удаляем старый
        // Важно освобождать GDI-объекты перед созданием новых
        if (font)
            DeleteObject(font);

        // Создаем новый шрифт с указанной высотой
        font = CreateFont(
            fontHeight,  
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
        );
    }

    // Метод установки геометрии текстового элемента
    // leftTop - начальная точка (левый верхний угол) текста
    // text - текст для отображения (по умолчанию пустая строка)
    void SetGeometry(POINT leftTop, wstring text = L"")
    {
        // Сохраняем текст
        this->text = text;

        // Если шрифт не создан - выходим
        if (!font) return;

        //Получаем контекст устройства экрана для расчета размеров текста
        // GetDC(NULL) получает HDC для рабочего стола
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        //Рассчитываем размеры текста
        SIZE textSize{};
        // Функция рассчитывает размеры текста в пикселях
        GetTextExtentPoint32W(
            hdc, // Контекст устройства
            text.c_str(), // Указатель на строку текста
            (int)text.length(),// Длина строки
            &textSize // Указатель на структуру для результата
        );

        SelectObject(hdc, oldFont);
        ReleaseDC(NULL, hdc);

        //Устанавливаем прямоугольник области текста
        body.left = leftTop.x;// Левый край = X начальной точки
        body.top = leftTop.y;// Верхний край = Y начальной точки
        body.right = body.left + textSize.cx; // Правый край = левый + ширина текста
        body.bottom = body.top + fontHeight;// Нижний край = верхний + высота шрифта

        // Примечание: используем fontHeight вместо textSize.cy для стабильной высоты
    }

    // Геттер для получения области текста
    RECT GetBody() const { return body; }

    // Геттер для получения текста
    wstring GetText() { return text; }

    // Метод изменения текста с автоматическим перерасчетом размеров
    void SetText(wstring text)
    {
        // Сохраняем новый текст
        this->text = text;

        // Если шрифт не создан - выходим
        if (!font) return;

        //Получаем контекст устройства для расчета размеров нового текста
        HDC hdc = GetDC(NULL);
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        //Рассчитываем размеры нового текста
        SIZE textSize{};
        GetTextExtentPoint32W(hdc, text.c_str(), (int)text.length(), &textSize);

        // Обновляем размеры прямоугольника текста
        // Левая и верхняя границы остаются неизменными
        body.right = body.left + textSize.cx;// Новая ширина
        body.bottom = body.top + textSize.cy;// Новая высота

        SelectObject(hdc, oldFont);
        ReleaseDC(NULL, hdc);
    }

    // Метод установки цвета текста
    void SetColor(COLORREF textColor)
    {
        this->textColor = textColor;
    }

    // Основной метод отрисовки текста
    void Draw(HDC hdc)
    {
        // Если шрифт не создан - нечего рисовать
        if (!font) return;

        // Выбираем наш шрифт в контекст, сохраняя старый
        HFONT oldFont = (HFONT)SelectObject(hdc, font);

        // Устанавливаем параметры отрисовки текста
        SetTextColor(hdc, textColor); // Цвет текста
        SetBkMode(hdc, TRANSPARENT);// Прозрачный фон (текст рисуется поверх существующего содержимого)

        // Отрисовываем текст в заданном прямоугольнике
        DrawTextW(hdc,// Контекст устройства
            text.c_str(),// Текст для отображения
            -1,// Длина строки (-1 означает "до нулевого символа")
            &body, // Прямоугольник для отрисовки
            DT_LEFT |// DT_LEFT - выравнивание по левому краю
            DT_TOP |// DT_TOP - выравнивание по верхнему краю
            DT_WORDBREAK// DT_WORDBREAK - перенос слов
        );

        // Восстанавливаем старый шрифт
        SelectObject(hdc, oldFont);
    }
};


ArchiveManager archiveManager;

//кнопки, текстовые метки кнопки выбора алгоритма сжатия помещены в массив для более удобной обработки их отрисовки при взаимодействии в UI
vector<Button> buttons;
vector<shared_ptr<Text>> textLabels; 
vector<shared_ptr<RadioButton>> radioButtons;

TextBox fileList{ "fileList" };// кастомный текст бокс для ввода списка файлов, которые требуется сжать
TextBox stats{ "stats" }; // кастомный текст бокс для вывода статистики
RECT mainRect;//вся область окна
HWND saveArchiveAsTB;//текст бокс для ввода пути и имени для создаваемого архива
HWND unboxArchiveToTB; // текст бокс для ввода пути, куда распаковывать архив
HWND unboxingArchiveTB; //текст бокс для ввода имени архива, который требуется распаковать
HFONT mainFont; // основной используемы шрифт - Arial
ProgressBar pg;

// Строки для хранения путей
wstring archivePath;// Путь для сохранения архива
wstring unboxArchivePath;// Путь для распаковки архива
wstring archiveName;// Имя архива для распаковки

// Переменные состояния:
CompressAlg alg; // Выбранный алгоритм сжатия (перечисление)
bool compressing = false; // Флаг, указывающий, что идет процесс сжатия
bool decompressing = false;// Флаг, указывающий, что идет процесс распаковки

int cnt = 0;


//функция для проверки нахождения курсора в данном прямоугольнике
bool IsInRECT(RECT r, POINT mousePos)
{
    if ((mousePos.x >= r.left && mousePos.y >= r.top) && (mousePos.x <= r.right && mousePos.y <= r.bottom))
        return true;
    else
        return false;
}

//основная функция обратного вызова для обработки сообщений системы
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hEdit;
    static HFONT hFont;

    switch (msg)
    {
#pragma region логика изменения размеров окна
    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);

        int borderWidth = 4;

        if (pt.x < borderWidth)
        {
            if (pt.y < borderWidth)
                return HTTOPLEFT;
            else if (pt.y > rcClient.bottom - borderWidth)
                return HTBOTTOMLEFT;
            else
                return HTLEFT;
        }
        else if (pt.x > rcClient.right - borderWidth)
        {
            if (pt.y < borderWidth)
                return HTTOPRIGHT;
            else if (pt.y > rcClient.bottom - borderWidth)
                return HTBOTTOMRIGHT;
            else
                return HTRIGHT;
        }
        else if (pt.y < borderWidth)
        {
            return HTTOP;
        }
        else if (pt.y > rcClient.bottom - borderWidth)
        {
            return HTBOTTOM;
        }

        return HTCLIENT;
    }
#pragma endregion 

#pragma region Создание всех контроллов в WM_CREATE
    case WM_CREATE:
    {
        // Получаем размеры клиентской области окна для правильного размещения элементов
        GetClientRect(hWnd, &mainRect);

        // Создание кнопки закрытия окна
        Button closeBtn{ "close" };
        closeBtn.SetGeometry({ mainRect.right - 25, mainRect.top + 5 }, { mainRect.right - 5, mainRect.top + 25 }, L"", 2);
        closeBtn.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(closeBtn);

        // Создание кнопки сворачивания окна
        Button collapseBtn{ "collapse" };
        collapseBtn.SetGeometry({ mainRect.right - 48, mainRect.top + 5 }, { mainRect.right - 28, mainRect.top + 25 }, L"", 2);
        collapseBtn.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(collapseBtn);

        // Настройка кастомного текстового поля для списка файлов
        // Располагается слева, занимает область от 4 до 270 по X и от 54 до 300 по Y
        fileList.SetGeometry({ 4, 54, 270, 300 });

        // Настройка кастомного текстового поля для статистики
        // Располагается справа от списка файлов, до правого края окна
        stats.SetGeometry({ 274, 54, mainRect.right - 4, 300 });

        // Получаем область статистики для правильного размещения прогресс-бара под ней
        RECT r = stats.GetBody();

        // Настройка прогресс-бара: располагается сразу под областью статистики
        // Высота прогресс-бара: 20 пикселей (от r.bottom+2 до r.bottom+22)
        pg.SetGeometry({ r.left, r.bottom + 2, r.right, r.bottom + 22 });

        // Создание кнопки "Выбрать файл" для добавления файлов в список
        Button addBtn{ "add" };
        addBtn.SetGeometry({ 4, 302 }, { 100, 332 }, L"Выбрать файл", 1);
        addBtn.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(addBtn);

        // Создание кнопки "Создать архив" для запуска процесса сжатия
        Button createBtn{ "create" };
        createBtn.SetGeometry({ 102, 302 }, { 198, 332 }, L"Создать архив", 1);
        createBtn.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(createBtn);

        // Создание текстовой метки "Сохранить архив как:" с помощью умного указателя
        auto saveLabel = make_shared<Text>("saveArchiveAs");
        saveLabel->SetFontHeight(15);
        saveLabel->SetGeometry({ 4, 336 }, L"Сохранить архив как:");
        saveLabel->SetColor(RGB(255, 255, 255));
        textLabels.push_back(saveLabel);

        // Создание текстовой метки "Распаковать архив в:"
        auto unboxLabel = make_shared<Text>("unboxArchiveAs");
        unboxLabel->SetFontHeight(15);
        unboxLabel->SetGeometry({ 4, 380 }, L"Распаковать архив в:");
        unboxLabel->SetColor(RGB(255, 255, 255));
        textLabels.push_back(unboxLabel);

        // Создание радиокнопок для выбора алгоритма сжатия:
        auto lz77RB = make_shared<RadioButton>("lz77");
        lz77RB->SetGeometry({ 310, 355 }, 15, L"LZ77");
        radioButtons.push_back(lz77RB);

        auto lz78RB = make_shared<RadioButton>("lz78");
        lz78RB->SetGeometry({ 310, lz77RB->GetBody().bottom + 4}, 15, L"LZ78");
        radioButtons.push_back(lz78RB);

        auto stHuff = make_shared<RadioButton>("stHuff");
        stHuff->SetGeometry({ 310, lz78RB->GetBody().bottom + 4 }, 15, L"Хаффман (статический)");
        stHuff->SetState(true);
        radioButtons.push_back(stHuff);

        auto adHuff = make_shared<RadioButton>("adHuff");
        adHuff->SetGeometry({ 310, stHuff->GetBody().bottom + 4 }, 15, L"Хаффман (адаптивный)");
        radioButtons.push_back(adHuff);

        // Создание стандартного текстового поля Windows для ввода пути сохранения архива
        saveArchiveAsTB = CreateWindow(
            TEXT("EDIT"), // Класс окна - поле ввода
            TEXT(""),// Начальный текст - пустой
            WS_CHILD | WS_VISIBLE | WS_BORDER | // Стили: дочернее, видимое, с рамкой
            ES_LEFT | ES_AUTOHSCROLL, // Выравнивание по левому краю, автоскролл по горизонтали
            4, 355,// Позиция X,Y (под меткой)
            230, 23,// Ширина и высота
            hWnd,// Родительское окно
            (HMENU)1, // Идентификатор контрола (ID)
            ((LPCREATESTRUCT)lParam)->hInstance,// Дескриптор экземпляра приложения
            NULL// Дополнительные параметры
        );

        // Создание основного шрифта Arial для всего интерфейса
        mainFont = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            TEXT("Arial")
        );

        // Применяем созданный шрифт к текстовому полю
        SendMessage(saveArchiveAsTB, WM_SETFONT, (WPARAM)mainFont, TRUE);

        // Кнопка "..." для выбора пути сохранения архива через диалог выбора папки
        Button selectArchivePath{ "selectArchivePath" };
        selectArchivePath.SetGeometry({ 236, 355 }, { 259, 378 }, L"...", 1);
        selectArchivePath.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(selectArchivePath);

        // Кнопка "..." для выбора пути распаковки архива
        Button selectUnboxArchivePath{ "selectWhereToUnbox" };
        selectUnboxArchivePath.SetGeometry({ 236, 397 }, { 259, 420 }, L"...", 1);
        selectUnboxArchivePath.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(selectUnboxArchivePath);

        // Текстовое поле для ввода пути распаковки архива
        unboxArchiveToTB = CreateWindow(
            TEXT("EDIT"),
            TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
            4, 397,
            230, 23,
            hWnd,
            (HMENU)1,
            ((LPCREATESTRUCT)lParam)->hInstance,
            NULL
        );

        // Применяем шрифт к новому текстовому полю
        SendMessage(unboxArchiveToTB, WM_SETFONT, (WPARAM)mainFont, TRUE);
       
        // Метка "Архив для распаковки:"
        auto archiveNameLabel = make_shared<Text>("unboxArchiveAs");
        archiveNameLabel->SetFontHeight(15);
        archiveNameLabel->SetGeometry({ 4, 422 }, L"Архив для распаковки:");
        archiveNameLabel->SetColor(RGB(255, 255, 255));
        textLabels.push_back(archiveNameLabel);

        // Текстовое поле для ввода имени архива для распаковки
        unboxingArchiveTB = CreateWindow(
            TEXT("EDIT"),
            TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
            4, 441,
            230, 23,
            hWnd,
            (HMENU)1,
            ((LPCREATESTRUCT)lParam)->hInstance,
            NULL
        );

        // Применяем шрифт
        SendMessage(unboxingArchiveTB, WM_SETFONT, (WPARAM)mainFont, TRUE);

        // Кнопка "..." для выбора архива для распаковки
        Button selectArchive{ "selectUnbocingArchive" };
        selectArchive.SetGeometry({ 236, 441 }, { 259, 464 }, L"...", 1);
        selectArchive.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(selectArchive);

        // Кнопка "Распаковать" для запуска процесса распаковки
        Button unbox{ "unbox" };
        unbox.SetGeometry({ 4, 466 }, { 100, 496 }, L"Распаковать", 1);
        unbox.SetColor(RGB(190, 190, 190), RGB(255, 255, 255), RGB(150, 150, 150));
        buttons.push_back(unbox);

        return 0;
    }
#pragma endregion

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        if (hFont)
        {
            DeleteObject(hFont);
            hFont = NULL;
        }
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:

        RECT r;

        switch (wParam)
        {
        case VK_DELETE: // При нажатии кнопки delete удаляется 1 файл из списка файлов
            if (fileList.GetLineCount() == 0)
                break;

            fileList.DeleteLineFromVisible();
            fileList.DeleteLineFromBase();

            r = fileList.GetBody();
            InvalidateRect(hWnd, &r, TRUE);

            break;
   
        }
        return 0;
    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam); // +120 (вниз) или -120 (вверх)
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };

        // Конвертируем координаты в клиентские
        ScreenToClient(hWnd, &pt);

        RECT r1 = fileList.GetBody();
        RECT r2 = stats.GetBody();

        if (PtInRect(&r1, pt))
        {
            // Прокручиваем на 3 строки за "щелчок" колесика
            fileList.Scroll(-delta / 40);
            InvalidateRect(hWnd, &r1, TRUE);
            break;
        }
        else if (PtInRect(&r2, pt))
        {
            stats.Scroll(-delta / 40);
            InvalidateRect(hWnd, &r2, TRUE);
            break;
        }

        
        return 0;
    }
    case WM_SIZE:
    {
        if (wParam == SIZE_RESTORED) // Окно восстановлено из свернутого состояния
        {
            // Получаем текущие координаты курсора в экранных координатах
            POINT pt;
            GetCursorPos(&pt);

            // Переводим в клиентские координаты
            ScreenToClient(hWnd, &pt);

            // Принудительно "вбросим" WM_MOUSEMOVE
            LPARAM lParamFake = MAKELPARAM(pt.x, pt.y);
            SendMessage(hWnd, WM_MOUSEMOVE, 0, lParamFake);
        }
        InvalidateRect(hWnd, NULL, TRUE);


        return 0;
    }

    // Обработчик сообщения WM_MOUSEMOVE - отслеживание движения мыши
    // Этот обработчик реагирует только на наведение курсора на кнопки интерфейса
    case WM_MOUSEMOVE: 
    {
        // Извлекаем координаты курсора мыши из параметра lParam
        // LOWORD(lParam) - X-координата (горизонтальное положение)
        // HIWORD(lParam) - Y-координата (вертикальное положение)
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // Проходим по всем кнопкам в интерфейсе для проверки наведения
        for (int i = 0; i < buttons.size(); i++)
        {
            // Получаем прямоугольник области текущей кнопки
            RECT r = buttons[i].GetBodyInfo();

            // Сохраняем предыдущее состояние "наведения" кнопки
           // (нужно для определения изменения состояния)
            buttons[i].preIsTargeted = buttons[i].isTargeted;

            // Проверяем, находится ли курсор в пределах прямоугольника кнопки
            // Условие: X между left и right, И Y между top и bottom
            buttons[i].isTargeted = (x >= r.left && y >= r.top) && (x <= r.right && y <= r.bottom);

            // Проверяем, изменилось ли состояние наведения на кнопке
           // (курсор либо вошел в область кнопки, либо вышел из нее)
            if (buttons[i].isTargeted != buttons[i].preIsTargeted)
            {
                // Определяем новое состояние кнопки:
                // - Hovered (наведение) - если курсор над кнопкой
                // - Normal (обычное) - если курсор ушел с кнопки
                Button::ButtonState newState = buttons[i].isTargeted ?
                    Button::ButtonState::Hovered : Button::ButtonState::Normal;

                // Устанавливаем новое состояние кнопки
                // (меняет ее визуальное отображение - цвет, рамку и т.д.)
                buttons[i].SetState(newState);

                // Запрашиваем перерисовку области кнопки
                // InvalidateRect помечает указанный прямоугольник как требующий перерисовки
                // true (третий параметр) - означает, что фон должен быть стерт перед перерисовкой
                InvalidateRect(hWnd, &r, true);

                // Примечание: вызов InvalidateRect не выполняет немедленную перерисовку,
                // а лишь помечает область для перерисовки. Фактическая перерисовка 
                // произойдет при обработке сообщения WM_PAINT
            }
        }

        return 0;
    }

    // Обработчик сообщения WM_LBUTTONDOWN - нажатие левой кнопки мыши
    // Основная логика архивации будет обрабатываться в WM_LBUTTONUP
    // Здесь только обработка UI-элементов
    case WM_LBUTTONDOWN:
    {
        // Извлекаем координаты клика мыши
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // Определяем зону для перетаскивания окна (верхняя панель)
        RECT mainRect;
        GetClientRect(hWnd, &mainRect); // Получаем размеры клиентской области окна

        // Создаем прямоугольник зоны перетаскивания:
        // - Отступ 2px слева и сверху
        // - Ширина: до 50px от правого края (оставляем место для кнопок свернуть/закрыть)
        // - Высота: 30px
        RECT dragZone{ 2, 2, mainRect.right - 50, 30 };

        // Проверяем, был ли клик в зоне перетаскивания окна
        if (PtInRect(&dragZone, { x, y }))
        {
            // Если да - имитируем клик по заголовку окна для перетаскивания
            ReleaseCapture(); // Освобождаем захват мыши (если был)

            // Отправляем сообщение WM_NCLBUTTONDOWN (клик по неклиентской области)
            // HTCAPTION - указывает, что клик был по заголовку окна
            // Это позволяет системе обработать перетаскивание окна
            SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // Обработка кликов по кнопкам
        for (int i = 0; i < buttons.size(); i++)
        {
            if (buttons[i].isTargeted)
            {
                RECT r = buttons[i].GetBodyInfo();
                buttons[i].SetState(Button::ButtonState::Pressed);
                InvalidateRect(hWnd, &r, true);
            }
        }

        // Обработка кликов по радиокнопкам выбора алгоритма сжатия
        for (int i = 0; i < radioButtons.size(); i++)
        {
            // Получаем квадратную область радиокнопки (где находится сам квадрат)
            RECT r = radioButtons[i]->GetSquareRECT();

            // Проверяем, был ли клик внутри квадрата радиокнопки
            if (IsInRECT(r, { x, y }))
            {
                // Если эта кнопка уже активна - ничего не делаем
                if (radioButtons[i]->GetState())
                {
                    break; 
                }

                // Деактивируем ВСЕ кнопки
                for (int j = 0; j < radioButtons.size(); j++)
                {
                    radioButtons[j]->SetState(false);
                    RECT b = radioButtons[j]->GetBody();
                    InvalidateRect(hWnd, &b, true); // Перерисовать каждую кнопку
                }

                // Активируем только кликнутую
                radioButtons[i]->SetState(true);
                RECT b = radioButtons[i]->GetBody();
                InvalidateRect(hWnd, &b, true);

                if (radioButtons[i]->name == "stHuff")
                    alg = CompressAlg::StaticHuffman;
                else if (radioButtons[i]->name == "adHuff")
                    alg = CompressAlg::AdaptiveHuffman;
                else if (radioButtons[i]->name == "lz77")
                    alg = CompressAlg::LZ77;
                else if (radioButtons[i]->name == "lz78")
                    alg = CompressAlg::LZ78;


                break; // Выходим после обработки клика
            }
        }
      
        return 0;
    }
#pragma region Обработка нажатий на кнопки (при отпускании ЛКМ)

    // Здесь выполняется основная логика архиватора при кликах по кнопкам
    case WM_LBUTTONUP:
    {
        // Перебираем все кнопки интерфейса
        for (int i = 0; i < buttons.size(); i++)
        {
            // Проверяем, была ли кнопка нажата (курсор над ней при отпускании)
            if (buttons[i].isTargeted)
            {
                RECT r = buttons[i].GetBodyInfo();

                // Меняем состояние кнопки с "нажата" на "наведение"
                // Это дает визуальную обратную связь пользователю
                buttons[i].SetState(Button::ButtonState::Hovered);
                InvalidateRect(hWnd, &r, true);

                // Выполняем действие в зависимости от имени кнопки
                // (имя было задано при создании кнопки в WM_CREATE)

                if (buttons[i].name == "collapse") // Кнопка "Свернуть окно"
                {
                    ShowWindow(hWnd, SW_MINIMIZE);
                }
                else if (buttons[i].name == "close") // Кнопка "Закрыть окно"
                {
                    SendMessage(hWnd, WM_CLOSE, 0, 0);
                }
                else if (buttons[i].name == "add") // Кнопка "Выбрать файл" для добавления в список архивации
                {
                    // Открываем диалог выбора файла
                    wstring fullPath = ShowOpenFileDialog(hWnd);

                    // Если пользователь отменил выбор - выходим
                    if (fullPath.empty())
                        break;

                    // Добавляем файл в список:
                    // 1. В видимую часть - только имя файла (без пути)
                    fileList.AddLineToVisible(GetNameFromPath(fullPath));
                    // 2. В базу данных - полный путь
                    fileList.AddLineToBase(fullPath);

                    // Перерисовываем список файлов
                    RECT r = fileList.GetBody();
                    InvalidateRect(hWnd, &r, true);
                }
                else if (buttons[i].name == "selectArchivePath") // Кнопка "..." для выбора пути сохранения архива
                {
                    // Открываем диалог выбора папки
                    archivePath = ShowFolderDialog(hWnd);
                    // Вставляем выбранный путь в текстовое поле
                    SendMessage(saveArchiveAsTB, WM_SETTEXT, 0, (LPARAM)archivePath.c_str());
                }
                else if (buttons[i].name == "selectWhereToUnbox") // Кнопка "..." для выбора папки распаковки
                {
                    // Открываем диалог выбора папки
                    unboxArchivePath = ShowFolderDialog(hWnd);

                    // Вставляем путь в соответствующее текстовое поле
                    SendMessage(unboxArchiveToTB, WM_SETTEXT, 0, (LPARAM)unboxArchivePath.c_str());
                }
                else if (buttons[i].name == "create") // Кнопка "Создать архив" - запуск процесса сжатия
                {
                    // Получаем список выбранных файлов (полные пути)
                    vector<wstring> namesWS = fileList.GetLines();

                    // Проверка 1: список файлов не должен быть пустым
                    if (namesWS.empty())
                    {
                        MessageBox(NULL, L"Добавьте хотя бы один файл в архив",  L"Список файлов пуст", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    // Проверка 2: проверка на уникальность имен файлов
                    // (в архиве не должно быть файлов с одинаковыми именами)
                    set<wstring> s{ namesWS.begin(), namesWS.end() };
                    
                    if (s.size() < namesWS.size())
                    {
                        MessageBox(NULL, L"Уберите повторяющиеся имена файлов", L"Внимание", MB_OK | MB_ICONWARNING);
                        break;
                    }

                    // Конвертируем wstring в string для работы с кодом сжатия
                    vector<string> namesS;

                    for (int i = 0; i < namesWS.size(); i++)
                    {
                        namesS.push_back(WstringToString(namesWS[i]));
                    }

                    // Получаем путь для сохранения архива из текстового поля
                    string archivePathS = WstringToString(GetTextBoxTextW(saveArchiveAsTB));

                    // Проверка 3: проверка расширения архива
                    if (!IsValidArchiveExtansion(archivePathS))
                    {
                        MessageBox(NULL, L"Некоррекрное имя архива", L"Ошибка", MB_OK | MB_ICONERROR);
                        break;
                    }

                    // Запускаем асинхронное создание архива
                    // Использует выбранный алгоритм сжатия (alg)
                    archiveManager.StartArchiveCreatingAsync(namesS, archivePathS, alg);

                    // Активируем прогресс-бар
                    pg.SetState(true);
                    compressing = true;

                    // Запускаем таймер для обновления прогресса (каждые 100 мс)
                    SetTimer(hWnd, 1, 100, NULL);
                }
                else if (buttons[i].name == "selectUnbocingArchive")// Кнопка "..." для выбора архива для распаковки
                {
                    // Открываем диалог выбора файла архива
                    wstring fullPath = ShowOpenFileDialog(hWnd);

                    // Вставляем путь в текстовое поле
                    SendMessage(unboxingArchiveTB, WM_SETTEXT, 0, (LPARAM)fullPath.c_str());
                }
                else if (buttons[i].name == "unbox") // Кнопка "Распаковать" - запуск процесса распаковки
                {
                    // Получаем путь к архиву и папку распаковки из текстовых полей
                    string unboxingArchive = WstringToString(GetTextBoxTextW(unboxingArchiveTB));
                    string unboxTo = WstringToString(GetTextBoxTextW(unboxArchiveToTB));

                    // Проверка 1: корректность расширения архива
                    if (!IsValidArchiveExtansion(unboxingArchive))
                    {
                        MessageBox(NULL, L"Некоррекрное имя архива", L"Ошибка", MB_OK | MB_ICONERROR);
                        break;
                    }

                    // Проверка 2: путь распаковки не должен быть пустым
                    if (unboxTo.empty())
                    {
                        MessageBox(NULL, L"Введите путь, куда распаковывать", L"Ошибка", MB_OK | MB_ICONWARNING);
                        break;
                    }

                    // Проверка 3: добавляем обратный слэш в конец пути, если его нет
                    char lastChar = unboxTo.back();
                    if (lastChar != '\\')
                        unboxTo += '\\';

                    // Запускаем асинхронную распаковку архива
                    archiveManager.StartArchiveUnboxingAsync(unboxingArchive, unboxTo);

                    // Активируем прогресс-бар
                    pg.SetState(true);
                    decompressing = true;

                    // Запускаем таймер для обновления прогресса
                    SetTimer(hWnd, 1, 100, NULL);
                }
            }
        }
        return 0;
    }
#pragma endregion 

#pragma region логика таймера для обработки прогресс-бара
    // Обработчик сообщения WM_TIMER - вызывается периодически по таймеру
    case WM_TIMER:
    {
        // Таймер 1: обновление прогресс-бара во время работы архиватора
        if (wParam == 1)
        {
            // Обновляем состояние менеджера архива
            archiveManager.Update();

            // Проверяем, завершена ли работа архиватора
            if (!archiveManager.isWorking.load())
            {
                // Если работа завершена - устанавливаем прогресс в 100%
                pg.SetProgress(100);
                RECT r = pg.GetBody();
                InvalidateRect(hWnd, &r, TRUE);

                // Запускаем второй таймер для отображения статистики
                SetTimer(hWnd, 2, 200, NULL);

                // Останавливаем первый таймер
                KillTimer(hWnd, 1);
            }
            else
            {
                // Если работа продолжается - обновляем прогресс
                pg.SetProgress(archiveManager.GetNewProgress());
                RECT r = pg.GetBody();
                InvalidateRect(hWnd, &r, TRUE);
            }
        }
        // Таймер 2: отображение статистики после завершения работы
        else if (wParam == 2)
        {
            // Сбрасываем прогресс - бар
            pg.Reset();
            pg.SetState(false);

            if (compressing)
            {
                // Получаем статистику по сжатию
                Statistics st = archiveManager.GetStatistics();

                // Получаем список файлов
                vector<wstring> namesWS = fileList.GetLines();

                // Добавляем заголовок в статистику
                stats.AddLineToVisible(L"СОЗДАНИЕ АРХИВА");
                stats.AddLineToBase(L"СОЗДАНИЕ АРХИВА");

                // Для каждого файла выводим детальную статистику
                for (int i = 0; i < st.sizes.size(); i++)
                {
                    // Имя файла
                    stats.AddLineToVisible(GetNameFromPath(namesWS[i]));
                    stats.AddLineToBase(GetNameFromPath(namesWS[i]));

                    // Размеры файла
                    uint64_t originalSize = st.sizes[i].first;
                    uint64_t compressedSize = st.sizes[i].second;

                    // Рассчитываем коэффициент сжатия
                    double ratio = static_cast<double>(originalSize) / compressedSize;
                    wstring ratio_str = DoubleToWstring(ratio, 2);// 2 знака после запятой

                    // Время сжатия
                    wstring time = DoubleToWstring(st.timeElapsed[i], 3);

                    // Исходный размер
                    stats.AddLineToVisible(L"исходный размер: " + to_wstring(st.sizes[i].first) + L" байт");
                    stats.AddLineToBase(L"исходный размер: " + to_wstring(st.sizes[i].first) + L" байт");

                    // Итоговый размер
                    stats.AddLineToVisible(L"итоговый размер: " + to_wstring(st.sizes[i].second) + L" байт");
                    stats.AddLineToBase(L"итоговый размер: " + to_wstring(st.sizes[i].second) + L" байт");

                    // Коэффициент сжатия
                    stats.AddLineToVisible(L"коэффициент сжатия: " + ratio_str);
                    stats.AddLineToBase(L"коэффициент сжатия: " + ratio_str);

                    // Время сжатия
                    stats.AddLineToVisible(L"время сжатия: " + time + L"c");
                    stats.AddLineToBase(L"время сжатия: " + time + L"c");

                    // Разделитель между файлами
                    stats.AddLineToVisible(L"----------------------------------------------------");
                    stats.AddLineToBase(L"----------------------------------------------------");
                }

                // Сбрасываем состояние менеджера архива и прогресс-бара
                archiveManager.Reset();
                pg.Reset();

                // Подготавливаем область для перерисовки (статистика + прогресс-бар)
                RECT r1 = stats.GetBody();
                RECT r2 = pg.GetBody();
                RECT unionRECT;
                UnionRect(&unionRECT, &r1, &r2);

                // Перерисовываем область со статистикой
                InvalidateRect(hWnd, &unionRECT, TRUE);

                // Сбрасываем флаг сжатия
                compressing = false;
            }
            else if (decompressing)
            {
                // Добавляем заголовок для распаковки
                stats.AddLineToVisible(L"РАСПАКОВКА АРХИВА");
                stats.AddLineToBase(L"РАСПАКОВКА АРХИВА");

                // Получаем время распаковки
                double time = archiveManager.GetDecompressingTime();
                wstring time_str = DoubleToWstring(time, 3);// 3 знака после запятой

                // Выводим время распаковки
                stats.AddLineToVisible(L"время распаковки: " + time_str +  L" с");
                stats.AddLineToBase(L"время распаковки: " + time_str + L" с");

                // Сбрасываем состояние
                archiveManager.Reset();
                pg.Reset();

                // Подготавливаем и перерисовываем область
                RECT r1 = stats.GetBody();
                RECT r2 = pg.GetBody();
                RECT unionRECT;
                UnionRect(&unionRECT, &r1, &r2);

                InvalidateRect(hWnd, &unionRECT, TRUE);

                // Сбрасываем флаг распаковки
                decompressing = false;
            }

            // Останавливаем второй таймер
            KillTimer(hWnd, 2);
        }
        return 0;
    }
#pragma endregion 

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        GetClientRect(hWnd, &mainRect);

        // черный фон окна
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &mainRect, black);
        DeleteObject(black);
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));

        //рамки окна
        RECT top{ 0, 0, mainRect.right, mainRect.top + 2 };
        FillRect(hdc, &top, whiteBrush);

        RECT bottom{ 0,mainRect.bottom - 2, mainRect.right, mainRect.bottom };
        FillRect(hdc, &bottom, whiteBrush);

        RECT left{ 0, 0,  2, mainRect.bottom };
        FillRect(hdc, &left, whiteBrush);

        RECT right{ mainRect.right - 2, 0,   mainRect.right, mainRect.bottom };
        FillRect(hdc, &right, whiteBrush);

        // зона, в которой курсор может делать драггинг окна
        RECT dragZone{ 2, 2, mainRect.right - 51, 30 };
        FillRect(hdc, &dragZone, whiteBrush);

        // полоса под кнопками управления
        RECT underControleBtn{ dragZone.right, dragZone.bottom - 2, mainRect.right - 2, dragZone.bottom };
        FillRect(hdc, &underControleBtn, whiteBrush);

        DeleteObject(whiteBrush);

        //Рисуем кнопки управления
        buttons[0].SetGeometry({ mainRect.right - 25, mainRect.top + 5 }, { mainRect.right - 5, mainRect.top + 25 }, L"", 2);
        buttons[0].Draw(hdc);

        buttons[1].SetGeometry({ mainRect.right - 48, mainRect.top + 5 }, { mainRect.right - 28, mainRect.top + 25 }, L"", 2);
        buttons[1].Draw(hdc);

        //обрабатываем отрисовку остальных кнопок, только если они были помечены, как требующие перерисовки
        for (int i = 0; i < buttons.size(); i++)
        {
            RECT buttonRect = buttons[i].GetBodyInfo();
            RECT repaintRect = ps.rcPaint;
            RECT intersection;

            if (IntersectRect(&intersection, &repaintRect, &buttonRect))
            {
                buttons[i].Draw(hdc);
            }
        }

        //рисуем текстбоксы списка файлов и статистики
        fileList.Draw(hdc);
        stats.Draw(hdc);

        //обрабатываем отрисовку текстовых меток, только если они были помечены, как требующие перерисовки
        for (int i = 0; i < textLabels.size(); i++)
        {
            RECT labelRect = textLabels[i]->GetBody();
            RECT repaintRect = ps.rcPaint;
            RECT intersection;

            if (IntersectRect(&intersection, &repaintRect, &labelRect))
            {
                textLabels[i]->Draw(hdc);
            }
        }

        //обрабатываем отрисовку радиокнопок, только если они были помечены, как требующие перерисовки
        for (int i = 0; i < radioButtons.size(); i++)
        {
            RECT radioButtonRECT = radioButtons[i]->GetBody();
            RECT repaintRect = ps.rcPaint;
            RECT intersection;

            if (IntersectRect(&intersection, &repaintRect, &radioButtonRECT))
            {
                radioButtons[i]->Draw(hdc);
            }
        }
      
        //рисуем прогресс-бар
        RECT pgRect = pg.GetBody();
        RECT repaintRect = ps.rcPaint;
        RECT intersection;

        if (IntersectRect(&intersection, &repaintRect, &pgRect))
        {
           pg.Draw(hdc);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyCustomClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        0, L"MyCustomClass", NULL,
        WS_POPUP,
        500, 500, 600, 500,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}