#include <windows.h>
#include <stdio.h>
#include <DbgHelp.h>
#include <psapi.h>
#include <string.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include <stdarg.h>
#include <cstdlib>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "winmm.lib")

// === PROXY EXPORTS ===
extern "C" LPVOID g_pExports[5] = {0};
extern "C" __declspec(naked) void DirectInput8Create_Proxy() { __asm jmp g_pExports[0] }
extern "C" __declspec(naked) void DllCanUnloadNow_Proxy()    { __asm jmp g_pExports[1] }
extern "C" __declspec(naked) void DllGetClassObject_Proxy()   { __asm jmp g_pExports[2] }
extern "C" __declspec(naked) void DllRegisterServer_Proxy()   { __asm jmp g_pExports[3] }
extern "C" __declspec(naked) void DllUnregisterServer_Proxy() { __asm jmp g_pExports[4] }

#pragma comment(linker, "/EXPORT:DirectInput8Create=_DirectInput8Create_Proxy")
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=_DllCanUnloadNow_Proxy,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=_DllGetClassObject_Proxy,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=_DllRegisterServer_Proxy,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=_DllUnregisterServer_Proxy,PRIVATE")

// === GLOBALS ===
HMODULE g_hModule = nullptr;
char g_szAOBLogPath[MAX_PATH] = {0};
char g_szIniPath[MAX_PATH] = {0};

// === CONFIG ===
struct Config {
    bool SingleCoreAffinity = true;
    bool CrashHandler = true;
    bool TrampolineFix = true;
    bool BorderlessFullscreen = true;
    bool LimitFPS = true;
    double TargetFPS = 60.0;
    bool AspectFix = true;
    bool HudFix = true;
    bool FlagFix = true;
    bool ShowWinnerHUDFix = true;
    bool MuteOnFocusLoss = true;
    bool PauseOnFocusLoss = true;
    bool AOBLog = false;
} cfg;

struct ConfigEntry {
    const char* section;
    const char* key;
    const char* def;
    enum { INT, FLOAT } type;
    void* ptr;
};

static ConfigEntry g_ConfigTable[] = {
    {"General",       "SingleCoreAffinity", "1",    ConfigEntry::INT,   &cfg.SingleCoreAffinity},
    {"General",       "CrashHandler",       "1",    ConfigEntry::INT,   &cfg.CrashHandler},
    {"General",       "TrampolineFix",      "1",    ConfigEntry::INT,   &cfg.TrampolineFix},
    {"Display",       "BorderlessFullscreen","1",   ConfigEntry::INT,   &cfg.BorderlessFullscreen},
    {"Display",       "LimitFPS",           "1",    ConfigEntry::INT,   &cfg.LimitFPS},
    {"Display",       "TargetFPS",          "60.0", ConfigEntry::FLOAT, &cfg.TargetFPS},
    {"Widescreen",    "AspectFix",          "1",    ConfigEntry::INT,   &cfg.AspectFix},
    {"Widescreen",    "HudFix",             "1",    ConfigEntry::INT,   &cfg.HudFix},
    {"Widescreen",    "FlagFix",            "1",    ConfigEntry::INT,   &cfg.FlagFix},
    {"Widescreen",    "ShowWinnerHUDFix",   "1",    ConfigEntry::INT,   &cfg.ShowWinnerHUDFix},
    {"AudioAndFocus", "MuteOnFocusLoss",    "1",    ConfigEntry::INT,   &cfg.MuteOnFocusLoss},
    {"AudioAndFocus", "PauseOnFocusLoss",   "1",    ConfigEntry::INT,   &cfg.PauseOnFocusLoss},
    {"Debug",         "AOBLog",             "0",    ConfigEntry::INT,   &cfg.AOBLog},
};

void LoadConfiguration() {
    if (g_hModule) {
        GetModuleFileNameA(g_hModule, g_szIniPath, MAX_PATH);
        char* s = strrchr(g_szIniPath, '\\');
        if (s) { *s = '\0'; strcat_s(g_szIniPath, "\\MashedFix.ini"); }
        else strcpy_s(g_szIniPath, "MashedFix.ini");
    } else strcpy_s(g_szIniPath, "MashedFix.ini");

    if (GetFileAttributesA(g_szIniPath) == INVALID_FILE_ATTRIBUTES)
        for (auto& e : g_ConfigTable)
            WritePrivateProfileStringA(e.section, e.key, e.def, g_szIniPath);

    for (auto& e : g_ConfigTable) {
        if (e.type == ConfigEntry::INT)
            *(bool*)e.ptr = GetPrivateProfileIntA(e.section, e.key, atoi(e.def), g_szIniPath) != 0;
        else {
            char buf[32];
            GetPrivateProfileStringA(e.section, e.key, e.def, buf, sizeof(buf), g_szIniPath);
            *(double*)e.ptr = atof(buf);
            if (*(double*)e.ptr <= 0.0) *(double*)e.ptr = 60.0;
        }
    }
}

