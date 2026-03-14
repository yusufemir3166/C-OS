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
    std::wstring actionLabel;
    wchar_t iconGlyph;
    COLORREF iconColor;
};

struct AppWindow {
    RECT rect{};
    int appIndex = 0;
    int metric = 0;
    int selection = 0;
    bool toggle = false;
    std::wstring memo;
};

static const int kTaskbarHeight = 44;
static const int kStartButtonWidth = 96;
static const int kMenuWidth = 360;
static const int kMenuHeight = 560;

static bool g_startMenuOpen = false;
static bool g_dragging = false;
static int g_draggingWindow = -1;
static POINT g_dragOffset{};

static std::vector<AppDef> g_apps;
static std::vector<AppWindow> g_windows;

static bool g_setupMode = true;
static bool g_langTR = true;
static std::wstring g_setupUsername;
static std::wstring g_setupPassword;
static bool g_setupShowPassword = false;
// 0: none, 1: username, 2: password
static int g_setupFocusField = 0;

static int ClampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(v, hi));
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

static RECT DesktopIconRect(int index) {
    int col = index % 2;
    int row = index / 2;
    int left = 14 + col * 92;
    int top = 20 + row * 82;
    return MakeRect(left, top, left + 86, top + 74);
}

static RECT SetupPanelRect(const RECT& client) {
    int w = 780;
    int h = 460;
    int l = (client.right - w) / 2;
    int t = (client.bottom - h) / 2;
    return MakeRect(l, t, l + w, t + h);
}

static RECT SetupLangTRRect(const RECT& client) {
    RECT p = SetupPanelRect(client);
    return MakeRect(p.left + 40, p.top + 112, p.left + 210, p.top + 152);
}

static RECT SetupLangENRect(const RECT& client) {
    RECT p = SetupPanelRect(client);
    return MakeRect(p.left + 220, p.top + 112, p.left + 390, p.top + 152);
}

static RECT SetupUserRect(const RECT& client) {
    RECT p = SetupPanelRect(client);
    return MakeRect(p.left + 40, p.top + 200, p.right - 40, p.top + 244);
}

static RECT SetupPassRect(const RECT& client) {
    RECT p = SetupPanelRect(client);
    return MakeRect(p.left + 40, p.top + 278, p.right - 40, p.top + 322);
}

static RECT SetupShowPassRect(const RECT& client) {
    RECT p = SetupPanelRect(client);
    return MakeRect(p.left + 40, p.top + 332, p.left + 250, p.top + 362);
}

static RECT SetupContinueRect(const RECT& client) {
    RECT p = SetupPanelRect(client);
    return MakeRect(p.right - 230, p.bottom - 70, p.right - 40, p.bottom - 30);
}

static RECT WindowTitleRect(const AppWindow& w) { return MakeRect(w.rect.left, w.rect.top, w.rect.right, w.rect.top + 34); }
static RECT WindowCloseRect(const AppWindow& w) { return MakeRect(w.rect.right - 42, w.rect.top + 5, w.rect.right - 10, w.rect.top + 29); }
static RECT WindowActionButtonRect(const AppWindow& w) { return MakeRect(w.rect.left + 14, w.rect.top + 106, w.rect.left + 220, w.rect.top + 136); }
static RECT WindowToggleButtonRect(const AppWindow& w) { return MakeRect(w.rect.left + 228, w.rect.top + 106, w.rect.left + 378, w.rect.top + 136); }
static RECT WindowSelectButtonRect(const AppWindow& w) { return MakeRect(w.rect.left + 386, w.rect.top + 106, w.rect.left + 560, w.rect.top + 136); }
static RECT WindowContentRect(const AppWindow& w) { return MakeRect(w.rect.left + 14, w.rect.top + 146, w.rect.right - 14, w.rect.bottom - 14); }

