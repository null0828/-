// GameCenter.h
#pragma once
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <cmath>
#include <string>
#include <fstream>
#include <memory>
#include <map>
#include <shlobj.h>
#include <objbase.h>
#include <algorithm>

// 在桌面创建快捷方式，便于直接打开程序
static bool CreateDesktopShortcut(const wchar_t* shortcutName) {
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return false;

    PWSTR desktopPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktopPath))) return false;

    wchar_t linkPath[MAX_PATH];
    _snwprintf_s(linkPath, MAX_PATH, _TRUNCATE, L"%s\\%s.lnk", desktopPath, shortcutName);

    bool result = false;
    HRESULT hr = CoInitialize(nullptr);
    if (SUCCEEDED(hr)) {
        IShellLinkW* psl = nullptr;
        hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
        if (SUCCEEDED(hr) && psl) {
            psl->SetPath(exePath);
            // 设置工作目录为exe所在目录
            wchar_t exeDir[MAX_PATH];
            wcscpy_s(exeDir, exePath);
            wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
            if (lastSlash) *lastSlash = L'\0';
            psl->SetWorkingDirectory(exeDir);
            psl->SetDescription(L"GameCenter 快捷方式");

            IPersistFile* ppf = nullptr;
            hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
            if (SUCCEEDED(hr) && ppf) {
                hr = ppf->Save(linkPath, TRUE);
                if (SUCCEEDED(hr)) result = true;
                ppf->Release();
            }
            psl->Release();
        }
        CoUninitialize();
    }

    CoTaskMemFree(desktopPath);
    return result;
}

// 游戏类型枚举
enum GameType {
    GAME_SOKOBAN,
    GAME_SNAKE,
    // GAME_BRICK removed
};

// 分数记录类
class ScoreManager {
private:
    std::map<GameType, int> highScores;
    std::string scoreFile;

    void LoadScores() {
        std::ifstream file(scoreFile);
        if (file.is_open()) {
            int score;
            for (int i = 0; i < 2; i++) {
                file >> score;
                highScores[(GameType)i] = score;
            }
            file.close();
        }
    }

public:
    ScoreManager() : scoreFile("highscores.dat") {
        LoadScores();
    }

    ~ScoreManager() {
        SaveScores();
    }

    void SaveScores() {
        std::ofstream file(scoreFile);
        if (file.is_open()) {
            for (int i = 0; i < 2; i++) {
                file << highScores[(GameType)i] << std::endl;
            }
            file.close();
        }
    }

    int GetHighScore(GameType game) {
        return highScores[game];
    }

    bool UpdateHighScore(GameType game, int score) {
        if (score > highScores[game]) {
            highScores[game] = score;
            SaveScores();
            return true;
        }
        return false;
    }
};

// 推箱子游戏类
class SokobanGame {
private:
    static const int WIDTH = 10;
    static const int HEIGHT = 10;
    int map[HEIGHT][WIDTH];
    int playerX, playerY;
    int currentLevel;
    int moves;
    bool gameComplete;

    // 0=空地, 1=墙, 2=目标点, 3=箱子, 4=玩家, 5=玩家在目标点, 6=箱子在目标点

public:
    SokobanGame() : currentLevel(1), moves(0), gameComplete(false) {
        LoadLevel(currentLevel);
    }

