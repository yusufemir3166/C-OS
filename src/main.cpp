#define NOMINMAX
#include <windows.h>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

struct AppDef {
    std::wstring name;
    std::wstring category;
    std::wstring summary;
    std::wstring line1;
    std::wstring line2;
    std::wstring line3;
    std::wstring actionLabel;
};

struct AppWindow {
    RECT rect{};
    int appIndex = 0;
    int metric = 0;
    int selection = 0;
    bool toggle = false;
    std::wstring notes;
};

static const int kTaskbarHeight = 44;
static const int kStartButtonWidth = 96;
static const int kMenuWidth = 330;
static const int kMenuHeight = 560;

static bool g_startMenuOpen = false;
static bool g_dragging = false;
static int g_draggingWindow = -1;
static POINT g_dragOffset{};

static std::vector<AppDef> g_apps;
static std::vector<AppWindow> g_windows;

static int ClampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static RECT MakeRect(int l, int t, int r, int b) {
    RECT rc{ l, t, r, b };
    return rc;
}

static bool PointInRectI(const RECT& r, int x, int y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

static RECT StartButtonRect(const RECT& client) {
    return MakeRect(0, client.bottom - kTaskbarHeight, kStartButtonWidth, client.bottom);
}

static RECT StartMenuRect(const RECT& client) {
    return MakeRect(0, client.bottom - kTaskbarHeight - kMenuHeight, kMenuWidth, client.bottom - kTaskbarHeight);
}

static RECT StartMenuItemRect(const RECT& client, int index) {
    RECT m = StartMenuRect(client);
    int top = m.top + 58 + index * 30;
    return MakeRect(m.left + 12, top, m.right - 12, top + 26);
}

static RECT WindowTitleRect(const AppWindow& w) {
    return MakeRect(w.rect.left, w.rect.top, w.rect.right, w.rect.top + 34);
}

static RECT WindowCloseRect(const AppWindow& w) {
    return MakeRect(w.rect.right - 42, w.rect.top + 5, w.rect.right - 10, w.rect.top + 29);
}

static RECT WindowActionButtonRect(const AppWindow& w) {
    return MakeRect(w.rect.left + 14, w.rect.top + 106, w.rect.left + 210, w.rect.top + 136);
}

static RECT WindowToggleButtonRect(const AppWindow& w) {
    return MakeRect(w.rect.left + 220, w.rect.top + 106, w.rect.left + 360, w.rect.top + 136);
}

static RECT WindowSelectButtonRect(const AppWindow& w) {
    return MakeRect(w.rect.left + 370, w.rect.top + 106, w.rect.left + 540, w.rect.top + 136);
}

static RECT WindowContentRect(const AppWindow& w) {
    return MakeRect(w.rect.left + 14, w.rect.top + 146, w.rect.right - 14, w.rect.bottom - 14);
}

static void DrawGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX vert[2] = {
        { rc.left, rc.top, (COLOR16)(GetRValue(top) << 8), (COLOR16)(GetGValue(top) << 8), (COLOR16)(GetBValue(top) << 8), 0x0000 },
        { rc.right, rc.bottom, (COLOR16)(GetRValue(bottom) << 8), (COLOR16)(GetGValue(bottom) << 8), (COLOR16)(GetBValue(bottom) << 8), 0x0000 }
    };
    GRADIENT_RECT gRect = { 0, 1 };
    GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

static void DrawCentered(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, int height, bool bold = false) {
    HFONT font = CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT r = rc;
    DrawTextW(hdc, text.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);
}

static void DrawBlock(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, int height, bool bold = false) {
    HFONT font = CreateFontW(height, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT r = rc;
    DrawTextW(hdc, text.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_WORDBREAK);
    SelectObject(hdc, old);
    DeleteObject(font);
}

static void DrawButton(HDC hdc, const RECT& rc, const std::wstring& label) {
    DrawGradient(hdc, rc, RGB(250, 252, 255), RGB(218, 227, 240));
    HPEN p = CreatePen(PS_SOLID, 1, RGB(144, 160, 181));
    HPEN oldP = (HPEN)SelectObject(hdc, p);
    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(p);
    DrawCentered(hdc, rc, label, RGB(34, 34, 34), 14, false);
}

static void SeedApps() {
    g_apps = {
        {L"Notepad Pro", L"Productivity", L"Rich text notes and quick snippets", L"- Structured notes", L"- Quick memo workflow", L"- Pin important lines", L"Append line"},
        {L"File Explorer", L"System", L"Browse folders and inspect metadata", L"- Project directories", L"- Recent files", L"- Quick properties", L"Refresh list"},
        {L"Terminal", L"Developer", L"Command runner simulation with sessions", L"- Build logs", L"- Environment info", L"- Session state", L"Run command"},
        {L"Calculator", L"Utilities", L"Fast arithmetic helper", L"- Basic operations", L"- Memory register", L"- History preview", L"Add +7"},
        {L"Calendar", L"Productivity", L"Weekly planning and reminders", L"- Sprint meetings", L"- Deadlines", L"- Time blocks", L"Next day"},
        {L"Music Player", L"Media", L"Playlist and playback controls", L"- Chill mix", L"- Coding beats", L"- Focus mode", L"Next track"},
        {L"Video Player", L"Media", L"Timeline and playback status", L"- Demo clip", L"- Playback speed", L"- Subtitle toggle", L"+10 sec"},
        {L"Browser", L"Internet", L"Bookmarks and web shortcuts", L"- Docs", L"- Dashboard", L"- Search profile", L"Open tab"},
        {L"Settings", L"System", L"System preference center", L"- Appearance", L"- Privacy", L"- Notifications", L"Toggle mode"},
        {L"Task Manager", L"System", L"Process and resource overview", L"- CPU usage", L"- Memory graph", L"- Foreground apps", L"Sample update"},
        {L"Weather", L"Info", L"City forecast dashboard", L"- Istanbul", L"- Berlin", L"- Tokyo", L"Switch city"},
        {L"Clock", L"Utilities", L"World clocks and stopwatch", L"- Local clock", L"- UTC offset", L"- Timer state", L"Start/Stop"},
        {L"Paint", L"Creative", L"Color and sketch workspace", L"- Palette selection", L"- Brush mode", L"- Layer info", L"Change color"},
        {L"Mail", L"Communication", L"Inbox and message status", L"- Primary inbox", L"- Sent queue", L"- Draft sync", L"Compose"},
        {L"Game Center", L"Fun", L"Mini game launcher and stats", L"- Dice mode", L"- Arcade list", L"- Score tracker", L"Roll dice"}
    };
}

static void InitState() {
    SeedApps();
    std::srand((unsigned int)std::time(nullptr));
}

static void OpenAppWindow(int appIndex) {
    AppWindow w;
    int n = (int)g_windows.size();
    w.appIndex = appIndex;
    w.rect = MakeRect(120 + (n % 5) * 36, 90 + (n % 4) * 26, 760 + (n % 5) * 36, 520 + (n % 4) * 26);
    w.metric = 0;
    w.selection = 0;
    w.toggle = false;
    w.notes = L"";
    if (appIndex == 0) {
        w.notes = L"Welcome to Notepad Pro\r\n\r\nType here after focusing this window.";
    }
    g_windows.push_back(w);
}

static int HitWindow(int x, int y) {
    for (int i = (int)g_windows.size() - 1; i >= 0; --i) {
        if (PointInRectI(g_windows[i].rect, x, y)) return i;
    }
    return -1;
}

static void BringToFront(int index) {
    if (index < 0 || index >= (int)g_windows.size() || index == (int)g_windows.size() - 1) return;
    AppWindow w = g_windows[index];
    g_windows.erase(g_windows.begin() + index);
    g_windows.push_back(w);
}

static void ClampWindowToClient(AppWindow& w, const RECT& client) {
    int width = w.rect.right - w.rect.left;
    int height = w.rect.bottom - w.rect.top;
    int maxLeft = (client.right - width > 0) ? client.right - width : 0;
    int maxTop = (client.bottom - kTaskbarHeight - height > 0) ? client.bottom - kTaskbarHeight - height : 0;
    w.rect.left = ClampInt(w.rect.left, 0, maxLeft);
    w.rect.top = ClampInt(w.rect.top, 0, maxTop);
    w.rect.right = w.rect.left + width;
    w.rect.bottom = w.rect.top + height;
}

static void DrawDesktop(HDC hdc, const RECT& client) {
    DrawGradient(hdc, client, RGB(93, 151, 224), RGB(16, 71, 144));

    RECT leftPanel = MakeRect(8, 8, 126, client.bottom - kTaskbarHeight - 8);
    DrawGradient(hdc, leftPanel, RGB(42, 94, 162), RGB(24, 65, 122));

    for (int i = 0; i < 6; ++i) {
        RECT ic = MakeRect(18, 24 + i * 86, 116, 100 + i * 86);
        HBRUSH b = CreateSolidBrush(RGB(255, 255, 255));
        RECT glyph = MakeRect(ic.left + 30, ic.top + 2, ic.left + 64, ic.top + 36);
        FillRect(hdc, &glyph, b);
        DeleteObject(b);
        DrawCentered(hdc, MakeRect(ic.left + 2, ic.top + 40, ic.right - 2, ic.bottom), g_apps[i].name, RGB(240, 247, 255), 13, false);
    }
}

static void DrawWindow(HDC hdc, const AppWindow& w) {
    const AppDef& app = g_apps[w.appIndex];

    HBRUSH body = CreateSolidBrush(RGB(248, 251, 255));
    FillRect(hdc, &w.rect, body);
    DeleteObject(body);

    RECT title = WindowTitleRect(w);
    DrawGradient(hdc, title, RGB(108, 170, 232), RGB(62, 128, 201));

    RECT closeR = WindowCloseRect(w);
    HBRUSH closeB = CreateSolidBrush(RGB(196, 40, 28));
    FillRect(hdc, &closeR, closeB);
    DeleteObject(closeB);

    DrawCentered(hdc, MakeRect(w.rect.left + 10, w.rect.top, w.rect.right - 50, w.rect.top + 34), app.name + L"  •  " + app.category, RGB(255, 255, 255), 17, true);
    DrawCentered(hdc, closeR, L"X", RGB(255, 255, 255), 16, true);

    HPEN border = CreatePen(PS_SOLID, 1, RGB(36, 90, 154));
    HPEN oldP = (HPEN)SelectObject(hdc, border);
    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, w.rect.left, w.rect.top, w.rect.right, w.rect.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(border);

    DrawButton(hdc, WindowActionButtonRect(w), app.actionLabel + L" (" + std::to_wstring(w.metric) + L")");
    DrawButton(hdc, WindowToggleButtonRect(w), std::wstring(L"Mode: ") + (w.toggle ? L"ON" : L"OFF"));
    DrawButton(hdc, WindowSelectButtonRect(w), L"Variant " + std::to_wstring(w.selection + 1));

    RECT content = WindowContentRect(w);
    HBRUSH contentB = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &content, contentB);
    DeleteObject(contentB);

    HPEN contentP = CreatePen(PS_SOLID, 1, RGB(187, 198, 216));
    HPEN oldCp = (HPEN)SelectObject(hdc, contentP);
    HGDIOBJ oldCb = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, content.left, content.top, content.right, content.bottom);
    SelectObject(hdc, oldCb);
    SelectObject(hdc, oldCp);
    DeleteObject(contentP);

    std::wstring summary = app.summary + L"\r\n\r\n" + app.line1 + L"\r\n" + app.line2 + L"\r\n" + app.line3 +
        L"\r\n\r\nStatus Metrics\r\n- Action Count: " + std::to_wstring(w.metric) +
        L"\r\n- Mode: " + (w.toggle ? std::wstring(L"Enabled") : std::wstring(L"Disabled")) +
        L"\r\n- Variant Index: " + std::to_wstring(w.selection + 1);

    if (w.appIndex == 0) {
        summary += L"\r\n\r\nNotes:\r\n" + w.notes;
    }

    if (w.appIndex == 14) {
        summary += L"\r\n\r\nLast Dice: " + std::to_wstring((w.metric % 6) + 1);
    }

    DrawBlock(hdc, MakeRect(content.left + 10, content.top + 8, content.right - 10, content.bottom - 8), summary, RGB(25, 25, 25), 15, false);
}

