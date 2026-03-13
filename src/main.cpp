#define NOMINMAX
#include <windows.h>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#include <ctime>
#include <string>
#include <vector>

struct FakeWindow {
    RECT rect{};
    std::wstring title;
    COLORREF bodyColor = RGB(255, 255, 255);
    bool visible = false;
};

struct ComputerItem {
    std::wstring name;
    std::wstring type;
    std::wstring size;
    std::wstring modified;
};

static const int kTaskbarHeight = 44;
static const int kStartButtonWidth = 94;
static const int kMenuWidth = 260;
static const int kMenuHeight = 340;

static FakeWindow g_notepad;
static FakeWindow g_computer;
static bool g_startMenuOpen = false;
static bool g_dragging = false;
static FakeWindow* g_dragTarget = nullptr;
static POINT g_dragOffset{};

static std::wstring g_notepadText = L"C-OS Notes\r\n\r\n- Bu Notepad artık yazı yazmayı destekliyor.\r\n- Üstte New / Insert Time / Clear butonları var.\r\n";
static bool g_notepadFocused = false;

static std::vector<ComputerItem> g_computerItems;
static int g_selectedComputerItem = -1;

static RECT MakeRect(int l, int t, int r, int b) {
    RECT rc{ l, t, r, b };
    return rc;
}

static bool PointInRect(const RECT& r, int x, int y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

static int ClampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static RECT StartButtonRect(const RECT& client) {
    return MakeRect(0, client.bottom - kTaskbarHeight, kStartButtonWidth, client.bottom);
}

static RECT StartMenuRect(const RECT& client) {
    return MakeRect(0, client.bottom - kTaskbarHeight - kMenuHeight, kMenuWidth, client.bottom - kTaskbarHeight);
}

static RECT NotepadMenuItemRect(const RECT& client) {
    RECT m = StartMenuRect(client);
    return MakeRect(m.left + 16, m.top + 68, m.right - 16, m.top + 112);
}

static RECT ComputerMenuItemRect(const RECT& client) {
    RECT m = StartMenuRect(client);
    return MakeRect(m.left + 16, m.top + 122, m.right - 16, m.top + 166);
}

static RECT DesktopNotepadIconRect() {
    return MakeRect(16, 24, 116, 140);
}

static RECT DesktopComputerIconRect() {
    return MakeRect(16, 148, 116, 264);
}

static RECT WindowCloseRect(const FakeWindow& w) {
    return MakeRect(w.rect.right - 42, w.rect.top + 5, w.rect.right - 10, w.rect.top + 29);
}

static RECT WindowTitleRect(const FakeWindow& w) {
    return MakeRect(w.rect.left, w.rect.top, w.rect.right, w.rect.top + 34);
}

static RECT NotepadToolbarRect(const FakeWindow& w) {
    return MakeRect(w.rect.left + 8, w.rect.top + 42, w.rect.right - 8, w.rect.top + 76);
}

static RECT NotepadEditorRect(const FakeWindow& w) {
    return MakeRect(w.rect.left + 10, w.rect.top + 82, w.rect.right - 10, w.rect.bottom - 12);
}

static RECT NotepadNewBtnRect(const FakeWindow& w) {
    RECT tb = NotepadToolbarRect(w);
    return MakeRect(tb.left + 4, tb.top + 4, tb.left + 82, tb.bottom - 4);
}

static RECT NotepadInsertTimeBtnRect(const FakeWindow& w) {
    RECT tb = NotepadToolbarRect(w);
    return MakeRect(tb.left + 88, tb.top + 4, tb.left + 220, tb.bottom - 4);
}

static RECT NotepadClearBtnRect(const FakeWindow& w) {
    RECT tb = NotepadToolbarRect(w);
    return MakeRect(tb.left + 224, tb.top + 4, tb.left + 306, tb.bottom - 4);
}

static RECT ComputerListRect(const FakeWindow& w) {
    return MakeRect(w.rect.left + 14, w.rect.top + 70, w.rect.left + 330, w.rect.bottom - 16);
}

static RECT ComputerDetailRect(const FakeWindow& w) {
    return MakeRect(w.rect.left + 340, w.rect.top + 70, w.rect.right - 14, w.rect.bottom - 16);
}

static RECT ComputerItemRect(const FakeWindow& w, int index) {
    RECT list = ComputerListRect(w);
    int itemTop = list.top + 10 + index * 34;
    return MakeRect(list.left + 8, itemTop, list.right - 8, itemTop + 30);
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

static void DrawTextBlock(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, int fontHeight) {
    HFONT font = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT drawRc = rc;
    DrawTextW(hdc, text.c_str(), -1, &drawRc, DT_LEFT | DT_TOP | DT_WORDBREAK);
    SelectObject(hdc, old);
    DeleteObject(font);
}

static void DrawDesktopIcon(HDC hdc, const RECT& rc, const wchar_t* label, COLORREF accent) {
    RECT icon = MakeRect(rc.left + 28, rc.top + 8, rc.left + 68, rc.top + 50);
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

    RECT textRc = MakeRect(rc.left + 4, rc.top + 56, rc.right - 4, rc.bottom);
    DrawTextCentered(hdc, textRc, label, RGB(245, 249, 255), 16, false);
}

static void DrawButton(HDC hdc, const RECT& rc, const wchar_t* caption) {
    DrawGradient(hdc, rc, RGB(246, 248, 252), RGB(219, 227, 238));
    HPEN border = CreatePen(PS_SOLID, 1, RGB(150, 166, 188));
    HPEN oldPen = (HPEN)SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);
    DrawTextCentered(hdc, rc, caption, RGB(35, 35, 35), 14, false);
}

static void DrawNotepadContent(HDC hdc, const FakeWindow& w) {
    RECT toolbar = NotepadToolbarRect(w);
    RECT editor = NotepadEditorRect(w);

    HBRUSH toolbarBrush = CreateSolidBrush(RGB(236, 241, 248));
    FillRect(hdc, &toolbar, toolbarBrush);
    DeleteObject(toolbarBrush);

    DrawButton(hdc, NotepadNewBtnRect(w), L"New");
    DrawButton(hdc, NotepadInsertTimeBtnRect(w), L"Insert Time");
    DrawButton(hdc, NotepadClearBtnRect(w), L"Clear");

    HBRUSH editorBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &editor, editorBrush);
    DeleteObject(editorBrush);

    HPEN border = CreatePen(PS_SOLID, 1, g_notepadFocused ? RGB(50, 132, 222) : RGB(170, 180, 194));
    HPEN oldPen = (HPEN)SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, editor.left, editor.top, editor.right, editor.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    RECT textRc = MakeRect(editor.left + 10, editor.top + 10, editor.right - 12, editor.bottom - 12);
    std::wstring visibleText = g_notepadText;
    if (g_notepadFocused && ((GetTickCount() / 500) % 2 == 0)) {
        visibleText += L"|";
    }
    DrawTextBlock(hdc, textRc, visibleText, RGB(20, 20, 20), 17);
}

