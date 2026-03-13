#include <windows.h>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#include <algorithm>
#include <ctime>
#include <string>

struct FakeWindow {
    RECT rect{};
    std::wstring title;
    COLORREF bodyColor = RGB(255, 255, 255);
    bool visible = false;
};

static const int kTaskbarHeight = 44;
static const int kStartButtonWidth = 94;
static const int kMenuWidth = 240;
static const int kMenuHeight = 320;

static FakeWindow g_notepad;
static FakeWindow g_computer;
static bool g_startMenuOpen = false;
static bool g_dragging = false;
static FakeWindow* g_dragTarget = nullptr;
static POINT g_dragOffset{};

static RECT MakeRect(int l, int t, int r, int b) {
    RECT rc{ l, t, r, b };
    return rc;
}

static bool PointInRect(const RECT& r, int x, int y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

static RECT StartButtonRect(const RECT& client) {
    return MakeRect(0, client.bottom - kTaskbarHeight, kStartButtonWidth, client.bottom);
}

static RECT StartMenuRect(const RECT& client) {
    return MakeRect(0, client.bottom - kTaskbarHeight - kMenuHeight, kMenuWidth, client.bottom - kTaskbarHeight);
}

static RECT NotepadMenuItemRect(const RECT& client) {
    RECT m = StartMenuRect(client);
    return MakeRect(m.left + 14, m.top + 60, m.right - 14, m.top + 104);
}

static RECT ComputerMenuItemRect(const RECT& client) {
    RECT m = StartMenuRect(client);
    return MakeRect(m.left + 14, m.top + 114, m.right - 14, m.top + 158);
}

static RECT DesktopNotepadIconRect() {
    return MakeRect(16, 24, 108, 136);
}

static RECT DesktopComputerIconRect() {
    return MakeRect(16, 146, 108, 258);
}

static RECT WindowCloseRect(const FakeWindow& w) {
    return MakeRect(w.rect.right - 42, w.rect.top + 5, w.rect.right - 10, w.rect.top + 29);
}

static RECT WindowTitleRect(const FakeWindow& w) {
    return MakeRect(w.rect.left, w.rect.top, w.rect.right, w.rect.top + 34);
}

static void DrawGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX vert[2] = {
        { rc.left, rc.top, (COLOR16)(GetRValue(top) << 8), (COLOR16)(GetGValue(top) << 8), (COLOR16)(GetBValue(top) << 8), 0x0000 },
        { rc.right, rc.bottom, (COLOR16)(GetRValue(bottom) << 8), (COLOR16)(GetGValue(bottom) << 8), (COLOR16)(GetBValue(bottom) << 8), 0x0000 }
    };
    GRADIENT_RECT gRect = { 0, 1 };
    GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

static void DrawTextCentered(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, int fontHeight, bool bold = false) {
    HFONT font = CreateFontW(fontHeight, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT drawRc = rc;
    DrawTextW(hdc, text.c_str(), -1, &drawRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);
}

static void DrawDesktopIcon(HDC hdc, const RECT& rc, const wchar_t* label, COLORREF accent) {
    RECT icon = MakeRect(rc.left + 26, rc.top + 8, rc.left + 66, rc.top + 50);
    HBRUSH iconBrush = CreateSolidBrush(accent);
    FillRect(hdc, &icon, iconBrush);
    DeleteObject(iconBrush);

    HPEN iconPen = CreatePen(PS_SOLID, 1, RGB(240, 240, 240));
    HPEN oldPen = (HPEN)SelectObject(hdc, iconPen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, icon.left, icon.top, icon.right, icon.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(iconPen);

    RECT textRc = MakeRect(rc.left + 2, rc.top + 56, rc.right - 2, rc.bottom);
    DrawTextCentered(hdc, textRc, label, RGB(245, 249, 255), 16, false);
}

static void DrawWindow(HDC hdc, const FakeWindow& w) {
    if (!w.visible) return;

    HBRUSH body = CreateSolidBrush(w.bodyColor);
    FillRect(hdc, &w.rect, body);
    DeleteObject(body);

    RECT titleBar = w.rect;
    titleBar.bottom = titleBar.top + 34;
    DrawGradient(hdc, titleBar, RGB(108, 170, 232), RGB(62, 128, 201));

    DrawTextCentered(hdc, MakeRect(w.rect.left + 12, w.rect.top, w.rect.right - 52, w.rect.top + 34), w.title, RGB(255, 255, 255), 18, true);

    RECT closeBtn = WindowCloseRect(w);
    HBRUSH closeBrush = CreateSolidBrush(RGB(196, 40, 28));
    FillRect(hdc, &closeBtn, closeBrush);
    DeleteObject(closeBrush);
    DrawTextCentered(hdc, closeBtn, L"X", RGB(255, 255, 255), 17, true);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(36, 90, 154));
    HPEN oldPen = (HPEN)SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, w.rect.left, w.rect.top, w.rect.right, w.rect.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    if (w.title == L"Notepad") {
        RECT textRc = MakeRect(w.rect.left + 20, w.rect.top + 50, w.rect.right - 20, w.rect.bottom - 20);
        DrawTextW(hdc, L"Welcome to C-OS!\n\n- Desktop icon ekledik\n- Daha düzgün pencere sürükleme\n- CI ve release iyileştirmeleri", -1, &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
    else if (w.title == L"Computer") {
        RECT textRc = MakeRect(w.rect.left + 20, w.rect.top + 52, w.rect.right - 20, w.rect.bottom - 20);
        DrawTextW(hdc,
            L"Computer\n\n- Local Disk (C:)\n- Data (D:)\n- Network\n\n(simulated)",
            -1, &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }
}

static void DrawClock(HDC hdc, const RECT& client) {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);

    wchar_t clockBuf[16];
    wchar_t dateBuf[24];
    wsprintfW(clockBuf, L"%02d:%02d", tm.tm_hour, tm.tm_min);
    wsprintfW(dateBuf, L"%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);

    RECT clockRc = MakeRect(client.right - 110, client.bottom - kTaskbarHeight + 2, client.right - 8, client.bottom - 22);
    RECT dateRc = MakeRect(client.right - 120, client.bottom - 22, client.right - 8, client.bottom - 4);
    DrawTextCentered(hdc, clockRc, clockBuf, RGB(240, 248, 255), 16, true);
    DrawTextCentered(hdc, dateRc, dateBuf, RGB(220, 233, 249), 14, false);
}

static void DrawTaskbarButton(HDC hdc, const RECT& rc, const wchar_t* title, bool active) {
    DrawGradient(hdc, rc, active ? RGB(116, 168, 224) : RGB(85, 134, 193), active ? RGB(72, 123, 186) : RGB(54, 97, 158));
    DrawTextCentered(hdc, rc, title, RGB(255, 255, 255), 15, false);
}

static void PaintScene(HWND hwnd, HDC hdc) {
    RECT client;
    GetClientRect(hwnd, &client);

    DrawGradient(hdc, client, RGB(94, 152, 224), RGB(17, 72, 145));

    DrawDesktopIcon(hdc, DesktopNotepadIconRect(), L"Notepad", RGB(74, 138, 214));
    DrawDesktopIcon(hdc, DesktopComputerIconRect(), L"Computer", RGB(75, 112, 190));

    RECT taskbar = MakeRect(0, client.bottom - kTaskbarHeight, client.right, client.bottom);
    DrawGradient(hdc, taskbar, RGB(58, 111, 179), RGB(36, 78, 134));

    RECT start = StartButtonRect(client);
    DrawGradient(hdc, start, RGB(98, 166, 58), RGB(59, 114, 29));
    DrawTextCentered(hdc, start, L"Start", RGB(255, 255, 255), 20, true);

    int taskX = kStartButtonWidth + 8;
    if (g_notepad.visible) {
        RECT btn = MakeRect(taskX, client.bottom - kTaskbarHeight + 6, taskX + 130, client.bottom - 6);
        DrawTaskbarButton(hdc, btn, L"Notepad", true);
        taskX += 136;
    }
    if (g_computer.visible) {
        RECT btn = MakeRect(taskX, client.bottom - kTaskbarHeight + 6, taskX + 130, client.bottom - 6);
        DrawTaskbarButton(hdc, btn, L"Computer", true);
    }

    DrawClock(hdc, client);

    if (g_startMenuOpen) {
        RECT menu = StartMenuRect(client);
        DrawGradient(hdc, menu, RGB(233, 241, 251), RGB(189, 216, 244));

        RECT header = MakeRect(menu.left, menu.top, menu.right, menu.top + 48);
        DrawGradient(hdc, header, RGB(90, 152, 222), RGB(52, 110, 181));
        DrawTextCentered(hdc, header, L"C-OS User", RGB(255, 255, 255), 20, true);

        RECT item1 = NotepadMenuItemRect(client);
        RECT item2 = ComputerMenuItemRect(client);

        HBRUSH itemBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &item1, itemBrush);
        FillRect(hdc, &item2, itemBrush);
        DeleteObject(itemBrush);

        DrawTextCentered(hdc, item1, L"Notepad", RGB(33, 33, 33), 18, false);
        DrawTextCentered(hdc, item2, L"Computer", RGB(33, 33, 33), 18, false);

        HPEN border = CreatePen(PS_SOLID, 1, RGB(72, 125, 193));
        HPEN oldPen = (HPEN)SelectObject(hdc, border);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, menu.left, menu.top, menu.right, menu.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(border);
    }

    DrawWindow(hdc, g_computer);
    DrawWindow(hdc, g_notepad);
}

static void InitWindows() {
    g_notepad.title = L"Notepad";
    g_notepad.rect = MakeRect(180, 120, 760, 520);
    g_notepad.bodyColor = RGB(255, 255, 255);

    g_computer.title = L"Computer";
    g_computer.rect = MakeRect(230, 160, 700, 500);
    g_computer.bodyColor = RGB(248, 251, 255);
}

static FakeWindow* HitTestWindow(int x, int y) {
    if (g_notepad.visible && PointInRect(g_notepad.rect, x, y)) return &g_notepad;
    if (g_computer.visible && PointInRect(g_computer.rect, x, y)) return &g_computer;
    return nullptr;
}

static void OpenWindow(FakeWindow& w) {
    w.visible = true;
}

static void ClampWindowToClient(FakeWindow& w, const RECT& client) {
    int width = w.rect.right - w.rect.left;
    int height = w.rect.bottom - w.rect.top;

    int minLeft = 0;
    int maxLeft = std::max(0, client.right - width);
    int minTop = 0;
    int maxTop = std::max(0, client.bottom - kTaskbarHeight - height);

    w.rect.left = std::clamp(w.rect.left, minLeft, maxLeft);
    w.rect.top = std::clamp(w.rect.top, minTop, maxTop);
    w.rect.right = w.rect.left + width;
    w.rect.bottom = w.rect.top + height;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitWindows();
        SetTimer(hwnd, 1, 1000, nullptr);
        return 0;

    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        RECT client;
        GetClientRect(hwnd, &client);

        if (PointInRect(StartButtonRect(client), x, y)) {
            g_startMenuOpen = !g_startMenuOpen;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PointInRect(DesktopNotepadIconRect(), x, y)) {
            OpenWindow(g_notepad);
            g_startMenuOpen = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PointInRect(DesktopComputerIconRect(), x, y)) {
            OpenWindow(g_computer);
            g_startMenuOpen = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_startMenuOpen && PointInRect(NotepadMenuItemRect(client), x, y)) {
            OpenWindow(g_notepad);
            g_startMenuOpen = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_startMenuOpen && PointInRect(ComputerMenuItemRect(client), x, y)) {
            OpenWindow(g_computer);
            g_startMenuOpen = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        FakeWindow* w = HitTestWindow(x, y);
        if (w) {
            if (PointInRect(WindowCloseRect(*w), x, y)) {
                w->visible = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (PointInRect(WindowTitleRect(*w), x, y)) {
                g_dragging = true;
                g_dragTarget = w;
                g_dragOffset.x = x - w->rect.left;
                g_dragOffset.y = y - w->rect.top;
                SetCapture(hwnd);
                return 0;
            }
        }

        g_startMenuOpen = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_dragging && g_dragTarget) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            int width = g_dragTarget->rect.right - g_dragTarget->rect.left;
            int height = g_dragTarget->rect.bottom - g_dragTarget->rect.top;
            g_dragTarget->rect.left = x - g_dragOffset.x;
            g_dragTarget->rect.top = y - g_dragOffset.y;
            g_dragTarget->rect.right = g_dragTarget->rect.left + width;
            g_dragTarget->rect.bottom = g_dragTarget->rect.top + height;

            RECT client;
            GetClientRect(hwnd, &client);
            ClampWindowToClient(*g_dragTarget, client);

            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_dragging) {
            g_dragging = false;
            g_dragTarget = nullptr;
            ReleaseCapture();
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintScene(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"COSDesktopClass";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"C-OS | Win7-style C++ Desktop Simulator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