static void DrawGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX v[2] = {
        { rc.left, rc.top, (COLOR16)(GetRValue(top) << 8), (COLOR16)(GetGValue(top) << 8), (COLOR16)(GetBValue(top) << 8), 0x0000 },
        { rc.right, rc.bottom, (COLOR16)(GetRValue(bottom) << 8), (COLOR16)(GetGValue(bottom) << 8), (COLOR16)(GetBValue(bottom) << 8), 0x0000 }
    };
    GRADIENT_RECT gRect = { 0, 1 };
    GradientFill(hdc, v, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
}

static HFONT CreateUIFont(int h, bool bold = false) {
    return CreateFontW(h, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static void DrawCentered(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, int height, bool bold = false) {
    HFONT font = CreateUIFont(height, bold);
    HFONT old = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT r = rc;
    DrawTextW(hdc, text.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);
}

static void DrawBlock(HDC hdc, const RECT& rc, const std::wstring& text, COLORREF color, int height, bool bold = false) {
    HFONT font = CreateUIFont(height, bold);
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
        {L"Notepad Pro", L"Productivity", L"Advanced rich notes and snippets", L"Append line", L'N', RGB(50, 130, 220)},
        {L"File Explorer", L"System", L"Folders, metadata and quick access", L"Refresh", L'F', RGB(248, 190, 54)},
        {L"Terminal", L"Developer", L"Command sessions and logs", L"Run cmd", L'T', RGB(56, 56, 58)},
        {L"Calculator", L"Utilities", L"Quick arithmetic helper", L"+7", L'C', RGB(76, 167, 90)},
        {L"Calendar", L"Productivity", L"Weekly planner and reminders", L"Next day", L'K', RGB(123, 93, 230)},
        {L"Music Player", L"Media", L"Playlist and playback controls", L"Next track", L'M', RGB(237, 80, 115)},
        {L"Video Player", L"Media", L"Timeline, subtitle and speed", L"+10 sec", L'V', RGB(230, 116, 60)},
        {L"Browser", L"Internet", L"Tabs and bookmarks", L"Open tab", L'B', RGB(72, 153, 236)},
        {L"Settings", L"System", L"Preferences and personalization", L"Apply", L'S', RGB(105, 117, 130)},
        {L"Task Manager", L"System", L"Resource snapshot and process list", L"Sample", L'R', RGB(91, 160, 200)},
        {L"Weather", L"Info", L"Forecast and city switch", L"Switch city", L'W', RGB(66, 178, 231)},
        {L"Clock", L"Utilities", L"Time tools and stopwatch", L"Tick", L'O', RGB(85, 170, 116)},
        {L"Paint", L"Creative", L"Color and brush workspace", L"Paint +", L'P', RGB(224, 89, 142)},
        {L"Mail", L"Communication", L"Inbox and compose flow", L"Compose", L'A', RGB(87, 119, 228)},
        {L"Game Center", L"Fun", L"Mini game hub and scores", L"Roll dice", L'G', RGB(250, 168, 41)}
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
    w.rect = MakeRect(140 + (n % 5) * 26, 90 + (n % 4) * 24, 910 + (n % 5) * 26, 620 + (n % 4) * 24);
    w.metric = 0;
    w.selection = 0;
    w.toggle = false;
    if (appIndex == 0) {
        w.memo = L"Editable note timeline started.";
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
    int maxLeft = std::max(0, static_cast<int>(client.right) - width);
    int maxTop = std::max(0, static_cast<int>(client.bottom) - kTaskbarHeight - height);
    w.rect.left = ClampInt(w.rect.left, 0, maxLeft);
    w.rect.top = ClampInt(w.rect.top, 0, maxTop);
    w.rect.right = w.rect.left + width;
    w.rect.bottom = w.rect.top + height;
}

static void DrawDesktopIcon(HDC hdc, const RECT& rc, const AppDef& app) {
    RECT panel = rc;
    DrawGradient(hdc, panel, RGB(40, 93, 160), RGB(24, 64, 120));

    RECT glyph = MakeRect(rc.left + 27, rc.top + 8, rc.left + 59, rc.top + 40);
    HBRUSH gB = CreateSolidBrush(app.iconColor);
    FillRect(hdc, &glyph, gB);
    DeleteObject(gB);

    DrawCentered(hdc, glyph, std::wstring(1, app.iconGlyph), RGB(255, 255, 255), 16, true);
    DrawCentered(hdc, MakeRect(rc.left + 2, rc.top + 42, rc.right - 2, rc.bottom - 3), app.name, RGB(238, 247, 255), 12, false);
}


static void FillRectColor(HDC hdc, const RECT& rc, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(hdc, &rc, b);
    DeleteObject(b);
}

static void DrawPanel(HDC hdc, const RECT& rc, COLORREF bg, COLORREF border) {
    FillRectColor(hdc, rc, bg);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HPEN oldP = (HPEN)SelectObject(hdc, p);
    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(p);
}

static void DrawProgressBar(HDC hdc, const RECT& rc, int value, COLORREF fill, COLORREF bg) {
    DrawPanel(hdc, rc, bg, RGB(160, 170, 186));
    int width = rc.right - rc.left - 2;
    int clamped = ClampInt(value, 0, 100);
    RECT bar = MakeRect(rc.left + 1, rc.top + 1, rc.left + 1 + (width * clamped) / 100, rc.bottom - 1);
    FillRectColor(hdc, bar, fill);
}

static void DrawAppSpecificWidgets(HDC hdc, const AppWindow& w, const RECT& content) {
    const int i = w.appIndex;
    RECT left = MakeRect(content.left + 10, content.top + 10, content.left + 250, content.top + 120);
    RECT right = MakeRect(content.left + 262, content.top + 10, content.right - 10, content.top + 120);

    DrawPanel(hdc, left, RGB(246, 250, 255), RGB(170, 184, 205));
    DrawPanel(hdc, right, RGB(252, 252, 252), RGB(170, 184, 205));

    switch (i) {
    case 0: // Notepad
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"Recent Notes", RGB(40, 70, 120), 14, true);
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 30, left.right - 8, left.bottom - 8), L"- standup points\r\n- release checklist\r\n- quick memo", RGB(35, 35, 35), 13, false);
        DrawCentered(hdc, MakeRect(right.left, right.top + 4, right.right, right.top + 28), L"Words", RGB(40, 70, 120), 14, true);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 42, right.right - 10, right.top + 66), (int)(w.memo.size() % 101), RGB(66, 145, 230), RGB(233, 241, 252));
        break;
    case 1: // File Explorer
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"Folders", RGB(120, 92, 20), 14, true);
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 32, left.right - 8, left.bottom - 8), L"Projects\r\nDownloads\r\nDesktop\r\nArchives", RGB(35, 35, 35), 13, false);
        DrawCentered(hdc, MakeRect(right.left, right.top + 4, right.right, right.top + 28), L"Disk Usage", RGB(120, 92, 20), 14, true);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 44, right.right - 10, right.top + 68), (w.metric * 3) % 101, RGB(246, 177, 44), RGB(250, 243, 225));
        break;
    case 2: // Terminal
        DrawPanel(hdc, left, RGB(28, 31, 35), RGB(90, 98, 108));
        DrawPanel(hdc, right, RGB(28, 31, 35), RGB(90, 98, 108));
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 10, left.right - 8, left.bottom - 8), L"> build --release\r\n> run tests\r\n> deploy preview", RGB(110, 235, 139), 13, false);
        DrawBlock(hdc, MakeRect(right.left + 8, right.top + 10, right.right - 8, right.bottom - 8), L"session: alpha\r\nstatus: connected\r\nlatency: 19ms", RGB(151, 205, 255), 13, false);
        break;
    case 3: // Calculator
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"Display", RGB(22, 96, 57), 14, true);
        DrawCentered(hdc, MakeRect(left.left + 8, left.top + 36, left.right - 8, left.top + 78), std::to_wstring(w.metric), RGB(22, 96, 57), 24, true);
        DrawBlock(hdc, MakeRect(right.left + 8, right.top + 10, right.right - 8, right.bottom - 8), L"7 8 9 /\r\n4 5 6 *\r\n1 2 3 -\r\n0 . = +", RGB(35, 35, 35), 15, true);
        break;
    case 4: // Calendar
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"Week Board", RGB(90, 61, 170), 14, true);
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 32, left.right - 8, left.bottom - 8), L"Mon: Design\r\nTue: Build\r\nWed: Review", RGB(35, 35, 35), 13, false);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 44, right.right - 10, right.top + 68), (w.metric * 3) % 101, RGB(131, 102, 228), RGB(240, 235, 252));
        break;
    case 5: // Music
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"Now Playing", RGB(175, 35, 94), 14, true);
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 32, left.right - 8, left.bottom - 8), L"Focus Lo-Fi Mix\r\ntrack #" + std::to_wstring((w.metric % 12) + 1), RGB(35, 35, 35), 13, false);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 44, right.right - 10, right.top + 68), (w.metric * 5) % 101, RGB(226, 79, 133), RGB(252, 233, 241));
        break;
    case 6: // Video
        DrawPanel(hdc, left, RGB(20, 22, 26), RGB(86, 90, 99));
        DrawCentered(hdc, MakeRect(left.left, left.top + 38, left.right, left.bottom - 10), L"PLAYBACK", RGB(212, 220, 232), 18, true);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 44, right.right - 10, right.top + 68), w.metric % 101, RGB(240, 126, 71), RGB(250, 236, 225));
        break;
    case 7: // Browser
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 10, left.right - 8, left.bottom - 8), L"Tabs\r\n- Docs\r\n- Dashboard\r\n- Issues", RGB(35, 35, 35), 13, false);
        DrawBlock(hdc, MakeRect(right.left + 8, right.top + 10, right.right - 8, right.bottom - 8), L"Bookmarks\r\n- Portal\r\n- CI\r\n- Metrics", RGB(35, 35, 35), 13, false);
        break;
    case 8: // Settings
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 10, left.right - 8, left.bottom - 8), L"Theme: " + std::wstring(w.toggle ? L"Dark" : L"Light") + L"\r\nScale: 100%\r\nLanguage: TR", RGB(35, 35, 35), 13, false);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 44, right.right - 10, right.top + 68), (w.metric * 9) % 101, RGB(106, 118, 132), RGB(235, 240, 247));
        break;
    case 9: // Task manager
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"CPU", RGB(34, 98, 122), 14, true);
        DrawProgressBar(hdc, MakeRect(left.left + 10, left.top + 42, left.right - 10, left.top + 66), w.metric % 101, RGB(87, 176, 214), RGB(230, 245, 251));
        DrawCentered(hdc, MakeRect(right.left, right.top + 4, right.right, right.top + 28), L"Memory", RGB(34, 98, 122), 14, true);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 42, right.right - 10, right.top + 66), (w.metric + 18) % 101, RGB(117, 155, 220), RGB(234, 241, 252));
        break;
    case 10: // Weather
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"City", RGB(37, 109, 163), 14, true);
        DrawCentered(hdc, MakeRect(left.left + 8, left.top + 36, left.right - 8, left.top + 76), (w.selection % 2 == 0) ? L"Istanbul" : L"Berlin", RGB(37, 109, 163), 20, true);
        DrawCentered(hdc, MakeRect(right.left + 8, right.top + 36, right.right - 8, right.top + 76), std::to_wstring(15 + (w.metric % 15)) + L" C", RGB(37, 109, 163), 24, true);
        break;
    case 11: // Clock
        DrawCentered(hdc, MakeRect(left.left + 6, left.top + 24, left.right - 6, left.bottom - 6), std::to_wstring(w.metric), RGB(37, 120, 78), 28, true);
        DrawCentered(hdc, MakeRect(right.left + 6, right.top + 40, right.right - 6, right.bottom - 6), w.toggle ? L"RUNNING" : L"PAUSED", RGB(37, 120, 78), 16, true);
        break;
    case 12: // Paint
        DrawPanel(hdc, left, RGB(255, 246, 252), RGB(220, 170, 200));
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 8, left.right - 8, left.bottom - 8), L"Palette\r\n- Red\r\n- Blue\r\n- Green", RGB(76, 48, 66), 13, false);
        DrawPanel(hdc, right, RGB(250, 252, 255), RGB(170, 184, 205));
        for (int k = 0; k < 6; ++k) {
            RECT sw = MakeRect(right.left + 14 + k * 26, right.top + 46, right.left + 34 + k * 26, right.top + 66);
            FillRectColor(hdc, sw, RGB(50 + k * 30, 80 + k * 20, 180 - k * 20));
        }
        break;
    case 13: // Mail
        DrawBlock(hdc, MakeRect(left.left + 8, left.top + 10, left.right - 8, left.bottom - 8), L"Inbox\r\n- Release update\r\n- Team sync\r\n- Code review", RGB(35, 35, 35), 13, false);
        DrawBlock(hdc, MakeRect(right.left + 8, right.top + 10, right.right - 8, right.bottom - 8), L"Compose\r\nTo: team@cos.local\r\nSubject: Status", RGB(35, 35, 35), 13, false);
        break;
    case 14: // Game Center
        DrawCentered(hdc, MakeRect(left.left, left.top + 4, left.right, left.top + 28), L"Dice", RGB(140, 92, 16), 14, true);
        DrawCentered(hdc, MakeRect(left.left + 8, left.top + 36, left.right - 8, left.top + 82), std::to_wstring((w.metric % 6) + 1), RGB(140, 92, 16), 28, true);
        DrawProgressBar(hdc, MakeRect(right.left + 10, right.top + 44, right.right - 10, right.top + 68), (w.metric * 17) % 101, RGB(247, 170, 50), RGB(251, 240, 218));
        break;
    default:
        break;
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

    DrawCentered(hdc, MakeRect(w.rect.left + 10, w.rect.top, w.rect.right - 50, w.rect.top + 34), app.name + L"  |  " + app.category, RGB(255, 255, 255), 17, true);
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
    DrawPanel(hdc, content, RGB(255, 255, 255), RGB(187, 198, 216));

    DrawAppSpecificWidgets(hdc, w, content);

}