// === LOGGING ===
void InitAOBLogger() {
    if (g_hModule) {
        GetModuleFileNameA(g_hModule, g_szAOBLogPath, MAX_PATH);
        char* s = strrchr(g_szAOBLogPath, '\\');
        if (s) { *s = '\0'; strcat_s(g_szAOBLogPath, "\\AOBLog.txt"); }
        else strcpy_s(g_szAOBLogPath, "AOBLog.txt");
    } else strcpy_s(g_szAOBLogPath, "AOBLog.txt");
    DeleteFileA(g_szAOBLogPath);
}

void Log(const char* fmt, ...) {
    if (!cfg.AOBLog || !g_szAOBLogPath[0]) return;
    FILE* f = nullptr;
    fopen_s(&f, g_szAOBLogPath, "a");
    if (!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args; va_start(args, fmt); vfprintf(f, fmt, args); va_end(args);
    fprintf(f, "\n"); fclose(f);
}

// === CRASH HANDLER ===
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    SYSTEMTIME st; GetLocalTime(&st);
    char path[MAX_PATH];
    sprintf_s(path, "crashdump_%04d-%02d-%02d_%02d-%02d-%02d.dmp", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = { GetCurrentThreadId(), ep, FALSE };
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mei, NULL, NULL);
        CloseHandle(hFile);
        char msg[512];
        sprintf_s(msg, "The game has crashed!\nCrash dump saved as '%s'.", path);
        MessageBoxA(NULL, msg, "Game Crash", MB_ICONERROR | MB_OK);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// === AOB SCANNER ===
struct PatByte { bool wild; unsigned char val; };

std::string AddrStr(uintptr_t addr) {
    HMODULE hMod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &hMod) && hMod) {
        char name[MAX_PATH];
        if (GetModuleBaseNameA(GetCurrentProcess(), hMod, name, MAX_PATH)) {
            char buf[256];
            sprintf_s(buf, "%s+%X", name, (unsigned)(addr - (uintptr_t)hMod));
            return buf;
        }
    }
    char buf[32]; sprintf_s(buf, "0x%08X", (unsigned)addr);
    return buf;
}

std::vector<PatByte> ParseAOB(const char* pat) {
    std::vector<PatByte> r;
    for (const char* p = pat; *p;) {
        if (*p == ' ') { p++; continue; }
        if (*p == '?') { r.push_back({true, 0}); p++; if (*p == '?') p++; }
        else {
            if (!isxdigit(p[0]) || !isxdigit(p[1])) break;
            char h[3] = {p[0], p[1], 0};
            r.push_back({false, (unsigned char)strtoul(h, nullptr, 16)});
            p += 2;
        }
    }
    return r;
}

static uintptr_t s_textStart = 0, s_textEnd = 0;

void InitTextSection() {
    if (s_textStart) return;
    HMODULE hMod = GetModuleHandle(nullptr);
    uintptr_t base = (uintptr_t)hMod;
    auto dos = (PIMAGE_DOS_HEADER)base;
    auto nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            s_textStart = base + sec[i].VirtualAddress;
            s_textEnd = s_textStart + sec[i].Misc.VirtualSize;
            break;
        }
    if (!s_textStart) {
        MODULEINFO mi; GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi));
        s_textStart = (uintptr_t)mi.lpBaseOfDll + 0x1000;
        s_textEnd = (uintptr_t)mi.lpBaseOfDll + mi.SizeOfImage;
    }
}

std::vector<uintptr_t> FindAll(const char* pattern, uintptr_t start = 0) {
    InitTextSection();
    std::vector<uintptr_t> matches;
    auto pb = ParseAOB(pattern);
    if (pb.empty()) { Log("[ERR] Empty pattern: %s", pattern); return matches; }
    uintptr_t from = (start > s_textStart && start < s_textEnd) ? start : s_textStart;
    size_t len = pb.size();
    for (uintptr_t i = from; i <= s_textEnd - len; i++) {
        bool ok = true;
        for (size_t j = 0; j < len; j++)
            if (!pb[j].wild && *(unsigned char*)(i + j) != pb[j].val) { ok = false; break; }
        if (ok) matches.push_back(i);
    }
    if (cfg.AOBLog) {
        if (matches.empty()) Log("[WARN] Not found: %s", pattern);
        else if (matches.size() == 1) Log("[OK] %s -> %s", pattern, AddrStr(matches[0]).c_str());
        else { Log("[WARN] %d matches: %s", (int)matches.size(), pattern); for (auto m : matches) Log("  -> %s", AddrStr(m).c_str()); }
    }
    return matches;
}

uintptr_t Find(const char* pattern, uintptr_t start = 0) {
    auto m = FindAll(pattern, start);
    return m.empty() ? 0 : m[0];
}