    void LoadLevel(int level) {
        // 随机生成关卡：四周为墙，内部放置若干目标点和箱子，玩家放在空白处
        // 清空地图并画墙
        for (int i = 0; i < HEIGHT; i++) {
            for (int j = 0; j < WIDTH; j++) {
                if (i == 0 || i == HEIGHT - 1 || j == 0 || j == WIDTH - 1) {
                    map[i][j] = 1; // 墙
                }
                else {
                    map[i][j] = 0; // 空地
                }
            }
        }

        // 随机放置目标点和箱子，数量根据地图大小简单决定
        int boxCount = 3; // 可以调整难度

        // 放置目标点
        int placedTargets = 0;
        while (placedTargets < boxCount) {
            // 避免目标紧贴墙壁，范围在 [2, WIDTH-3]
            int tx = rand() % (WIDTH - 4) + 2;
            int ty = rand() % (HEIGHT - 4) + 2;
            if (map[ty][tx] == 0) {
                map[ty][tx] = 2; // 目标点
                placedTargets++;
            }
        }

        // 放置箱子（不要放到目标上）
        int placedBoxes = 0;
        while (placedBoxes < boxCount) {
            // 避免箱子贴近墙壁，范围在 [2, WIDTH-3]
            int bx = rand() % (WIDTH - 4) + 2;
            int by = rand() % (HEIGHT - 4) + 2;
            if (map[by][bx] == 0) {
                map[by][bx] = 3; // 箱子
                placedBoxes++;
            }
        }

        // 放置玩家在空白处
        while (true) {
            int px = rand() % (WIDTH - 2) + 1;
            int py = rand() % (HEIGHT - 2) + 1;
            if (map[py][px] == 0) {
                map[py][px] = 4; // 玩家
                playerX = px;
                playerY = py;
                break;
            }
        }
    }

    bool Move(int dx, int dy) {
        if (gameComplete) return false;

        int newX = playerX + dx;
        int newY = playerY + dy;
        int newX2 = newX + dx;
        int newY2 = newY + dy;

        // 检查是否碰到墙
        if (map[newY][newX] == 1) return false;

        // 推箱子
        if (map[newY][newX] == 3 || map[newY][newX] == 6) {
            if (map[newY2][newX2] == 0 || map[newY2][newX2] == 2) {
                // 移动箱子
                if (map[newY2][newX2] == 2) {
                    map[newY2][newX2] = 6;
                }
                else {
                    map[newY2][newX2] = 3;
                }

                // 移动玩家
                if (map[newY][newX] == 6) {
                    map[newY][newX] = 2;
                }
                else {
                    map[newY][newX] = 0;
                }

                if (map[playerY][playerX] == 5) {
                    map[playerY][playerX] = 2;
                }
                else {
                    map[playerY][playerX] = 0;
                }

                playerX = newX;
                playerY = newY;

                if (map[playerY][playerX] == 2) {
                    map[playerY][playerX] = 5;
                }
                else {
                    map[playerY][playerX] = 4;
                }

                moves++;
                CheckCompletion();
                return true;
            }
            return false;
        }

        // 普通移动
        if (map[newY][newX] == 0 || map[newY][newX] == 2) {
            if (map[playerY][playerX] == 5) {
                map[playerY][playerX] = 2;
            }
            else {
                map[playerY][playerX] = 0;
            }

            playerX = newX;
            playerY = newY;

            if (map[playerY][playerX] == 2) {
                map[playerY][playerX] = 5;
            }
            else {
                map[playerY][playerX] = 4;
            }

            moves++;
            return true;
        }

        return false;
    }

    void CheckCompletion() {
        bool allBoxesOnTarget = true;
        for (int i = 0; i < HEIGHT; i++) {
            for (int j = 0; j < WIDTH; j++) {
                if (map[i][j] == 3) {
                    allBoxesOnTarget = false;
                }
            }
        }
        gameComplete = allBoxesOnTarget;
    }

    bool IsGameComplete() { return gameComplete; }
    int GetMoves() { return moves; }
    int GetMap(int x, int y) { return map[y][x]; }
    void Reset() { LoadLevel(currentLevel); moves = 0; gameComplete = false; }
};

// 贪吃蛇游戏类
class SnakeGame {
private:
    struct Point {
        int x, y;
        Point(int _x = 0, int _y = 0) : x(_x), y(_y) {}
    };

    static const int WIDTH = 20;
    static const int HEIGHT = 20;
    static const int CELL_SIZE = 25;

    std::vector<Point> snake;
    Point food;
    int direction; // 0=右,1=下,2=左,3=上
    int score;
    bool gameOver;
    bool gameStarted;
    int moveCounter;

public:
    SnakeGame() : direction(0), score(0), gameOver(false), gameStarted(false), moveCounter(0) {
        Reset();
    }