static void DrawDesktop(HDC hdc, const RECT& client) {
    DrawGradient(hdc, client, RGB(93, 151, 224), RGB(16, 71, 144));

    for (int i = 0; i < 15; ++i) {
        DrawDesktopIcon(hdc, DesktopIconRect(i), g_apps[i]);
    }
}

static void DrawTaskbar(HDC hdc, const RECT& client) {
    RECT taskbar = MakeRect(0, client.bottom - kTaskbarHeight, client.right, client.bottom);
    DrawGradient(hdc, taskbar, RGB(58, 111, 179), RGB(36, 78, 134));

    RECT start = StartButtonRect(client);
    DrawGradient(hdc, start, RGB(98, 166, 58), RGB(59, 114, 29));
    DrawCentered(hdc, start, L"Start", RGB(255, 255, 255), 18, true);

    int x = kStartButtonWidth + 6;
    for (int i = std::max(0, (int)g_windows.size() - 8); i < (int)g_windows.size(); ++i) {
        RECT b = MakeRect(x, client.bottom - kTaskbarHeight + 6, x + 120, client.bottom - 6);
        DrawGradient(hdc, b, RGB(112, 164, 221), RGB(70, 118, 180));
        DrawCentered(hdc, b, g_apps[g_windows[i].appIndex].name, RGB(255, 255, 255), 12, false);
        x += 124;
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

        RECT icon = MakeRect(item.left + 4, item.top + 4, item.left + 22, item.bottom - 4);
        HBRUSH ib = CreateSolidBrush(g_apps[i].iconColor);
        FillRect(hdc, &icon, ib);
        DeleteObject(ib);

        DrawCentered(hdc, icon, std::wstring(1, g_apps[i].iconGlyph), RGB(255, 255, 255), 12, true);
        DrawBlock(hdc, MakeRect(item.left + 28, item.top + 4, item.right - 8, item.bottom - 3), g_apps[i].name + L"  -  " + g_apps[i].category, RGB(40, 40, 40), 13, false);
    }

    HPEN p = CreatePen(PS_SOLID, 1, RGB(72, 125, 193));
    HPEN oldP = (HPEN)SelectObject(hdc, p);
    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, m.left, m.top, m.right, m.bottom);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(p);
}