// === PATCH HELPERS ===
void* PlaceHook(void* target, void* hook, int len) {
    if (len < 5) { Log("[ERR] Hook len<5 at %s", AddrStr((uintptr_t)target).c_str()); return nullptr; }
    DWORD prot;
    VirtualProtect(target, len, PAGE_EXECUTE_READWRITE, &prot);
    void* gw = VirtualAlloc(nullptr, len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!gw) { VirtualProtect(target, len, prot, &prot); return nullptr; }
    memcpy(gw, target, len);
    *(BYTE*)((uintptr_t)gw + len) = 0xE9;
    *(uintptr_t*)((uintptr_t)gw + len + 1) = ((uintptr_t)target - (uintptr_t)gw) - 5;
    *(BYTE*)target = 0xE9;
    *(uintptr_t*)((uintptr_t)target + 1) = ((uintptr_t)hook - (uintptr_t)target) - 5;
    for (int i = 5; i < len; i++) *(BYTE*)((uintptr_t)target + i) = 0x90;
    VirtualProtect(target, len, prot, &prot);
    Log("[OK] Hook %s len=%d", AddrStr((uintptr_t)target).c_str(), len);
    return gw;
}

void PlaceJMP(uintptr_t addr, uintptr_t to, int len) {
    DWORD prot; VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &prot);
    *(BYTE*)addr = 0xE9;
    *(uintptr_t*)(addr + 1) = (to - addr) - 5;
    for (int i = 5; i < len; i++) *(BYTE*)(addr + i) = 0x90;
    VirtualProtect((void*)addr, len, prot, &prot);
    Log("[OK] JMP %s -> %s", AddrStr(addr).c_str(), AddrStr(to).c_str());
}

void PlaceCall(uintptr_t addr, uintptr_t to, int len) {
    DWORD prot; VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &prot);
    *(BYTE*)addr = 0xE8;
    *(uintptr_t*)(addr + 1) = (to - addr) - 5;
    for (int i = 5; i < len; i++) *(BYTE*)(addr + i) = 0x90;
    VirtualProtect((void*)addr, len, prot, &prot);
    Log("[OK] CALL %s -> %s", AddrStr(addr).c_str(), AddrStr(to).c_str());
}

void PatchPtr(uintptr_t addr, int off, void* val) {
    DWORD prot; uintptr_t a = addr + off;
    VirtualProtect((void*)a, 4, PAGE_EXECUTE_READWRITE, &prot);
    *(uintptr_t*)a = (uintptr_t)val;
    VirtualProtect((void*)a, 4, prot, &prot);
    Log("[OK] PatchPtr %s+%d", AddrStr(addr).c_str(), off);
}

void PatchFloat(uintptr_t addr, int off, float val) {
    DWORD prot; uintptr_t a = addr + off;
    VirtualProtect((void*)a, 4, PAGE_EXECUTE_READWRITE, &prot);
    *(float*)a = val;
    VirtualProtect((void*)a, 4, prot, &prot);
    Log("[OK] PatchFloat %s+%d = %.2f", AddrStr(addr).c_str(), off, val);
}

void PatchByte(uintptr_t addr, BYTE val) {
    DWORD prot; VirtualProtect((void*)addr, 1, PAGE_EXECUTE_READWRITE, &prot);
    *(BYTE*)addr = val;
    VirtualProtect((void*)addr, 1, prot, &prot);
    Log("[OK] PatchByte %s = 0x%02X", AddrStr(addr).c_str(), val);
}

uintptr_t ResolveCall(uintptr_t addr) { return addr + 5 + *(int*)(addr + 1); }

// === TYPES ===
struct RwV2d { float x, y; };
struct RwV3d { float x, y, z; };
struct RwMatrix { RwV3d right; unsigned int f; RwV3d up; unsigned int p1; RwV3d at; unsigned int p2; RwV3d pos; unsigned int p3; };