static void DrawComputerContent(HDC hdc, const FakeWindow& w) {
    RECT list = ComputerListRect(w);
    RECT detail = ComputerDetailRect(w);

    HBRUSH panelBrush = CreateSolidBrush(RGB(249, 251, 254));
    FillRect(hdc, &list, panelBrush);
    FillRect(hdc, &detail, panelBrush);
    DeleteObject(panelBrush);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(170, 180, 194));
    HPEN oldPen = (HPEN)SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, list.left, list.top, list.right, list.bottom);
    Rectangle(hdc, detail.left, detail.top, detail.right, detail.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);

    DrawTextCentered(hdc, MakeRect(list.left, list.top + 2, list.right, list.top + 28), L"Files", RGB(45, 64, 93), 15, true);

    for (int i = 0; i < (int)g_computerItems.size(); ++i) {
        RECT row = ComputerItemRect(w, i);
        HBRUSH rowBrush = CreateSolidBrush(i == g_selectedComputerItem ? RGB(211, 229, 250) : RGB(255, 255, 255));
        FillRect(hdc, &row, rowBrush);
        DeleteObject(rowBrush);

        HPEN rowPen = CreatePen(PS_SOLID, 1, RGB(224, 230, 240));
        HPEN oldRowPen = (HPEN)SelectObject(hdc, rowPen);
        HGDIOBJ oldRowBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, row.left, row.top, row.right, row.bottom);
        SelectObject(hdc, oldRowBrush);
        SelectObject(hdc, oldRowPen);
        DeleteObject(rowPen);

        DrawTextBlock(hdc, MakeRect(row.left + 8, row.top + 6, row.right - 8, row.bottom - 4), g_computerItems[i].name, RGB(25, 25, 25), 14);
    }

    DrawTextCentered(hdc, MakeRect(detail.left, detail.top + 2, detail.right, detail.top + 30), L"Details", RGB(45, 64, 93), 15, true);

    std::wstring detailText = L"Select a file to view metadata.";
    if (g_selectedComputerItem >= 0 && g_selectedComputerItem < (int)g_computerItems.size()) {
        const ComputerItem& it = g_computerItems[g_selectedComputerItem];
        detailText = L"Name: " + it.name + L"\r\n\r\nType: " + it.type + L"\r\n\r\nSize: " + it.size + L"\r\n\r\nModified: " + it.modified;
    }
    DrawTextBlock(hdc, MakeRect(detail.left + 12, detail.top + 42, detail.right - 12, detail.bottom - 12), detailText, RGB(30, 30, 30), 15);
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
        DrawNotepadContent(hdc, w);
    } else if (w.title == L"Computer") {
        DrawComputerContent(hdc, w);
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

static void DrawTaskbarButton(HDC hdc, const RECT& rc, const wchar_t* title) {
    DrawGradient(hdc, rc, RGB(112, 164, 221), RGB(70, 118, 180));
    DrawTextCentered(hdc, rc, title, RGB(255, 255, 255), 14, false);
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
        RECT btn = MakeRect(taskX, client.bottom - kTaskbarHeight + 6, taskX + 132, client.bottom - 6);
        DrawTaskbarButton(hdc, btn, L"Notepad");
        taskX += 138;
    }
    if (g_computer.visible) {
        RECT btn = MakeRect(taskX, client.bottom - kTaskbarHeight + 6, taskX + 132, client.bottom - 6);
        DrawTaskbarButton(hdc, btn, L"Computer");
    }

    DrawClock(hdc, client);

    if (g_startMenuOpen) {
        RECT menu = StartMenuRect(client);
        DrawGradient(hdc, menu, RGB(233, 241, 251), RGB(189, 216, 244));

        RECT header = MakeRect(menu.left, menu.top, menu.right, menu.top + 52);
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

        DrawTextBlock(hdc, MakeRect(menu.left + 16, menu.top + 190, menu.right - 16, menu.bottom - 16),
            L"Quick Tips:\r\n- Notepad icinde yazabilirsin\r\n- Computer penceresinde dosya secip detay gorebilirsin", RGB(42, 55, 77), 14);

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
    g_notepad.rect = MakeRect(160, 100, 860, 560);
    g_notepad.bodyColor = RGB(248, 250, 253);

    g_computer.title = L"Computer";
    g_computer.rect = MakeRect(220, 130, 920, 560);
    g_computer.bodyColor = RGB(246, 250, 255);

    g_computerItems = {
        {L"Projects", L"Folder", L"--", L"2026-03-13"},
        {L"Wallpaper.jpg", L"JPEG Image", L"2.1 MB", L"2026-02-11"},
        {L"game-save.dat", L"DAT File", L"540 KB", L"2026-03-01"},
        {L"setup.log", L"Text Document", L"14 KB", L"2026-03-09"},
        {L"C-OS.exe", L"Application", L"1.8 MB", L"2026-03-13"}
    };
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
    int maxLeft = (client.right - width > 0) ? (client.right - width) : 0;
    int minTop = 0;
    int maxTop = (client.bottom - kTaskbarHeight - height > 0) ? (client.bottom - kTaskbarHeight - height) : 0;

    w.rect.left = ClampInt(w.rect.left, minLeft, maxLeft);
    w.rect.top = ClampInt(w.rect.top, minTop, maxTop);
    w.rect.right = w.rect.left + width;
    w.rect.bottom = w.rect.top + height;
}

static void InsertCurrentTimeToNotepad() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);

    wchar_t stamp[64];
    wsprintfW(stamp, L"[%02d:%02d %02d/%02d/%04d]\r\n", tm.tm_hour, tm.tm_min, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
    g_notepadText += stamp;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitWindows();
        SetTimer(hwnd, 1, 400, nullptr);
        return 0;

    case WM_TIMER:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        RECT client;
        GetClientRect(hwnd, &client);

        g_notepadFocused = false;

        if (PointInRect(StartButtonRect(client), x, y)) {
            g_startMenuOpen = !g_startMenuOpen;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (PointInRect(DesktopNotepadIconRect(), x, y)) {
            OpenWindow(g_notepad);
            g_startMenuOpen = false;
            g_notepadFocused = true;
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
            g_notepadFocused = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_startMenuOpen && PointInRect(ComputerMenuItemRect(client), x, y)) {
            OpenWindow(g_computer);
            g_startMenuOpen = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_notepad.visible) {
            if (PointInRect(NotepadNewBtnRect(g_notepad), x, y)) {
                g_notepadText = L"";
                g_notepadFocused = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRect(NotepadInsertTimeBtnRect(g_notepad), x, y)) {
                InsertCurrentTimeToNotepad();
                g_notepadFocused = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRect(NotepadClearBtnRect(g_notepad), x, y)) {
                g_notepadText = L"";
                g_notepadFocused = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRect(NotepadEditorRect(g_notepad), x, y)) {
                g_notepadFocused = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        if (g_computer.visible) {
            for (int i = 0; i < (int)g_computerItems.size(); ++i) {
                if (PointInRect(ComputerItemRect(g_computer, i), x, y)) {
                    g_selectedComputerItem = i;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
        }

        FakeWindow* w = HitTestWindow(x, y);
        if (w) {
            if (PointInRect(WindowCloseRect(*w), x, y)) {
                w->visible = false;
                if (w == &g_notepad) g_notepadFocused = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (PointInRect(WindowTitleRect(*w), x, y)) {
                g_dragging = true;
                g_dragTarget = w;
                g_dragOffset.x = x - w->rect.left;
                g_dragOffset.y = y - w->rect.top;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        g_startMenuOpen = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CHAR:
        if (g_notepad.visible && g_notepadFocused) {
            if (wParam == VK_BACK) {
                if (!g_notepadText.empty()) {
                    g_notepadText.pop_back();
                }
            } else if (wParam == VK_RETURN) {
                g_notepadText += L"\r\n";
            } else if (wParam == VK_TAB) {
                g_notepadText += L"    ";
            } else if (wParam >= 32) {
                g_notepadText += (wchar_t)wParam;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

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