static std::wstring MaskPassword(const std::wstring& p) {
    return std::wstring(p.size(), L'*');
}

static void DrawSetupWizard(HDC hdc, const RECT& client) {
    DrawGradient(hdc, client, RGB(69, 120, 197), RGB(29, 66, 130));

    RECT panel = SetupPanelRect(client);
    DrawPanel(hdc, panel, RGB(247, 250, 255), RGB(103, 132, 178));

    RECT head = MakeRect(panel.left, panel.top, panel.right, panel.top + 64);
    DrawGradient(hdc, head, RGB(97, 156, 228), RGB(61, 117, 191));

    DrawCentered(hdc, MakeRect(panel.left + 20, panel.top + 8, panel.right - 20, panel.top + 36),
        g_langTR ? L"C-OS Kurulum Sihirbazi" : L"C-OS Setup Wizard", RGB(255, 255, 255), 24, true);
    DrawCentered(hdc, MakeRect(panel.left + 20, panel.top + 34, panel.right - 20, panel.top + 60),
        g_langTR ? L"Dil secin, kullanici adi ve sifre olusturun" : L"Choose language, create username and password", RGB(228, 240, 255), 14, false);

    RECT tr = SetupLangTRRect(client);
    RECT en = SetupLangENRect(client);
    DrawButton(hdc, tr, L"Turkce");
    DrawButton(hdc, en, L"English");

    if (g_langTR) {
        DrawPanel(hdc, tr, RGB(215, 234, 255), RGB(74, 125, 204));
        DrawCentered(hdc, tr, L"Turkce", RGB(22, 69, 143), 14, true);
    } else {
        DrawPanel(hdc, en, RGB(215, 234, 255), RGB(74, 125, 204));
        DrawCentered(hdc, en, L"English", RGB(22, 69, 143), 14, true);
    }

    RECT user = SetupUserRect(client);
    RECT pass = SetupPassRect(client);
    DrawPanel(hdc, user, RGB(255, 255, 255), g_setupFocusField == 1 ? RGB(64, 126, 211) : RGB(170, 182, 199));
    DrawPanel(hdc, pass, RGB(255, 255, 255), g_setupFocusField == 2 ? RGB(64, 126, 211) : RGB(170, 182, 199));

    DrawBlock(hdc, MakeRect(user.left + 10, user.top + 12, user.right - 10, user.bottom - 10),
        (g_langTR ? L"Kullanici adi: " : L"Username: ") + g_setupUsername, RGB(26, 26, 26), 16, false);

    std::wstring shownPass = g_setupShowPassword ? g_setupPassword : MaskPassword(g_setupPassword);
    DrawBlock(hdc, MakeRect(pass.left + 10, pass.top + 12, pass.right - 10, pass.bottom - 10),
        (g_langTR ? L"Sifre: " : L"Password: ") + shownPass, RGB(26, 26, 26), 16, false);

    RECT showPass = SetupShowPassRect(client);
    DrawButton(hdc, showPass, g_setupShowPassword
        ? (g_langTR ? L"Sifreyi gizle" : L"Hide password")
        : (g_langTR ? L"Sifreyi goster" : L"Show password"));

    RECT cont = SetupContinueRect(client);
    bool ready = g_setupUsername.size() >= 3 && g_setupPassword.size() >= 3;
    DrawPanel(hdc, cont, ready ? RGB(208, 234, 197) : RGB(235, 238, 244), ready ? RGB(80, 150, 72) : RGB(160, 170, 184));
    DrawCentered(hdc, cont, g_langTR ? L"Kurulumu Tamamla" : L"Finish Setup", ready ? RGB(39, 97, 36) : RGB(84, 95, 112), 15, true);

    std::wstring tips = g_langTR
        ? L"Ipuclari:\r\n- En az 3 karakter kullanin\r\n- TAB ile alan degistirebilirsiniz\r\n- ENTER ile kurulumu tamamlayabilirsiniz"
        : L"Tips:\r\n- Use at least 3 characters\r\n- Press TAB to switch fields\r\n- Press ENTER to finish setup";
    DrawBlock(hdc, MakeRect(panel.left + 40, panel.bottom - 118, panel.right - 260, panel.bottom - 20), tips, RGB(47, 61, 86), 13, false);
}

