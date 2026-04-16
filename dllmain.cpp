#include "pch.h"
#include <windows.h>
#include <string>
#include <vector>
#include "MinHook.h"

#pragma comment(linker, "/export:GetFileVersionInfoA=C:\\Windows\\System32\\version.GetFileVersionInfoA")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=C:\\Windows\\System32\\version.GetFileVersionInfoByHandle")
#pragma comment(linker, "/export:GetFileVersionInfoExA=C:\\Windows\\System32\\version.GetFileVersionInfoExA")
#pragma comment(linker, "/export:GetFileVersionInfoExW=C:\\Windows\\System32\\version.GetFileVersionInfoExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeW")
#pragma comment(linker, "/export:GetFileVersionInfoW=C:\\Windows\\System32\\version.GetFileVersionInfoW")
#pragma comment(linker, "/export:VerFindFileA=C:\\Windows\\System32\\version.VerFindFileA")
#pragma comment(linker, "/export:VerFindFileW=C:\\Windows\\System32\\version.VerFindFileW")
#pragma comment(linker, "/export:VerInstallFileA=C:\\Windows\\System32\\version.VerInstallFileA")
#pragma comment(linker, "/export:VerInstallFileW=C:\\Windows\\System32\\version.VerInstallFileW")
#pragma comment(linker, "/export:VerLanguageNameA=KERNEL32.VerLanguageNameA")
#pragma comment(linker, "/export:VerLanguageNameW=KERNEL32.VerLanguageNameW")
#pragma comment(linker, "/export:VerQueryValueA=C:\\Windows\\System32\\version.VerQueryValueA")
#pragma comment(linker, "/export:VerQueryValueW=C:\\Windows\\System32\\version.VerQueryValueW")

// 数据结构
struct ModInfo {
    const char* name;
    const char* version;
    const char* author;
};

struct ModLoaderAPI {
    uintptr_t(*FindPattern)(const char* pattern);
    void(*Log)(const char* text);
};

typedef ModInfo* (*GetModInfoFn)();
typedef void    (*InitializeModFn)(ModLoaderAPI* api);

// 线程安全日志（Win32 原生，不依赖 C++ 运行时）
static HANDLE g_hLogFile = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION  g_logCS;

static void Log_Init()
{
    InitializeCriticalSection(&g_logCS);

    g_hLogFile = CreateFileA(
        "mod_loader.log",
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (g_hLogFile != INVALID_HANDLE_VALUE) {
        // 写 UTF-8 BOM
        DWORD written;
        const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        WriteFile(g_hLogFile, bom, 3, &written, NULL);
    }
}

static void Log_Write(const char* text)
{
    if (g_hLogFile == INVALID_HANDLE_VALUE) return;

    EnterCriticalSection(&g_logCS);

    // 获取当前本地时间
    SYSTEMTIME st;
    GetLocalTime(&st);

    // 格式化时间字符串：[HH:MM:SS.mmm]
    char timeBuf[32];
    wsprintfA(timeBuf, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds
    );

    // 写入顺序：时间戳 → 前缀 → 正文 → 换行
    const char prefix[] = "[ModLoader] ";
    const char suffix[] = "\r\n"; DWORD written;
    WriteFile(g_hLogFile, timeBuf, (DWORD)strlen(timeBuf), &written, NULL);
    WriteFile(g_hLogFile, prefix, (DWORD)(sizeof(prefix) - 1), &written, NULL);
    WriteFile(g_hLogFile, text, (DWORD)strlen(text), &written, NULL);
    WriteFile(g_hLogFile, suffix, (DWORD)(sizeof(suffix) - 1), &written, NULL);
    FlushFileBuffers(g_hLogFile);

    LeaveCriticalSection(&g_logCS);
}

// 传给Mod 的回调
static void LogWrapper(const char* text)
{
    Log_Write(text);
}

// AOB 扫描
extern "C" __declspec(dllexport) uintptr_t FindPattern(const char* pattern)
{
    // 解析特征码字符串为字节数组，-1 表示通配符
    std::vector<int> bytes;
    const char* cur = pattern;
    while (*cur != '\0')
    {
        if (*cur == ' ') { ++cur; continue; }

        if (*cur == '?')
        {
            bytes.push_back(-1);
            ++cur;
            if (*cur == '?') ++cur;
        }
        else
        {
            char* endPtr = nullptr;
            bytes.push_back((int)strtoul(cur, &endPtr, 16));
            cur = endPtr;
        }
    }

    // 扫描整个模块内存
    auto* dosHeader = (PIMAGE_DOS_HEADER)GetModuleHandle(NULL);
    auto* ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)dosHeader + dosHeader->e_lfanew); DWORD imageSize = ntHeaders->OptionalHeader.SizeOfImage;
    auto* scanBytes = (uint8_t*)dosHeader;

    size_t patSize = bytes.size();
    int* patData = bytes.data();

    for (DWORD i = 0; i < imageSize - (DWORD)patSize; ++i)
    {
        bool found = true;
        for (size_t j = 0; j < patSize; ++j)
        {
            if (patData[j] != -1 && scanBytes[i + j] != (uint8_t)patData[j])
            {
                found = false;
                break;
            }
        }
        if (found) return (uintptr_t)&scanBytes[i];
    }
    return 0;
}

// 加载 Mod 线程
static DWORD WINAPI LoadMods(LPVOID)
{
    // 等待游戏完成初始化
    Sleep(2000);

    Log_Init();
    Log_Write("=== Mod Loader Started ===");

    // 获取游戏根目录
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string dir = exePath;
    dir = dir.substr(0, dir.find_last_of("\\/"));

    std::string modsDir = dir + "\\mods";
    std::string searchPath = modsDir + "\\*.dll";

    Log_Write(("Searching for mods in: " + modsDir).c_str());

    // 检查 mods 文件夹
    DWORD attr = GetFileAttributesA(modsDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        Log_Write("ERROR: 'mods' folder not found.");
        return 0;
    }

    // 准备 API（static保证生命周期永久有效）
    static ModLoaderAPI api;
    api.FindPattern = FindPattern;
    api.Log = LogWrapper;

    //遍历所有 DLL
    WIN32_FIND_DATAA fd = {};
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        Log_Write("No DLLs found in mods folder.");
        return 0;
    }

    do
    {
        std::string dllName = fd.cFileName;
        std::string fullPath = modsDir + "\\" + dllName; HMODULE hMod = LoadLibraryA(fullPath.c_str());
        if (!hMod)
        {
            Log_Write(("Error: Could not load " + dllName).c_str());
            continue;
        }

        auto getInfo = (GetModInfoFn)GetProcAddress(hMod, "GetModInfo");
        if (!getInfo)
        {
            Log_Write(("REJECTED: " + dllName + " (Missing GetModInfo)").c_str());
            FreeLibrary(hMod);
            continue;
        }

        ModInfo* info = getInfo();
        std::string msg = "SUCCESS: Loaded [" + std::string(info->name) + "]" + " v" + std::string(info->version)
            + " by " + std::string(info->author);
        Log_Write(msg.c_str());

        auto initMod = (InitializeModFn)GetProcAddress(hMod, "InitializeMod");
        if (initMod)
        {
            initMod(&api);
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    Log_Write("--- Mod Load Sequence Finished ---");
    return 0;
}

// DLL 入口
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, LoadMods, NULL, 0, NULL);
    }
    return TRUE;
}