static void DrawTaskbar(HDC hdc, const RECT& client) {
    RECT taskbar = MakeRect(0, client.bottom - kTaskbarHeight, client.right, client.bottom);
    DrawGradient(hdc, taskbar, RGB(58, 111, 179), RGB(36, 78, 134));

    RECT start = StartButtonRect(client);
    DrawGradient(hdc, start, RGB(98, 166, 58), RGB(59, 114, 29));
    DrawCentered(hdc, start, L"Start", RGB(255, 255, 255), 18, true);

    int x = kStartButtonWidth + 6;
    for (int i = 0; i < (int)g_windows.size() && i < 8; ++i) {
        RECT b = MakeRect(x, client.bottom - kTaskbarHeight + 6, x + 130, client.bottom - 6);
        DrawGradient(hdc, b, RGB(112, 164, 221), RGB(70, 118, 180));
        DrawCentered(hdc, b, g_apps[g_windows[i].appIndex].name, RGB(255, 255, 255), 13, false);
        x += 134;
    }

    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t clockBuf[20];
    wsprintfW(clockBuf, L"%02d:%02d", tm.tm_hour, tm.tm_min);
    DrawCentered(hdc, MakeRect(client.right - 90, client.bottom - kTaskbarHeight + 2, client.right - 8, client.bottom - 4), clockBuf, RGB(238, 246, 255), 16, true);
}