typedef void* (__cdecl* RwCameraSetViewWindow_t)(void*, const RwV2d*);
typedef void* (__cdecl* RwFrameTranslate_t)(void*, RwV3d*, int);
typedef RwMatrix* (__cdecl* GetActiveCameraMatrix_t)(int);
typedef void (__cdecl* HUD_DrawAsciiText_t)(const char*, float, float, int, float, int);
typedef void (__cdecl* HUD_DrawTextInRect_t)(int, float, float, float, float, int, float);
typedef void (__cdecl* sub_4045D0_t)();
typedef void (__cdecl* UpdateMatchState_t)(int);
typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT);
typedef HRESULT (WINAPI* Direct3DCreate9Ex_t)(UINT, IDirect3D9Ex**);
typedef HRESULT (STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT (STDMETHODCALLTYPE* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (STDMETHODCALLTYPE* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

// === HOOK STATE ===
RwCameraSetViewWindow_t orig_RwCameraSetViewWindow = nullptr;
RwFrameTranslate_t orig_RwFrameTranslate = nullptr;
GetActiveCameraMatrix_t pGetActiveCameraMatrix = nullptr;
HUD_DrawAsciiText_t orig_HUD_DrawAsciiText = nullptr;
HUD_DrawTextInRect_t orig_HUD_DrawTextInRect = nullptr;
sub_4045D0_t orig_sub_4045D0 = nullptr;
UpdateMatchState_t orig_UpdateMatchState = nullptr;
Direct3DCreate9_t orig_Direct3DCreate9 = nullptr;
Direct3DCreate9Ex_t orig_Direct3DCreate9Ex = nullptr;
CreateDevice_t orig_CreateDevice = nullptr;
Reset_t orig_Reset = nullptr;
Present_t orig_Present = nullptr;
void* orig_HUD_DrawMessageBox = nullptr;

uintptr_t flag_entry_jmp_back1 = 0, flag_entry_jmp_back2 = 0;
uintptr_t flag_jmp_back_addr1 = 0, flag_jmp_back_addr2 = 0;
float fFlagMultiplier = 1.0f, fNewScreenWidth = 640.0f, fNewCenterX = 320.0f, fFlagMargin = 1.28f;
int g_current_flag_arg4 = 0;
float* pDefaultTextPos = nullptr;
float* p_flt_5DE358 = nullptr;
float* p_flt_5E0E30 = nullptr;
int* g_GameState_ptr = nullptr;
int* p_dword_777F5C = nullptr;
void* g_PlatformID_ptr = nullptr;

// === UTILITY ===
float GetGameAspectRatio() {
    HWND hWnd = GetActiveWindow();
    if (hWnd) {
        DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);
        if (pid == GetCurrentProcessId()) {
            RECT r; if (GetClientRect(hWnd, &r)) {
                int w = r.right - r.left, h = r.bottom - r.top;
                if (w > 0 && h > 0) return (float)w / (float)h;
            }
        }
    }
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    return (sw > 0 && sh > 0) ? (float)sw / (float)sh : 1.3333333f;
}

bool IsGameFocused() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

void AdjustWindowForBorderless(HWND hWnd) {
    if (!hWnd) return;
    LONG style = GetWindowLong(hWnd, GWL_STYLE) & ~(WS_CAPTION | WS_BORDER | WS_THICKFRAME | WS_DLGFRAME);
    LONG exStyle = GetWindowLong(hWnd, GWL_EXSTYLE) & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hWnd, GWL_STYLE, style);
    SetWindowLong(hWnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hWnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOZORDER);
}

void LimitFPS(double target) {
    static LARGE_INTEGER freq, last;
    static bool init = false;
    if (!init) { QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last); init = true; }
    double frameTime = 1.0 / target;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - last.QuadPart) / freq.QuadPart;
    if (elapsed < frameTime) {
        double sleep = frameTime - elapsed;
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        double endTarget = (double)end.QuadPart + sleep * freq.QuadPart;
        if (sleep > 0.002) Sleep((DWORD)((sleep - 0.001) * 1000.0));
        do { QueryPerformanceCounter(&end); } while ((double)end.QuadPart < endTarget);
    }
    QueryPerformanceCounter(&last);
}

// === D3D9 HOOKS ===
HRESULT STDMETHODCALLTYPE hk_Present(IDirect3DDevice9* This, const RECT* s, const RECT* d, HWND h, const RGNDATA* r) {
    if (cfg.LimitFPS) LimitFPS(cfg.TargetFPS);
    return orig_Present(This, s, d, h, r);
}

HRESULT STDMETHODCALLTYPE hk_Reset(IDirect3DDevice9* This, D3DPRESENT_PARAMETERS* pp) {
    if (cfg.BorderlessFullscreen && pp) {
        pp->Windowed = TRUE; pp->FullScreen_RefreshRateInHz = 0; pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
        if (pp->hDeviceWindow) AdjustWindowForBorderless(pp->hDeviceWindow);
    }
    return orig_Reset(This, pp);
}

HRESULT STDMETHODCALLTYPE hk_CreateDevice(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DevType, HWND hFocus, DWORD Flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** ppDev) {
    if (cfg.BorderlessFullscreen && pp) {
        pp->Windowed = TRUE; pp->FullScreen_RefreshRateInHz = 0; pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
        if (!pp->hDeviceWindow) pp->hDeviceWindow = hFocus;
        AdjustWindowForBorderless(pp->hDeviceWindow);
    }
    HRESULT hr = orig_CreateDevice(This, Adapter, DevType, hFocus, Flags, pp, ppDev);
    if (SUCCEEDED(hr) && ppDev && *ppDev && !orig_Reset) {
        uintptr_t* vt = *(uintptr_t**)*ppDev;
        DWORD prot; VirtualProtect(&vt[16], sizeof(uintptr_t) * 2, PAGE_EXECUTE_READWRITE, &prot);
        orig_Reset = (Reset_t)vt[16]; vt[16] = (uintptr_t)hk_Reset;
        orig_Present = (Present_t)vt[17]; vt[17] = (uintptr_t)hk_Present;
        VirtualProtect(&vt[16], sizeof(uintptr_t) * 2, prot, &prot);
    }
    return hr;
}

void HookD3D9Vtable(IDirect3D9* d3d) {
    if (!d3d || orig_CreateDevice) return;
    uintptr_t* vt = *(uintptr_t**)d3d;
    DWORD prot; VirtualProtect(&vt[16], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &prot);
    orig_CreateDevice = (CreateDevice_t)vt[16]; vt[16] = (uintptr_t)hk_CreateDevice;
    VirtualProtect(&vt[16], sizeof(uintptr_t), prot, &prot);
}