    void Reset() {
        snake.clear();
        snake.push_back(Point(WIDTH / 2, HEIGHT / 2));
        snake.push_back(Point(WIDTH / 2 - 1, HEIGHT / 2));
        snake.push_back(Point(WIDTH / 2 - 2, HEIGHT / 2));
        direction = 0;
        score = 0;
        gameOver = false;
        gameStarted = true;
        GenerateFood();
    }

    void GenerateFood() {
        bool validPosition = false;
        while (!validPosition) {
            // 避免在边界墙上生成，范围 [1, WIDTH-2]
            food.x = rand() % (WIDTH - 2) + 1;
            food.y = rand() % (HEIGHT - 2) + 1;
            validPosition = true;
            for (const auto& segment : snake) {
                if (segment.x == food.x && segment.y == food.y) {
                    validPosition = false;
                    break;
                }
            }
        }
    }

    void Update() {
        if (gameOver || !gameStarted) return;

        moveCounter++;
        // 减慢移动速度：阈值设为6（比之前速度慢）
        if (moveCounter < 6) return;
        moveCounter = 0;

        Point newHead = snake[0];
        switch (direction) {
        case 0: newHead.x++; break;
        case 1: newHead.y++; break;
        case 2: newHead.x--; break;
        case 3: newHead.y--; break;
        }

        // 检查碰撞（四周为墙，撞到边界视为失败）
        if (newHead.x <= 0 || newHead.x >= WIDTH - 1 || newHead.y <= 0 || newHead.y >= HEIGHT - 1) {
            gameOver = true;
            return;
        }

        // 检查是否吃到食物
        bool ateFood = (newHead.x == food.x && newHead.y == food.y);

        // 移动蛇
        snake.insert(snake.begin(), newHead);
        if (!ateFood) {
            snake.pop_back();
        }
        else {
            score += 10;
            GenerateFood();
        }

        // 检查自身碰撞
        for (size_t i = 1; i < snake.size(); i++) {
            if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
                gameOver = true;
                break;
            }
        }
    }

    void ChangeDirection(int newDir) {
        if ((direction == 0 && newDir == 2) ||
            (direction == 2 && newDir == 0) ||
            (direction == 1 && newDir == 3) ||
            (direction == 3 && newDir == 1)) {
            return;
        }
        direction = newDir;
    }

    bool IsGameOver() { return gameOver; }
    int GetScore() { return score; }
    const std::vector<Point>& GetSnake() { return snake; }
    Point GetFood() { return food; }
    int GetWidth() { return WIDTH; }
    int GetHeight() { return HEIGHT; }
};

// BrickGame removed

// 主窗口类
class GameCenter {
private:
    HWND hwnd;
    HDC hdc;
    RECT clientRect;
    GameType currentGame;
    ScoreManager scoreManager;

    std::unique_ptr<SokobanGame> sokoban;
    std::unique_ptr<SnakeGame> snake;
    // Brick game removed

    bool showMenu;
    bool showScore;

    static GameCenter* instance;

public:
    GameCenter() : hwnd(nullptr), hdc(nullptr), currentGame(GAME_SOKOBAN),
        showMenu(true), showScore(false) {
        instance = this;
        sokoban = std::make_unique<SokobanGame>();
        snake = std::make_unique<SnakeGame>();
        // Brick game removed
    }

    ~GameCenter() {
        // unique_ptr会自动释放内存
    }

    void CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"GameCenterClass";

        RegisterClassEx(&wc);

        hwnd = CreateWindowEx(0, L"GameCenterClass", L"游戏中心 - 推箱子/贪吃蛇",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            800, 600, nullptr, nullptr, hInstance, nullptr);

