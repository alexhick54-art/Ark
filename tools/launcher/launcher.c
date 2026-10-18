// Ark Launcher (Windows): pick Ark's options and start the game. The choices are kept in ark_launcher.ini next to the
// launcher; the game runs from the folder above build\ (where ark.ini and the game folder are), its log in ark.log.
// Build: make launcher (MSYS2 UCRT64) -> build/ArkLauncher.exe
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

enum {
    ID_GAME = 100, ID_GAME_BROWSE, ID_LOG, ID_60FPS, ID_LOD, ID_SHADOWS, ID_SCALE, ID_ANISO, ID_BIAS, ID_MODS,
    ID_MODS_BROWSE, ID_CONFIG, ID_EXTRA, ID_LAUNCH, ID_OPENLOG, ID_OPENDIR, ID_STATUS, ID_PRESET_DEFAULT
};

static HWND wnd, h_game, h_log, h_60, h_lod, h_shadows, h_scale, h_aniso, h_bias, h_mods, h_config, h_extra, h_status;
static HFONT font, font_bold;
static int dpi = 96;
static wchar_t root[MAX_PATH], ini[MAX_PATH], exe_path[MAX_PATH];

static const wchar_t *LOD_ITEMS[] = {L"Game default", L"1 - community LoD values", L"2 - plus terrain streaming further out",
                                      L"3 - maximum (all models full detail, grass to its limit)"};
static const wchar_t *SHADOW_ITEMS[] = {L"Game default (1024)", L"2x (2048)", L"3x (3072)", L"4x (4096)",
                                         L"2x + tighter cascades (experimental)"};
static const wchar_t *SCALE_ITEMS[] = {L"1", L"1.5", L"2", L"0.75", L"0.5"};
static const wchar_t *ANISO_ITEMS[] = {L"Off", L"2x", L"4x", L"8x", L"16x"};
static const wchar_t *BIAS_ITEMS[] = {L"0", L"-0.5", L"-1"};
static const wchar_t *LOD0_ITEMS[] = {L"With draw distance 3", L"Always", L"Never"};

// right column: on/off options, each one environment variable (or a bit of ARK_60FPS_SITES)
typedef struct { const wchar_t *label, *key, *var, *val; int bit, def; HWND h; } Check;
static Check checks[] = {
    {L"Timers and movement steps", L"g1", NULL, NULL, 1, 1},
    {L"Fixed-step fallbacks (physics)", L"g2", NULL, NULL, 2, 1},
    {L"Objects' default time step", L"g4", NULL, NULL, 4, 1},
    {L"Time steps passed as arguments", L"g8", NULL, NULL, 8, 1},
    {L"Dialogue text at the 30 fps pace", L"g64", NULL, NULL, 64, 1},
    {L"Halve the second frame table (old behaviour)", L"table2", L"ARK_60FPS_TABLE2", L"1", 0, 0},
    {L"Save translated shaders (build\\glsl)", L"savemsl", L"LARECOMP_SAVE_MSL", L"1", 0, 0},
    {L"F9 frame dumps include buffer data", L"dbgdata", L"LARECOMP_DEBUG_DATA", L"1", 0, 0},
    {L"Thread CPU profile in the log (every 2 s)", L"profsvc", L"LARECOMP_PROF_SVC", L"1", 0, 0},
    {L"Hottest game functions in the log (every 10 s)", L"sample", L"LARECOMP_SAMPLE", L"10", 0, 0},
    {L"Log every file the game opens", L"logopen", L"ARK_LOG_OPEN", L"1", 0, 0},
};
static HWND h_lod0;

static int S(int v) { return MulDiv(v, dpi, 96); }