IDirect3D9* WINAPI hk_Direct3DCreate9(UINT v) { IDirect3D9* d = orig_Direct3DCreate9(v); if (d) HookD3D9Vtable(d); return d; }
HRESULT WINAPI hk_Direct3DCreate9Ex(UINT v, IDirect3D9Ex** pp) { HRESULT hr = orig_Direct3DCreate9Ex(v, pp); if (SUCCEEDED(hr) && pp && *pp) HookD3D9Vtable(*pp); return hr; }

// === ASPECT FIX ===
void* __cdecl hk_RwCameraSetViewWindow(void* cam, const RwV2d* vw) {
    if (!cam || !vw) return orig_RwCameraSetViewWindow(cam, vw);
    float aspect = 1.3333333f;
    void* fb = *(void**)((char*)cam + 96);
    if (fb) {
        int w = *(int*)((char*)fb + 12), h = *(int*)((char*)fb + 16);
        aspect = (w > 0 && h > 0 && w != h) ? (float)w / (float)h : GetGameAspectRatio();
    } else aspect = GetGameAspectRatio();
    if (aspect > 1.333334f) {
        fNewScreenWidth = 480.0f * aspect; fNewCenterX = 240.0f * aspect;
        RwV2d mod = *vw; mod.x = mod.y * aspect;
        return orig_RwCameraSetViewWindow(cam, &mod);
    }
    return orig_RwCameraSetViewWindow(cam, vw);
}

// === FLAG FIX ===
#pragma optimize("", off)
RwMatrix* Call_GetActiveCameraMatrix(int p) { return pGetActiveCameraMatrix ? pGetActiveCameraMatrix(p) : nullptr; }

void __cdecl ScaleFlagMatrix(RwMatrix* m) {
    if (!m || fFlagMultiplier <= 1.0f) return;
    float sf = fFlagMultiplier * fFlagMargin - 1.0f;
    float v = (g_current_flag_arg4 == 0) ? 2.0f : -2.0f;
    RwMatrix* cm = Call_GetActiveCameraMatrix(0);
    if (cm) { m->pos.x += cm->right.x * v * sf; m->pos.y += cm->right.y * v * sf; m->pos.z += cm->right.z * v * sf; }
    float s = fFlagMultiplier * fFlagMargin;
    m->right.x *= s; m->right.y *= s; m->right.z *= s;
}
#pragma optimize("", on)

void* __cdecl hk_RwFrameTranslate_Cup(void* frame, RwV3d* pos, int combine) {
    if (pos && fFlagMultiplier > 1.0f) pos->x = pos->x * fFlagMultiplier - 1.25f * (fFlagMultiplier - 1.0f);
    return orig_RwFrameTranslate(frame, pos, combine);
}

__declspec(naked) void hk_FlagEntry1() {
    __asm {
        mov eax, [esp + 8]
        mov g_current_flag_arg4, eax
        sub esp, 60h
        mov eax, [esp + 68h]
        jmp flag_entry_jmp_back1
    }
}

__declspec(naked) void hk_FlagEntry2() {
    __asm {
        mov eax, [esp + 8]
        mov g_current_flag_arg4, eax
        sub esp, 60h
        mov eax, [esp + 68h]
        jmp flag_entry_jmp_back2
    }
}

__declspec(naked) void hk_FlagLea1() {
    __asm {
        lea ecx, [esp + 24h]
        pushad
        push ecx
        call ScaleFlagMatrix
        add esp, 4
        popad
        push ecx
        jmp flag_jmp_back_addr1
    }
}

__declspec(naked) void hk_FlagLea2() {
    __asm {
        lea ecx, [esp + 24h]
        pushad
        push ecx
        call ScaleFlagMatrix
        add esp, 4
        popad
        push ecx
        jmp flag_jmp_back_addr2
    }
}
// === HUD FIX ===
void __cdecl hk_HUD_DrawMessageBox() {
    float origX = 0; bool saved = false;
    if (pDefaultTextPos) {
        DWORD prot; VirtualProtect(pDefaultTextPos, 4, PAGE_EXECUTE_READWRITE, &prot);
        origX = *pDefaultTextPos; *pDefaultTextPos = ((190.0f * fFlagMultiplier) + 80.0f) / fNewScreenWidth;
        VirtualProtect(pDefaultTextPos, 4, prot, &prot); saved = true;
    }
    ((void(__cdecl*)())orig_HUD_DrawMessageBox)();
    if (saved && pDefaultTextPos) {
        DWORD prot; VirtualProtect(pDefaultTextPos, 4, PAGE_EXECUTE_READWRITE, &prot);
        *pDefaultTextPos = origX;
        VirtualProtect(pDefaultTextPos, 4, prot, &prot);
    }
}