        if (hwnd) {
            ShowWindow(hwnd, nCmdShow);
            UpdateWindow(hwnd);
            // 设置定时器，用于驱动游戏更新与持续移动（约60fps）
            SetTimer(hwnd, 1, 16, nullptr);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (instance) {
            return instance->HandleMessage(hwnd, msg, wParam, lParam);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_PAINT:
            OnPaint();
            break;

        case WM_TIMER:
            // 定时更新当前游戏（持续移动、物理和刷新）
        switch (currentGame) {
        case GAME_SOKOBAN:
            // 推箱子无需持续更新
            break;
        case GAME_SNAKE:
            if (snake && !snake->IsGameOver()) {
                snake->Update();
            }
            break;
        }
            InvalidateRect(hwnd, nullptr, FALSE);
            break;

        case WM_ERASEBKGND:
            // 避免默认擦除背景导致闪烁，使用自定义双缓冲绘制
            return 1;

        case WM_KEYDOWN:
            OnKeyDown(wParam);
            break;

        case WM_DESTROY:
            // 关闭定时器并退出
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }

    void OnPaint() {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &clientRect);

        // 双缓冲绘制，避免闪烁
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // 填充背景
        HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(memDC, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        if (showMenu) {
            DrawMenu(memDC);
        }
        else if (showScore) {
            DrawScore(memDC);
        }
        else {
            DrawGame(memDC);
        }

        // 复制到屏幕（使用SRCCOPY）
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

        // 清理双缓冲对象
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
    }

    void DrawMenu(HDC hdc) {
        // 使用支持中文的字体以避免乱码
        HFONT titleFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        HFONT menuFont = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");

        SetBkMode(hdc, TRANSPARENT);

        // 标题
        SelectObject(hdc, titleFont);
        SetTextColor(hdc, RGB(0, 0, 255));
        TextOut(hdc, 250, 50, L"游戏中心", (int)wcslen(L"游戏中心"));

        // 菜单选项
        SelectObject(hdc, menuFont);
        SetTextColor(hdc, RGB(0, 0, 0));
        TextOut(hdc, 300, 200, L"1. 推箱子", (int)wcslen(L"1. 推箱子"));
        TextOut(hdc, 300, 250, L"2. 贪吃蛇", (int)wcslen(L"2. 贪吃蛇"));
        TextOut(hdc, 300, 300, L"3. 查看最高分", (int)wcslen(L"3. 查看最高分"));
        TextOut(hdc, 300, 350, L"4. 退出游戏", (int)wcslen(L"4. 退出游戏"));

        SelectObject(hdc, titleFont);
        SetTextColor(hdc, RGB(128, 128, 128));
        TextOut(hdc, 250, 500, L"按数字键选择游戏", (int)wcslen(L"按数字键选择游戏"));

        DeleteObject(titleFont);
        DeleteObject(menuFont);
    }

    void DrawScore(HDC hdc) {
        HFONT titleFont = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        HFONT scoreFont = CreateFont(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");

        SetBkMode(hdc, TRANSPARENT);

        // 标题
        SelectObject(hdc, titleFont);
        SetTextColor(hdc, RGB(0, 0, 255));
        TextOut(hdc, 300, 50, L"最高分记录", (int)wcslen(L"最高分记录"));

        // 显示分数
        SelectObject(hdc, scoreFont);
        SetTextColor(hdc, RGB(0, 0, 0));

        wchar_t text[256];
        wsprintf(text, L"推箱子: %d 步", scoreManager.GetHighScore(GAME_SOKOBAN));
        TextOut(hdc, 250, 150, text, wcslen(text));

        wsprintf(text, L"贪吃蛇: %d 分", scoreManager.GetHighScore(GAME_SNAKE));
        TextOut(hdc, 250, 200, text, wcslen(text));

        SelectObject(hdc, titleFont);
        SetTextColor(hdc, RGB(128, 128, 128));
        TextOut(hdc, 250, 400, L"按 ESC 返回主菜单", (int)wcslen(L"按 ESC 返回主菜单"));

        DeleteObject(titleFont);
        DeleteObject(scoreFont);
    }

    void DrawGame(HDC hdc) {
        switch (currentGame) {
        case GAME_SOKOBAN:
            DrawSokoban(hdc);
            break;
        case GAME_SNAKE:
            DrawSnake(hdc);
            break;
        }
    }

    void DrawSokoban(HDC hdc) {
        const int CELL_SIZE = 40;
        int startX = (clientRect.right - 10 * CELL_SIZE) / 2;
        int startY = 50;

        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                int x = startX + j * CELL_SIZE;
                int y = startY + i * CELL_SIZE;
                int cell = sokoban->GetMap(j, i);

                RECT cellRect = { x, y, x + CELL_SIZE, y + CELL_SIZE };

                // 根据格子类型绘制：墙/目标/箱子/玩家/玩家在目标/箱子在目标
                switch (cell) {
                case 1: { // 墙
                    HBRUSH wallBrush = CreateSolidBrush(RGB(100, 100, 100));
                    FillRect(hdc, &cellRect, wallBrush);
                    DeleteObject(wallBrush);
                    break;
                }
                case 2: { // 目标点（浅灰）
                    HBRUSH targetBrush = CreateSolidBrush(RGB(200, 200, 200));
                    FillRect(hdc, &cellRect, targetBrush);
                    DeleteObject(targetBrush);
                    break;
                }
                case 3: { // 箱子（实心黑色方块）
                    HBRUSH boxBrush = CreateSolidBrush(RGB(0, 0, 0));
                    FillRect(hdc, &cellRect, boxBrush);
                    DeleteObject(boxBrush);
                    break;
                }
                case 4: { // 玩家（黄色圆）
                    // 背景为白色
                    HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(hdc, &cellRect, bgBrush);
                    DeleteObject(bgBrush);

                    // 画黄圆
                    HBRUSH playerBrush = CreateSolidBrush(RGB(255, 215, 0));
                    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
                    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, playerBrush);
                    Ellipse(hdc, x + 4, y + 4, x + CELL_SIZE - 4, y + CELL_SIZE - 4);
                    SelectObject(hdc, oldBrush);
                    SelectObject(hdc, oldPen);
                    DeleteObject(playerBrush);
                    break;
                }
                case 5: { // 玩家在目标点（浅灰背景 + 黄圆）
                    HBRUSH targetBrush = CreateSolidBrush(RGB(200, 200, 200));
                    FillRect(hdc, &cellRect, targetBrush);
                    DeleteObject(targetBrush);

                    HBRUSH playerBrush = CreateSolidBrush(RGB(255, 215, 0));
                    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
                    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, playerBrush);
                    Ellipse(hdc, x + 4, y + 4, x + CELL_SIZE - 4, y + CELL_SIZE - 4);
                    SelectObject(hdc, oldBrush);
                    SelectObject(hdc, oldPen);
                    DeleteObject(playerBrush);
                    break;
                }
                case 6: { // 箱子在目标点（浅灰背景 + 黑色方块）
                    HBRUSH targetBrush = CreateSolidBrush(RGB(200, 200, 200));
                    FillRect(hdc, &cellRect, targetBrush);
                    DeleteObject(targetBrush);

                    RECT inner = { x + 6, y + 6, x + CELL_SIZE - 6, y + CELL_SIZE - 6 };
                    HBRUSH boxBrush = CreateSolidBrush(RGB(0, 0, 0));
                    FillRect(hdc, &inner, boxBrush);
                    DeleteObject(boxBrush);
                    break;
                }
                default: { // 空地
                    HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(hdc, &cellRect, bgBrush);
                    DeleteObject(bgBrush);
                    break;
                }
                }

                // 绘制单元格边框（仅描边）
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                Rectangle(hdc, x, y, x + CELL_SIZE, y + CELL_SIZE);
                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
                DeleteObject(pen);
            }
        }

        // 显示信息
        HFONT font = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject(hdc, font);
        SetBkMode(hdc, TRANSPARENT);

        wchar_t text[256];
        wsprintf(text, L"移动步数: %d", sokoban->GetMoves());
        TextOut(hdc, 10, 10, text, wcslen(text));

        if (sokoban->IsGameComplete()) {
            SetTextColor(hdc, RGB(255, 0, 0));
            TextOut(hdc, 300, 500, L"恭喜通关！按 R 重新开始", 20);

            // 更新最高分（推箱子记录最少步数）
            int highScore = scoreManager.GetHighScore(GAME_SOKOBAN);
            if (highScore == 0 || sokoban->GetMoves() < highScore) {
                scoreManager.UpdateHighScore(GAME_SOKOBAN, sokoban->GetMoves());
            }
        }

        TextOut(hdc, 10, 40, L"方向键移动，R重新开始，ESC返回菜单", (int)wcslen(L"方向键移动，R重新开始，ESC返回菜单"));

        DeleteObject(font);
    }

    void DrawSnake(HDC hdc) {
        const int CELL_SIZE = 25;
        int startX = (clientRect.right - snake->GetWidth() * CELL_SIZE) / 2;
        int startY = 50;

        // 绘制网格
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        SelectObject(hdc, pen);

        for (int i = 0; i <= snake->GetWidth(); i++) {
            MoveToEx(hdc, startX + i * CELL_SIZE, startY, nullptr);
            LineTo(hdc, startX + i * CELL_SIZE, startY + snake->GetHeight() * CELL_SIZE);
            MoveToEx(hdc, startX, startY + i * CELL_SIZE, nullptr);
            LineTo(hdc, startX + snake->GetWidth() * CELL_SIZE, startY + i * CELL_SIZE);
        }

        // 绘制食物（红色实心圆）
        auto food = snake->GetFood();
        int fx = startX + food.x * CELL_SIZE + CELL_SIZE / 2;
        int fy = startY + food.y * CELL_SIZE + CELL_SIZE / 2;
        HBRUSH foodBrush = CreateSolidBrush(RGB(255, 0, 0));
        HPEN oldPenF = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        HBRUSH oldBrushF = (HBRUSH)SelectObject(hdc, foodBrush);
        Ellipse(hdc, fx - CELL_SIZE/2 + 4, fy - CELL_SIZE/2 + 4, fx + CELL_SIZE/2 - 4, fy + CELL_SIZE/2 - 4);
        SelectObject(hdc, oldBrushF);
        SelectObject(hdc, oldPenF);
        DeleteObject(foodBrush);

        // 绘制蛇（灰色方块），四周出现墙壁
        HBRUSH snakeBrush = CreateSolidBrush(RGB(150, 150, 150));
        auto& snakeBody = snake->GetSnake();
        for (const auto& segment : snakeBody) {
            RECT rect = { startX + segment.x * CELL_SIZE, startY + segment.y * CELL_SIZE,
                        startX + (segment.x + 1) * CELL_SIZE, startY + (segment.y + 1) * CELL_SIZE };
            FillRect(hdc, &rect, snakeBrush);
        }
        DeleteObject(snakeBrush);

        // 绘制边界墙（四周）
        HBRUSH wallBrush = CreateSolidBrush(RGB(100,100,100));
        for (int i = 0; i < snake->GetWidth(); i++) {
            // top
            RECT topRect = { startX + i * CELL_SIZE, startY, startX + (i+1) * CELL_SIZE, startY + CELL_SIZE };
            FillRect(hdc, &topRect, wallBrush);
            // bottom
            RECT bottomRect = { startX + i * CELL_SIZE, startY + snake->GetHeight() * CELL_SIZE - CELL_SIZE, startX + (i+1) * CELL_SIZE, startY + snake->GetHeight() * CELL_SIZE };
            FillRect(hdc, &bottomRect, wallBrush);
        }
        for (int i = 0; i < snake->GetHeight(); i++) {
            // left
            RECT leftRect = { startX, startY + i * CELL_SIZE, startX + CELL_SIZE, startY + (i+1) * CELL_SIZE };
            FillRect(hdc, &leftRect, wallBrush);
            // right
            RECT rightRect = { startX + snake->GetWidth() * CELL_SIZE - CELL_SIZE, startY + i * CELL_SIZE, startX + snake->GetWidth() * CELL_SIZE, startY + (i+1) * CELL_SIZE };
            FillRect(hdc, &rightRect, wallBrush);
        }
        DeleteObject(wallBrush);

        DeleteObject(pen);

        // 显示信息
        HFONT font = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
        SelectObject(hdc, font);
        SetBkMode(hdc, TRANSPARENT);

        wchar_t text[256];
        wsprintf(text, L"得分: %d", snake->GetScore());
        TextOut(hdc, 10, 10, text, wcslen(text));

        wsprintf(text, L"最高分: %d", scoreManager.GetHighScore(GAME_SNAKE));
        TextOut(hdc, 10, 40, text, wcslen(text));

        if (snake->IsGameOver()) {
            SetTextColor(hdc, RGB(255, 0, 0));
            TextOut(hdc, 300, 500, L"游戏结束！按 R 重新开始", 20);

            scoreManager.UpdateHighScore(GAME_SNAKE, snake->GetScore());
        }

        TextOut(hdc, 10, 70, L"方向键控制，R重新开始，ESC返回菜单", 26);

        DeleteObject(font);
    }

    // DrawBrick removed with BrickGame

    void OnKeyDown(WPARAM wParam) {
        if (showMenu) {
            switch (wParam) {
            case '1':
                currentGame = GAME_SOKOBAN;
                showMenu = false;
                showScore = false;
                InvalidateRect(hwnd, nullptr, TRUE);
                break;
            case '2':
                currentGame = GAME_SNAKE;
                showMenu = false;
                showScore = false;
                InvalidateRect(hwnd, nullptr, TRUE);
                break;
            case '3':
                showScore = true;
                showMenu = false;
                InvalidateRect(hwnd, nullptr, TRUE);
                break;
            case '4':
                PostQuitMessage(0);
                break;
            }
        }
        else if (showScore) {
            if (wParam == VK_ESCAPE) {
                showMenu = true;
                showScore = false;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        else {
            switch (currentGame) {
            case GAME_SOKOBAN:
                HandleSokobanInput(wParam);
                break;
            case GAME_SNAKE:
                HandleSnakeInput(wParam);
                break;
            // Brick game removed
            }

            if (wParam == VK_ESCAPE) {
                showMenu = true;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else if (wParam == 'R' || wParam == 'r') {
                ResetCurrentGame();
                InvalidateRect(hwnd, nullptr, TRUE);
            }

            InvalidateRect(hwnd, nullptr, TRUE);
        }
    }

    void HandleSokobanInput(WPARAM wParam) {
        switch (wParam) {
        case VK_LEFT: sokoban->Move(-1, 0); break;
        case VK_RIGHT: sokoban->Move(1, 0); break;
        case VK_UP: sokoban->Move(0, -1); break;
        case VK_DOWN: sokoban->Move(0, 1); break;
        }

        if (sokoban->IsGameComplete()) {
            scoreManager.UpdateHighScore(GAME_SOKOBAN, sokoban->GetMoves());
        }
    }

    void HandleSnakeInput(WPARAM wParam) {
        switch (wParam) {
        case VK_RIGHT: snake->ChangeDirection(0); break;
        case VK_DOWN: snake->ChangeDirection(1); break;
        case VK_LEFT: snake->ChangeDirection(2); break;
        case VK_UP: snake->ChangeDirection(3); break;
        }

        static DWORD lastUpdate = GetTickCount();
        // 提高更新频率以加快贪吃蛇移动速度
        if (GetTickCount() - lastUpdate > 25) {
            snake->Update();
            lastUpdate = GetTickCount();
        }

        if (snake->IsGameOver()) {
            scoreManager.UpdateHighScore(GAME_SNAKE, snake->GetScore());
        }
    }

    // HandleBrickInput removed with BrickGame

    void ResetCurrentGame() {
        switch (currentGame) {
        case GAME_SOKOBAN:
            sokoban->Reset();
            break;
        case GAME_SNAKE:
            snake->Reset();
            break;
        // Brick game removed
        }
    }

    void Run() {
        MSG msg = { 0 };
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

GameCenter* GameCenter::instance = nullptr;

// 程序入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    srand(GetTickCount());

    GameCenter gameCenter;
    gameCenter.CreateMainWindow(hInstance, nCmdShow);

    // 尝试在桌面创建快捷方式（只创建一次）
    CreateDesktopShortcut(L"GameCenter");
    gameCenter.Run();

    return 0;
}