static void PaintScene(HWND hwnd, HDC hdc) {
    RECT client;
    GetClientRect(hwnd, &client);

    if (g_setupMode) {
        DrawSetupWizard(hdc, client);
        return;
    }

    DrawDesktop(hdc, client);
    DrawTaskbar(hdc, client);
    DrawStartMenu(hdc, client);

    for (const auto& w : g_windows) DrawWindow(hdc, w);
}

static void ApplyAppAction(AppWindow& w) {
    switch (w.appIndex) {
    case 0:
        w.metric += 1;
        w.memo += L"\r\n- note line #" + std::to_wstring(w.metric);
        break;
    case 1:
        w.metric = (w.metric + 3) % 100;
        break;
    case 2:
        w.metric += 2;
        break;
    case 3:
        w.metric += 7;
        break;
    case 4:
        w.metric = (w.metric + 1) % 31;
        break;
    case 5:
        w.metric = (w.metric + 1) % 42;
        break;
    case 6:
        w.metric += 10;
        break;
    case 7:
        w.metric += 1;
        break;
    case 8:
        w.toggle = !w.toggle;
        w.metric += 1;
        break;
    case 9:
        w.metric = 40 + (std::rand() % 55);
        break;
    case 10:
        w.selection = (w.selection + 1) % 4;
        w.metric = 15 + (std::rand() % 18);
        break;
    case 11:
        w.metric += 1;
        break;
    case 12:
        w.metric += 5;
        break;
    case 13:
        w.metric += 1;
        break;
    case 14:
        w.metric = std::rand() % 6 + 1;
        break;
    default:
        w.metric += 1;
        break;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitState();
        SetTimer(hwnd, 1, 1000, nullptr);
        return 0;

    case WM_TIMER:
        for (auto& w : g_windows) {
            if (w.appIndex == 11 && w.toggle) w.metric += 1;
            if (w.appIndex == 5 && w.toggle) w.metric = (w.metric + 1) % 42;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        RECT client;
        GetClientRect(hwnd, &client);

        if (g_setupMode) {
            if (PointInRectI(SetupLangTRRect(client), x, y)) {
                g_langTR = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRectI(SetupLangENRect(client), x, y)) {
                g_langTR = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRectI(SetupUserRect(client), x, y)) {
                g_setupFocusField = 1;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRectI(SetupPassRect(client), x, y)) {
                g_setupFocusField = 2;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRectI(SetupShowPassRect(client), x, y)) {
                g_setupShowPassword = !g_setupShowPassword;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (PointInRectI(SetupContinueRect(client), x, y) && g_setupUsername.size() >= 3 && g_setupPassword.size() >= 3) {
                g_setupMode = false;
                g_setupFocusField = 0;
                OpenAppWindow(8); // settings
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }

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

        for (int i = 0; i < 15; ++i) {
            if (PointInRectI(DesktopIconRect(i), x, y)) {
                OpenAppWindow(i);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
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
                ApplyAppAction(w);
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

        g_startMenuOpen = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }


    case WM_CHAR:
        if (g_setupMode) {
            if (wParam == VK_TAB) {
                g_setupFocusField = (g_setupFocusField == 1) ? 2 : 1;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (wParam == VK_RETURN) {
                if (g_setupUsername.size() >= 3 && g_setupPassword.size() >= 3) {
                    g_setupMode = false;
                    g_setupFocusField = 0;
                    OpenAppWindow(8);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (g_setupFocusField == 0) return 0;
            std::wstring* target = (g_setupFocusField == 1) ? &g_setupUsername : &g_setupPassword;
            if (wParam == VK_BACK) {
                if (!target->empty()) target->pop_back();
            } else if (wParam >= 32 && wParam <= 126) {
                if (target->size() < 24) target->push_back((wchar_t)wParam);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

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
        1440, 820,
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