void __cdecl SetupHUDTextElement_C(char* el, float* coords, DWORD a0, DWORD* a4, DWORD* a8, DWORD aC, DWORD a10) {
    __try {
        if (!coords || !el) return;
        float flt358 = p_flt_5DE358 ? *p_flt_5DE358 : 0.0015625f;
        float fltE30 = p_flt_5E0E30 ? *p_flt_5E0E30 : 480.0f;
        float x = coords[0], y = coords[1];
        if (fNewScreenWidth > 640.0f) x = (x == 320.0f) ? fNewCenterX : x * (fNewScreenWidth / 640.0f);
        *(float*)(el + 0x8C) = x; *(float*)(el + 0x90) = y;
        float xs = (fNewScreenWidth > 640.0f) ? (1.0f / fNewScreenWidth) : flt358;
        *(float*)(el + 0x94) = (fNewCenterX - x) * xs;
        *(float*)(el + 0x98) = (fltE30 - y) * flt358;
        *(DWORD*)(el + 0x9C) = a0;
        if (a4) { __try { *(DWORD*)(el + 0xA0) = *a4; } __except(1) {} }
        if (a8) { __try { *(DWORD*)(el + 0xA4) = *a8; } __except(1) {} }
        *(DWORD*)(el + 0xA8) = aC; *(DWORD*)(el + 0xAC) = a10;
    } __except(1) {}
}

__declspec(naked) void hk_SetupHUDTextElement() {
    __asm {
        mov edx, esp
        push [edx + 14h]
        push [edx + 10h]
        push [edx + 0Ch]
        push [edx + 08h]
        push [edx + 04h]
        push ecx
        push eax
        call SetupHUDTextElement_C
        add esp, 28
        retn
    }
}

void __cdecl hk_HUD_DrawAsciiText(const char* text, float x, float y, int color, float scale, int align) {
    if (align == 2 && fNewScreenWidth > 640.0f) {
        if (x == 320.0f) x = fNewCenterX;
        else if (x == 323.0f) x = fNewCenterX + 3.0f;
        else x *= (fNewScreenWidth / 640.0f);
    }
    orig_HUD_DrawAsciiText(text, x, y, color, scale, align);
}

void __cdecl hk_HUD_DrawTextInRect(int id, float x, float y, float w, float h, int color, float scale) {
    if (fNewScreenWidth > 640.0f) { float s = 640.0f / fNewScreenWidth; x = (x - 320.0f) * s + 320.0f; }
    orig_HUD_DrawTextInRect(id, x, y, w, h, color, scale);
}

void __cdecl hk_sub_4045D0() {
    __try {
        if (!orig_HUD_DrawTextInRect) return;
        float s = (fNewScreenWidth > 640.0f) ? 640.0f / fNewScreenWidth : 1.0f;
        float off = 15.0f;
        float xs = (123.0f - 320.0f) * s + 320.0f + off;
        float xt = (120.0f - 320.0f) * s + 320.0f + off;
        orig_HUD_DrawTextInRect(559, xs, 83.0f, 400.0f, 80.0f, -16777216, 0.8f);
        orig_HUD_DrawTextInRect(559, xt, 80.0f, 400.0f, 80.0f, -939468584, 0.8f);
        orig_HUD_DrawTextInRect(560, xs, 143.0f, 400.0f, 80.0f, -16777216, 0.8f);
        orig_HUD_DrawTextInRect(560, xt, 140.0f, 400.0f, 80.0f, -939468584, 0.8f);
    } __except(1) {}
}

// === FOCUS/AUDIO ===
void __cdecl hk_UpdateMatchState(int a0) {
    if (cfg.PauseOnFocusLoss && g_GameState_ptr && *g_GameState_ptr == 3 && !IsGameFocused()) return;
    orig_UpdateMatchState(a0);
}

int __cdecl hk_ShouldMuteSound() {
    bool f = IsGameFocused();
    if (p_dword_777F5C) *p_dword_777F5C = f ? 1 : 0;
    return f ? 0 : 1;
}

// === TRAMPOLINE FIX ===
__declspec(naked) void hk_TrampolineFix() {
    __asm {
        push eax
        mov eax, g_PlatformID_ptr
        test eax, eax
        jz skip
        mov dword ptr [eax], 6
    skip:
        pop eax
        ret
    }
}