static HWND add(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(!wcscmp(cls, L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y),
                             S(w), S(h), wnd, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}
static HWND label(const wchar_t *text, int x, int y, int w) { return add(L"STATIC", text, 0, x, y + 3, w, 20, 0); }
static HWND heading_at(const wchar_t *text, int x, int y) {
    HWND h = add(L"STATIC", text, 0, x, y, 330, 20, 0);
    SendMessageW(h, WM_SETFONT, (WPARAM)font_bold, TRUE);
    return h;
}
static HWND heading(const wchar_t *text, int y) {
    HWND h = add(L"STATIC", text, 0, 16, y, 560, 20, 0);
    SendMessageW(h, WM_SETFONT, (WPARAM)font_bold, TRUE);
    return h;
}
static HWND combo(const wchar_t *const *items, int n, int x, int y, int w, int id, bool editable) {
    HWND c = add(L"COMBOBOX", L"", (editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST) | WS_VSCROLL | WS_TABSTOP, x, y, w, 200, id);
    for (int i = 0; i < n; i++) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageW(c, CB_SETCURSEL, 0, 0);
    return c;
}
#define N(a) (int)(sizeof a / sizeof *a)

// ---- settings (ark_launcher.ini)
static void get_text(HWND h, wchar_t *buf, int n) { GetWindowTextW(h, buf, n); }
static int sel(HWND h) { return (int)SendMessageW(h, CB_GETCURSEL, 0, 0); }
static void save_int(const wchar_t *k, int v) { wchar_t b[16]; swprintf(b, 16, L"%d", v); WritePrivateProfileStringW(L"ark", k, b, ini); }
static void save_text(const wchar_t *k, HWND h) {
    wchar_t b[4096];
    get_text(h, b, 4096);
    for (wchar_t *p = b; *p; p++) if (*p == L'\r') *p = L'\x01'; else if (*p == L'\n') *p = L'\x02';  // (one ini line)
    WritePrivateProfileStringW(L"ark", k, b, ini);
}
static void load_text(const wchar_t *k, HWND h, const wchar_t *def) {
    wchar_t b[4096];
    GetPrivateProfileStringW(L"ark", k, def, b, 4096, ini);
    for (wchar_t *p = b; *p; p++) if (*p == L'\x01') *p = L'\r'; else if (*p == L'\x02') *p = L'\n';
    SetWindowTextW(h, b);
}
static void save_all(void) {
    save_text(L"game", h_game);
    save_int(L"log", (int)SendMessageW(h_log, BM_GETCHECK, 0, 0));
    save_int(L"fps60", (int)SendMessageW(h_60, BM_GETCHECK, 0, 0));
    save_int(L"lod", sel(h_lod));
    save_int(L"shadows", sel(h_shadows));
    save_text(L"scale", h_scale);
    save_int(L"aniso", sel(h_aniso));
    save_text(L"bias", h_bias);
    save_text(L"mods", h_mods);
    save_text(L"config", h_config);
    save_text(L"extra", h_extra);
    save_int(L"lod0", sel(h_lod0));
    for (int i = 0; i < N(checks); i++) save_int(checks[i].key, (int)SendMessageW(checks[i].h, BM_GETCHECK, 0, 0));
}
static void load_all(void) {
    load_text(L"game", h_game, L"game");
    SendMessageW(h_log, BM_SETCHECK, GetPrivateProfileIntW(L"ark", L"log", 1, ini), 0);
    SendMessageW(h_60, BM_SETCHECK, GetPrivateProfileIntW(L"ark", L"fps60", 1, ini), 0);
    SendMessageW(h_lod, CB_SETCURSEL, GetPrivateProfileIntW(L"ark", L"lod", 3, ini), 0);
    SendMessageW(h_shadows, CB_SETCURSEL, GetPrivateProfileIntW(L"ark", L"shadows", 1, ini), 0);
    load_text(L"scale", h_scale, L"1");
    SendMessageW(h_aniso, CB_SETCURSEL, GetPrivateProfileIntW(L"ark", L"aniso", 4, ini), 0);
    load_text(L"bias", h_bias, L"0");
    load_text(L"mods", h_mods, L"");
    load_text(L"config", h_config, L"");
    load_text(L"extra", h_extra, L"");
    SendMessageW(h_lod0, CB_SETCURSEL, GetPrivateProfileIntW(L"ark", L"lod0", 0, ini), 0);
    for (int i = 0; i < N(checks); i++)
        SendMessageW(checks[i].h, BM_SETCHECK, GetPrivateProfileIntW(L"ark", checks[i].key, checks[i].def, ini), 0);
}

// ---- launching
static void env(const wchar_t *k, const wchar_t *v) { SetEnvironmentVariableW(k, v && *v ? v : NULL); }
static void status(const wchar_t *fmt, const wchar_t *arg) {
    wchar_t b[512];
    swprintf(b, 512, fmt, arg);
    SetWindowTextW(h_status, b);
}

static DWORD WINAPI watch_game(void *proc) {
    WaitForSingleObject(proc, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(proc, &code);
    CloseHandle(proc);
    PostMessageW(wnd, WM_APP, code, 0);
    return 0;
}

static void launch(void) {
    save_all();
    wchar_t game[MAX_PATH], b[4096];
    get_text(h_game, game, MAX_PATH);
    // every option Ark reads is set or cleared here, so the launcher's choices are the ones in force
    env(L"ARK_60FPS", SendMessageW(h_60, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : NULL);
    int lod = sel(h_lod);
    swprintf(b, 16, L"%d", lod);
    env(L"ARK_LOD", lod > 0 ? b : NULL);
    int sh = sel(h_shadows);
    env(L"ARK_SHADOWS", sh == 0 ? NULL : sh == 4 ? L"2" : L"1");
    swprintf(b, 16, L"%d", sh == 4 ? 2 : sh + 1);
    env(L"ARK_SHADOW_RES", sh >= 1 && sh <= 3 ? b : NULL);
    get_text(h_scale, b, 64);
    env(L"LARECOMP_SCALE", wcscmp(b, L"1") ? b : NULL);
    static const wchar_t *aniso[] = {NULL, L"2", L"4", L"8", L"16"};
    env(L"ARK_ANISO", aniso[sel(h_aniso) < 0 ? 0 : sel(h_aniso)]);
    get_text(h_bias, b, 64);
    env(L"ARK_TEX_LOD_BIAS", wcscmp(b, L"0") ? b : NULL);
    get_text(h_mods, b, MAX_PATH);
    env(L"ARK_MODS", b);
    int l0 = sel(h_lod0);
    env(L"ARK_LOD0", l0 == 1 ? L"1" : l0 == 2 ? L"0" : NULL);
    int mask = 0, all = 0;
    for (int i = 0; i < N(checks); i++) {
        bool on = SendMessageW(checks[i].h, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (checks[i].bit) { all |= checks[i].bit; if (on) mask |= checks[i].bit; }
        else env(checks[i].var, on ? checks[i].val : NULL);
    }
    swprintf(b, 16, L"%d", mask);
    env(L"ARK_60FPS_SITES", mask != all ? b : NULL);
    get_text(h_config, b, 4096);
    env(L"ARK_CONFIG", b);
    // extra variables, one KEY=VALUE per line (test options)
    get_text(h_extra, b, 4096);
    for (wchar_t *line = b, *next; line && *line; line = next) {
        next = wcspbrk(line, L"\r\n");
        if (next) { *next++ = 0; while (*next == L'\r' || *next == L'\n') next++; }
        while (*line == L' ' || *line == L'\t') line++;
        if (!wcsncmp(line, L"set ", 4)) line += 4;
        wchar_t *eq = wcschr(line, L'=');
        if (!*line || *line == L'#' || !eq) continue;
        *eq = 0;
        env(line, eq + 1);
    }

    // ark.exe uses MSYS2's DLLs (SDL3, Vulkan loader, shaderc, ...): started from Explorer, their folder isn't on PATH
    {
        wchar_t dlls[MAX_PATH], path[32768];
        GetPrivateProfileStringW(L"ark", L"msys2_bin", L"C:\\msys64\\ucrt64\\bin", dlls, MAX_PATH, ini);
        DWORD n = GetEnvironmentVariableW(L"PATH", path, 32768);
        if (GetFileAttributesW(dlls) != INVALID_FILE_ATTRIBUTES && n < 32768 && !wcsstr(path, dlls)) {
            wchar_t np[32768];
            swprintf(np, 32768, L"%ls;%ls", dlls, path);
            SetEnvironmentVariableW(L"PATH", np);
        }
    }
    wchar_t cmd[2 * MAX_PATH + 16];
    swprintf(cmd, 2 * MAX_PATH + 16, L"\"%ls\" \"%ls\"", exe_path, game);
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    HANDLE log = INVALID_HANDLE_VALUE;
    DWORD flags = 0;
    if (SendMessageW(h_log, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
        wchar_t lp[MAX_PATH];
        swprintf(lp, MAX_PATH, L"%ls\\ark.log", root);
        log = CreateFileW(lp, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (log != INVALID_HANDLE_VALUE) {
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = si.hStdError = log;
            flags = CREATE_NO_WINDOW;
        }
    } else {
        flags = CREATE_NEW_CONSOLE;
    }
    if (!CreateProcessW(exe_path, cmd, NULL, NULL, log != INVALID_HANDLE_VALUE, flags, NULL, root, &si, &pi)) {
        status(L"Could not start %ls (is the game built? make ark)", exe_path);
    } else {
        CloseHandle(pi.hThread);
        CreateThread(NULL, 0, watch_game, pi.hProcess, 0, NULL);  // (reports when the game exits)
        status(L"Running. Log: %ls\\ark.log", root);
    }
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
}

static void browse_folder(HWND target, const wchar_t *title) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = wnd;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t p[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, p)) SetWindowTextW(target, p);
    CoTaskMemFree(pidl);
}

static void build_ui(void) {
    int y = 12;
    heading(L"Game", y); y += 26;
    label(L"Game folder", 24, y, 140);
    h_game = add(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 170, y, 330, 24, ID_GAME);
    add(L"BUTTON", L"Browse...", WS_TABSTOP, 508, y, 80, 24, ID_GAME_BROWSE); y += 32;
    h_log = add(L"BUTTON", L"Write the log to ark.log (send this to Claude when something goes wrong)", BS_AUTOCHECKBOX | WS_TABSTOP, 24, y, 560, 22, ID_LOG);
    y += 34;

    heading(L"Frame rate", y); y += 26;
    h_60 = add(L"BUTTON", L"60 fps at normal game speed", BS_AUTOCHECKBOX | WS_TABSTOP, 24, y, 400, 22, ID_60FPS); y += 34;

    heading(L"Graphics", y); y += 26;
    label(L"Draw distance", 24, y, 140);
    h_lod = combo(LOD_ITEMS, N(LOD_ITEMS), 170, y, 418, ID_LOD, false); y += 32;
    label(L"Shadows", 24, y, 140);
    h_shadows = combo(SHADOW_ITEMS, N(SHADOW_ITEMS), 170, y, 418, ID_SHADOWS, false); y += 32;
    label(L"Resolution scale", 24, y, 140);
    h_scale = combo(SCALE_ITEMS, N(SCALE_ITEMS), 170, y, 120, ID_SCALE, true);
    label(L"Texture filtering", 310, y, 110);
    h_aniso = combo(ANISO_ITEMS, N(ANISO_ITEMS), 428, y, 160, ID_ANISO, false); y += 32;
    label(L"Texture mip bias", 24, y, 140);
    h_bias = combo(BIAS_ITEMS, N(BIAS_ITEMS), 170, y, 120, ID_BIAS, true);
    label(L"(negative = sharper far textures)", 310, y, 280); y += 40;

    heading(L"Mods and testing", y); y += 26;
    label(L"Mods folder", 24, y, 140);
    h_mods = add(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 170, y, 330, 24, ID_MODS);
    add(L"BUTTON", L"Browse...", WS_TABSTOP, 508, y, 80, 24, ID_MODS_BROWSE); y += 32;
    label(L"Game settings", 24, y, 140);
    h_config = add(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 170, y, 418, 24, ID_CONFIG); y += 28;
    label(L"e.g. kTreeVisibleDistance=800;terrain_radius=1000", 170, y, 418); y += 28;
    label(L"Extra options", 24, y, 140);
    h_extra = add(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP, 170, y, 418, 84, ID_EXTRA);
    y += 88;
    label(L"One NAME=value per line (test options from Claude)", 170, y, 418); y += 36;

    add(L"BUTTON", L"Launch", BS_DEFPUSHBUTTON | WS_TABSTOP, 24, y, 140, 34, ID_LAUNCH);
    add(L"BUTTON", L"Open ark.log", WS_TABSTOP, 176, y, 130, 34, ID_OPENLOG);
    add(L"BUTTON", L"Open Ark folder", WS_TABSTOP, 318, y, 130, 34, ID_OPENDIR);
    add(L"BUTTON", L"Defaults", WS_TABSTOP, 460, y, 128, 34, ID_PRESET_DEFAULT); y += 44;
    h_status = add(L"STATIC", L"", SS_LEFTNOWORDWRAP, 24, y, 900, 20, ID_STATUS);

    // right column: testing switches
    int x = 624;
    y = 12;
    heading_at(L"60 fps: code switched to 1/60 s", x, y); y += 26;
    for (int i = 0; i < N(checks) && checks[i].bit; i++, y += 26)
        checks[i].h = add(L"BUTTON", checks[i].label, BS_AUTOCHECKBOX | WS_TABSTOP, x + 8, y, 320, 22, 200 + i);
    y += 10;
    heading_at(L"Testing", x, y); y += 26;
    label(L"Full-detail models", x + 8, y, 120);
    h_lod0 = combo(LOD0_ITEMS, N(LOD0_ITEMS), x + 132, y, 196, 300, false); y += 32;
    int i = 0;
    while (checks[i].bit) i++;
    checks[i].h = add(L"BUTTON", checks[i].label, BS_AUTOCHECKBOX | WS_TABSTOP, x + 8, y, 320, 22, 200 + i);
    y += 36;
    heading_at(L"Diagnostics", x, y); y += 26;
    for (i++; i < N(checks); i++, y += 26)
        checks[i].h = add(L"BUTTON", checks[i].label, BS_AUTOCHECKBOX | WS_TABSTOP, x + 8, y, 320, 22, 200 + i);
    y += 12;
    heading_at(L"Keys in the game window", x, y); y += 24;
    static const wchar_t *keys[] = {L"F2  60 fps on / off", L"F3  full-detail models", L"F4  grass to its limit",
                                    L"F5  dialogue text pace", L"F6  second frame table halved", L"F9  frame dump",
                                    L"F10  timing scan", L"F11  fullscreen",
                                    L"(the title bar shows which are on)"};
    for (int k = 0; k < N(keys); k++, y += 20) label(keys[k], x + 8, y, 320);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_LAUNCH: launch(); return 0;
        case ID_GAME_BROWSE: browse_folder(h_game, L"The game folder (with exefs and romfs.bin)"); return 0;
        case ID_MODS_BROWSE: browse_folder(h_mods, L"The mods folder (holding romfs\\...)"); return 0;
        case ID_OPENLOG: {
            wchar_t p[MAX_PATH];
            swprintf(p, MAX_PATH, L"%ls\\ark.log", root);
            if ((INT_PTR)ShellExecuteW(h, L"open", L"notepad.exe", p, NULL, SW_SHOWNORMAL) <= 32) status(L"No log at %ls", p);
            return 0;
        }
        case ID_OPENDIR: ShellExecuteW(h, L"open", root, NULL, NULL, SW_SHOWNORMAL); return 0;
        case ID_PRESET_DEFAULT:
            SendMessageW(h_60, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(h_log, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(h_lod, CB_SETCURSEL, 3, 0);
            SendMessageW(h_shadows, CB_SETCURSEL, 1, 0);
            SetWindowTextW(h_scale, L"1");
            SendMessageW(h_aniso, CB_SETCURSEL, 4, 0);
            SetWindowTextW(h_bias, L"0");
            SetWindowTextW(h_config, L"");
            SetWindowTextW(h_extra, L"");
            SendMessageW(h_lod0, CB_SETCURSEL, 0, 0);
            for (int i = 0; i < N(checks); i++) SendMessageW(checks[i].h, BM_SETCHECK, checks[i].def, 0);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_APP: {
        wchar_t b[64];
        swprintf(b, 64, w ? L"%u (0x%x)" : L"%u", (unsigned)w, (unsigned)w);
        status(w ? L"The game stopped with code %ls: see ark.log" : L"The game closed (code %ls).", b);
        return 0;
    }
    case WM_CLOSE: save_all(); DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show) {
    (void)prev, (void)cmdline;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // the launcher sits in build\ next to ark.exe; the game runs from the folder above (ark.ini, game\, ark.log)
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    wcscpy(root, self);
    wchar_t *slash = wcsrchr(root, L'\\');
    if (slash) *slash = 0;
    swprintf(exe_path, MAX_PATH, L"%ls\\ark.exe", root);
    swprintf(ini, MAX_PATH, L"%ls\\ark_launcher.ini", root);
    slash = wcsrchr(root, L'\\');
    if (slash && !_wcsicmp(slash + 1, L"build")) *slash = 0;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"ArkLauncher";
    RegisterClassW(&wc);

    HDC sdc = GetDC(NULL);
    dpi = GetDeviceCaps(sdc, LOGPIXELSY);
    ReleaseDC(NULL, sdc);
    font = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    font_bold = CreateFontW(-S(15), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    RECT r = {0, 0, S(968), S(668)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    wnd = CreateWindowW(L"ArkLauncher", L"Ark Launcher", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left,
                        r.bottom - r.top, NULL, NULL, inst, NULL);
    build_ui();
    load_all();
    status(L"Ark folder: %ls", root);
    ShowWindow(wnd, show);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (IsDialogMessageW(wnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