static void DrawStartMenu(HDC hdc, const RECT& client) {
    if (!g_startMenuOpen) return;
    RECT m = StartMenuRect(client);
    DrawGradient(hdc, m, RGB(233, 241, 251), RGB(189, 216, 244));
    RECT head = MakeRect(m.left, m.top, m.right, m.top + 48);
    DrawGradient(hdc, head, RGB(90, 152, 222), RGB(52, 110, 181));
    DrawCentered(hdc, head, L"C-OS Apps (15)", RGB(255, 255, 255), 19, true);

    for (int i = 0; i < (int)g_apps.size(); ++i) {
        RECT item = StartMenuItemRect(client, i);
        HBRUSH b = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &item, b);
        DeleteObject(b);

        DrawBlock(hdc, MakeRect(item.left + 8, item.top + 4, item.right - 8, item.bottom - 3), g_apps[i].name + L"  -  " + g_apps[i].category, RGB(40, 40, 40), 13, false);
    }

    HPEN p = CreatePen(PS_SOLID, 1, RGB(72, 125, 193));
    HPEN oldP = (HPEN)SelectObject(hdc, p);
    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, m.left, m.top, m.right, m.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(p);
}

static void PaintScene(HWND hwnd, HDC hdc) {
    RECT client;
    GetClientRect(hwnd, &client);

    DrawDesktop(hdc, client);
    DrawTaskbar(hdc, client);
    DrawStartMenu(hdc, client);

    for (const auto& w : g_windows) {
        DrawWindow(hdc, w);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitState();
        SetTimer(hwnd, 1, 1000, nullptr);
        return 0;

    case WM_TIMER:
        if (!g_windows.empty()) {
            for (auto& w : g_windows) {
                if (w.appIndex == 11 && w.toggle) w.metric += 1;
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        RECT client;
        GetClientRect(hwnd, &client);

        if (PointInRectI(StartButtonRect(client), x, y)) {
            g_startMenuOpen = !g_startMenuOpen;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (g_startMenuOpen) {
            for (int i = 0; i < (int)g_apps.size(); ++i) {
                if (PointInRectI(StartMenuItemRect(client, i), x, y)) {
                    OpenAppWindow(i);
                    g_startMenuOpen = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
        }

        int hit = HitWindow(x, y);
        if (hit >= 0) {
            BringToFront(hit);
            int top = (int)g_windows.size() - 1;
            AppWindow& w = g_windows[top];

            if (PointInRectI(WindowCloseRect(w), x, y)) {
                g_windows.pop_back();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (PointInRectI(WindowActionButtonRect(w), x, y)) {
                if (w.appIndex == 14) {
                    w.metric = std::rand() % 6 + 1;
                } else {
                    w.metric += 1;
                }
                if (w.appIndex == 0) {
                    w.notes += L"\r\n- Quick line #" + std::to_wstring(w.metric);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (PointInRectI(WindowToggleButtonRect(w), x, y)) {
                w.toggle = !w.toggle;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (PointInRectI(WindowSelectButtonRect(w), x, y)) {
                w.selection = (w.selection + 1) % 4;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (PointInRectI(WindowTitleRect(w), x, y)) {
                g_dragging = true;
                g_draggingWindow = top;
                g_dragOffset.x = x - w.rect.left;
                g_dragOffset.y = y - w.rect.top;
                SetCapture(hwnd);
                return 0;
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (x >= 16 && x <= 116 && y >= 24 && y <= 540) {
            int iconIndex = (y - 24) / 86;
            if (iconIndex >= 0 && iconIndex < 6) {
                OpenAppWindow(iconIndex);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        g_startMenuOpen = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_dragging && g_draggingWindow >= 0 && g_draggingWindow < (int)g_windows.size()) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            AppWindow& w = g_windows[g_draggingWindow];
            int width = w.rect.right - w.rect.left;
            int height = w.rect.bottom - w.rect.top;
            w.rect.left = x - g_dragOffset.x;
            w.rect.top = y - g_dragOffset.y;
            w.rect.right = w.rect.left + width;
            w.rect.bottom = w.rect.top + height;
            RECT client;
            GetClientRect(hwnd, &client);
            ClampWindowToClient(w, client);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_dragging) {
            g_dragging = false;
            g_draggingWindow = -1;
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
        1366, 768,
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