// === PATTERNS ===
namespace Pat {
    constexpr const char* CameraMatrix   = "83 F8 03 75 1F 83 7C 24 04 FF 75 0C A1 ? ? ? ? 8B 40 04 83 C0 10 C3";
    constexpr const char* SetViewWindow  = "8B 44 24 08 56 8B 74 24 08 8B 08 D9 05 ? ? ? ?";
    constexpr const char* FsubrCenter    = "D8 0D ? ? ? ? D8 2D ? ? ? ? D9 5C 24 14";
    constexpr const char* FlagEntry      = "83 EC 60 8B 44 24 68 85 C0 C7 44 24 08 ? ? ? ? C7 44 24 0C ? ? ? ? C7 44 24 10 ? ? ? ? C7 44 24 ? ? ? ? 40 C7 44 24 04 ? ? 34 43";
    constexpr const char* FlagLea        = "8D 4C 24 24 51 52 E8 ? ? ? ? C1 E3 04";
    constexpr const char* FrameTranslate = "E8 ? ? ? ? 8B 0D ? ? ? ? 6A 01 51 68 ? ? ? ?";
    constexpr const char* DefaultTextPos = "68 ? ? ? ? 51 8D 4C 24 14 D9 1C 24 51 52";
    constexpr const char* SetupHudElem   = "8B 11 D9 05 ? ? ? ? 89 90 8C ? ? ? ? D8 A0 8C ? ? ? ? 8B 49 04 8B 54 24 04 D8 0D ? ? ? ?";
    constexpr const char* MsgBox         = "81 EC 18 02 ? ? A1 ? ? ? ? 33 84 24 18 02 ? ?";
    constexpr const char* DrawTextInRect = "81 EC 14 02 00 00 A1 ? ? ? ? 33 84 24 14 02 00 00 53 89 84 24 14 02 00 00 8B 84 24 1C 02 00 00 50 E8 ? ? ? ? 8B D0 8D 5C 24 18 E8 ? ? ? ? E8 ? ? ? ? 8D 8C 24 34 02 00 00 51";
    constexpr const char* Sub4045D0      = "83 EC 08 56 B0 D8 57 88 44 24 0C 88 44 24 0D 32 C0 68 CD CC 4C 3F";
    constexpr const char* GameState      = "83 EC 10 E8 ? ? ? ? A1";
    constexpr const char* UpdateMatch    = "51 FF 05 ? ? ? ? E8 ? ? ? ? 85 C0 0F 84";
    constexpr const char* WaitMessage    = "A1 ? ? ? ? 85 C0 75 06 FF 15 ? ? ? ?";
    constexpr const char* ShouldMute     = "A1 ? ? ? ? 85 C0 74 19 A1 ? ? ? ? 3D ? ? 21 FF";
    constexpr const char* Trampoline     = "55 8B EC 83 E4 F8 81 EC ? ? ? ? A1 ? ? ? ? 33 45 04 53 56 57 68 ? ? ? ? 89 84 24 ? ? ? ? E8";
    constexpr const char* PointWinner    = "83 EC 20 B0 FF 56 8B 74 24 2C 83 FE FF 88 44 24 08 88 44 24 09 88 44 24 0A 88 44 24 0B 88 44 24 04 C6 44 24 05 00 C6 44 24 06 00 88 44 24 07 C7 44 24 0C 00 00 A0 43 C7 44 24 10 00 00 70 43";
    constexpr const char* FldCenter      = "D9 05 C8 EE 5D 00";
}

// === INIT ===
DWORD WINAPI InitPlugin(LPVOID) {
    LoadConfiguration();

    HMODULE hD3D9 = nullptr;
    for (int i = 0; i < 200 && !hD3D9; i++) { hD3D9 = GetModuleHandleA("d3d9.dll"); if (!hD3D9) Sleep(25); }
    if (hD3D9 && (cfg.BorderlessFullscreen || cfg.LimitFPS)) {
        if (auto p = GetProcAddress(hD3D9, "Direct3DCreate9")) orig_Direct3DCreate9 = (Direct3DCreate9_t)PlaceHook(p, (void*)hk_Direct3DCreate9, 5);
        if (auto p = GetProcAddress(hD3D9, "Direct3DCreate9Ex")) orig_Direct3DCreate9Ex = (Direct3DCreate9Ex_t)PlaceHook(p, (void*)hk_Direct3DCreate9Ex, 5);
    }

    float aspect = GetGameAspectRatio();
    if (aspect > 1.333334f) { fFlagMultiplier = aspect / (4.0f / 3.0f); fNewScreenWidth = 480.0f * aspect; fNewCenterX = 240.0f * aspect; }

    if (cfg.AspectFix) {
        if (auto a = Find(Pat::CameraMatrix)) pGetActiveCameraMatrix = (GetActiveCameraMatrix_t)(a - 5);
        if (auto a = Find(Pat::SetViewWindow)) orig_RwCameraSetViewWindow = (RwCameraSetViewWindow_t)PlaceHook((void*)a, (void*)hk_RwCameraSetViewWindow, 5);
        if (auto a = Find(Pat::FsubrCenter)) PatchPtr(a + 6, 2, &fNewCenterX);
    }

    if (cfg.FlagFix) {
        auto e1 = Find(Pat::FlagEntry);
        auto e2 = e1 ? Find(Pat::FlagEntry, e1 + 49) : 0;
        auto l1 = Find(Pat::FlagLea);
        auto l2 = l1 ? Find(Pat::FlagLea, l1 + 14) : 0;
        auto tr = Find(Pat::FrameTranslate);
        if (e1) { flag_entry_jmp_back1 = e1 + 7; PlaceHook((void*)e1, (void*)hk_FlagEntry1, 7); }
        if (l1) { flag_jmp_back_addr1 = l1 + 5; PlaceHook((void*)l1, (void*)hk_FlagLea1, 5); }
        if (e2) { flag_entry_jmp_back2 = e2 + 7; PlaceHook((void*)e2, (void*)hk_FlagEntry2, 7); }
        if (l2) { flag_jmp_back_addr2 = l2 + 5; PlaceHook((void*)l2, (void*)hk_FlagLea2, 5); }
        if (tr) { orig_RwFrameTranslate = (RwFrameTranslate_t)ResolveCall(tr); PlaceCall(tr, (uintptr_t)hk_RwFrameTranslate_Cup, 5); }
    }

    if (cfg.HudFix) {
        if (auto a = Find(Pat::DefaultTextPos)) pDefaultTextPos = *(float**)(a + 1);
        if (auto a = Find(Pat::SetupHudElem)) {
            p_flt_5E0E30 = *(float**)(a + 4); p_flt_5DE358 = *(float**)(a + 30);
            PlaceJMP(a, (uintptr_t)hk_SetupHUDTextElement, 5);
        }
        if (auto a = Find(Pat::MsgBox)) orig_HUD_DrawMessageBox = PlaceHook((void*)a, (void*)hk_HUD_DrawMessageBox, 6);
        if (auto a = Find(Pat::DrawTextInRect)) orig_HUD_DrawTextInRect = (HUD_DrawTextInRect_t)a;
        if (auto a = Find(Pat::Sub4045D0)) orig_sub_4045D0 = (sub_4045D0_t)PlaceHook((void*)a, (void*)hk_sub_4045D0, 6);
    }

    if (auto a = Find(Pat::GameState)) g_GameState_ptr = *(int**)(a + 9);

    if (cfg.PauseOnFocusLoss)
        if (auto a = Find(Pat::UpdateMatch)) orig_UpdateMatchState = (UpdateMatchState_t)PlaceHook((void*)a, (void*)hk_UpdateMatchState, 7);

    if (cfg.MuteOnFocusLoss) {
        if (auto a = Find(Pat::WaitMessage)) { p_dword_777F5C = *(int**)(a + 1); PatchByte(a + 7, 0xEB); }
        if (auto a = Find(Pat::ShouldMute)) PlaceJMP(a, (uintptr_t)hk_ShouldMuteSound, 5);
    }

    if (cfg.ShowWinnerHUDFix) {
        for (auto funcAddr : FindAll(Pat::PointWinner)) {
            Log("[OK] PointWinnerHUD at %s", AddrStr(funcAddr).c_str());
            uintptr_t s1 = funcAddr + 47;
            if (*(BYTE*)s1 == 0xC7 && *(BYTE*)(s1+1) == 0x44 && *(BYTE*)(s1+2) == 0x24 && *(BYTE*)(s1+3) == 0x0C)
                PatchFloat(s1, 4, fNewCenterX);
            else Log("[WARN] ShowWinner Script1 mismatch at %s", AddrStr(s1).c_str());
        }
        bool done = false;
        for (auto s2 : FindAll(Pat::FldCenter)) {
            uintptr_t s3 = 0;
            if (*(BYTE*)(s2 - 0x1A7) == 0x75 && *(BYTE*)(s2 - 0x1A6) == 0x0E) s3 = s2 - 0x1A7;
            else for (uintptr_t j = s2 - 0x250; j < s2 && !s3; j++) if (*(BYTE*)j == 0x75 && *(BYTE*)(j+1) == 0x0E) s3 = j;
            if (s3) { PatchPtr(s2, 2, &fNewCenterX); PatchByte(s3, 0xEB); done = true; break; }
        }
        if (!done) Log("[ERR] ShowWinner Scripts 2/3 not found");
    }

    return 0;
}

// === DLLMAIN ===
void LoadOriginalDInput8() {
    char path[MAX_PATH]; GetSystemDirectoryA(path, sizeof(path)); strcat_s(path, "\\dinput8.dll");
    HMODULE h = LoadLibraryA(path);
    if (!h) return;
    const char* names[] = {"DirectInput8Create","DllCanUnloadNow","DllGetClassObject","DllRegisterServer","DllUnregisterServer"};
    for (int i = 0; i < 5; i++) g_pExports[i] = GetProcAddress(h, names[i]);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule; DisableThreadLibraryCalls(hModule);
        LoadOriginalDInput8(); LoadConfiguration(); InitAOBLogger();
        timeBeginPeriod(1);
        if (cfg.SingleCoreAffinity) SetProcessAffinityMask(GetCurrentProcess(), 1);
        if (cfg.CrashHandler) SetUnhandledExceptionFilter(CrashHandler);
        if (cfg.TrampolineFix) {
            if (auto a = Find(Pat::Trampoline)) {
                uintptr_t pidInst = 0;
                for (uintptr_t i = a; i < a + 500 && !pidInst; i++)
                    if (*(BYTE*)i == 0xC7 && *(BYTE*)(i+1) == 0x05 && *(BYTE*)(i+6) == 0x01 && *(BYTE*)(i+7) == 0x00 && *(BYTE*)(i+8) == 0x00 && *(BYTE*)(i+9) == 0x00)
                        pidInst = i;
                if (pidInst) {
                    g_PlatformID_ptr = *(void**)(pidInst + 2);
                    if (g_PlatformID_ptr) { DWORD p; VirtualProtect(g_PlatformID_ptr, 4, PAGE_READWRITE, &p); *(int*)g_PlatformID_ptr = 6; VirtualProtect(g_PlatformID_ptr, 4, p, &p); }
                    PlaceJMP(a, (uintptr_t)hk_TrampolineFix, 5);
                }
            }
        }
        CreateThread(nullptr, 0, InitPlugin, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) timeEndPeriod(1);
    return TRUE;
}
