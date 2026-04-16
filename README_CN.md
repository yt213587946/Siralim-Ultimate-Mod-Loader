# Siralim Ultimate Mod Loader(By 紫心醉梦)


一个基于 DLL 劫持（version.dll）的轻量级 Mod 加载框架，无需修改游戏本体。

### 环境要求

* Windows 10 / 11（64 位）
* Siralim Ultimate（Steam 正版）
* Visual C++ Redistributable 2022（x64）

### 安装步骤

**1. 放置框架文件**

将以下文件复制到游戏根目录（与 `Siralim Ultimate.exe` 同级）：

```cpp
Siralim Ultimate/
├── Siralim Ultimate.exe
├── version.dll          ← 框架主文件，放在这里
└── ...
```


**2. 创建 mods 文件夹**

在游戏根目录下新建 `mods` 文件夹：
    
```cpp
Siralim Ultimate/
├── Siralim Ultimate.exe
├── version.dll├── mods/                ← 新建此文件夹
│   └── （将Mod 的 .dll 放在这里）
└── ...
```

**3. 安装 Mod**

将下载的 Mod `.dll` 文件放入 `mods` 文件夹：

```cpp
mods/
├── AnotherMod.dll
└── ...
```


**4. 启动游戏**

正常通过 Steam 启动游戏，框架会自动加载 `mods` 文件夹内所有 Mod。

### 验证是否生效

游戏根目录会生成 `mod_loader.log` 日志文件，用记事本打开确认：

`[2026-04-16 15:37:17.440] [ModLoader] === Mod Loader Started ===`
`[2026-04-16 15:37:17.446] [ModLoader] SUCCESS: Loaded [AnotherMod] version 1.1 by author`
`[2026-04-16 15:37:19.003] [ModLoader] --- Mod Load Sequence Finished ---`

看到 `SUCCESS: Loaded [Mod名]` 即表示加载成功。



### 卸载

* **卸载单个 Mod**：从 `mods` 文件夹删除对应`.dll`
* **完全卸载框架**：删除游戏根目录的 `version.dll` 和 `mods` 文件夹

### 常见问题

**问题：日志中出现 `ERROR: 'mods' folder not found`**

原因：未创建 mods 文件夹

解决：手动创建 `mods` 文件夹



**问题：日志中出现 `REJECTED: xxx.dll (Missing GetModInfo)`**

原因：Mod 文件损坏或不兼容

解决：联系该 Mod 作者



**问题：日志中出现 `Error: Could not load xxx.dll`**

原因：缺少 Mod 的依赖运行库

解决：安装 VC++ Redistributable



**问题：没有生成 `mod_loader.log`**

原因：`version.dll` 未正确放置

解决：确认与`.exe` 同目录



## 🔧 Mod 制作者 · 框架对接指南

### 项目配置

创建一个 **64 位 Windows DLL** 项目（MSVC / MinGW 均可）：

* 平台：`x64`
* 输出类型：`.dll`
* 字符集：`多字节字符集`或 `Unicode` 均可

### 必须实现的导出函数

框架通过以下两个导出函数识别和初始化 Mod，**缺少`GetModInfo` 将被拒绝加载**。

#### 1. `GetModInfo` — Mod 元信息（必须）

```cpp
#include <windows.h>

struct ModInfo {
const char* name;      // Mod 名称
const char* version;   // 版本号
const char* author;    // 作者名};

extern "C" declspec(dllexport) ModInfo* GetModInfo()
{
static ModInfo info = {
"Better Game",   // Mod 名称
"1.1",           // 版本号
"YourName"       // 作者名
};
return &info;
}
```

#### 2. `InitializeMod` — 初始化入口（可选，但通常必要）

框架加载 Mod 后会调用此函数，并传入 `ModLoaderAPI` 供 Mod 使用：

```cpp
struct ModLoaderAPI {
uintptr_t (*FindPattern)(const char* pattern);  // AOB 特征码扫描
void      (*Log)(const char* text);                   // 写入框架日志
};

&#x20;   extern "C" declspec(dllexport) void InitializeMod(ModLoaderAPI* api)
{
// 所有初始化逻辑写在这里
api->Log("Hello from MyMod!");
}

```

### API 使用说明

#### `api->Log(text)` · 日志输出

将信息写入 `mod_loader.log`，自动附加时间戳和`[ModLoader]` 前缀：

```cpp
api->Log("Mod initialized successfully.");
api->Log(("Player HP: " + std::to_string(hp)).c_str());
``` 

输出效果：
`[2026-04-16 15:37:18.816] [ModLoader] Mod initialized successfully.`

#### `api->FindPattern(pattern)` · AOB 特征码扫描

在游戏主模块内存中搜索特征码，返回匹配地址，未找到返回 `0`：

```cpp
// 使用 ? 或 ?? 作为通配符
uintptr_t addr = api->FindPattern("48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ??8B 40");

if (addr == 0) {
api->Log("ERROR: Pattern not found, game version mismatch?");
return;
}

api->Log(("Pattern found at: " + std::to_string(addr)).c_str());

// 在此修改内存...
```

### 完整 Mod 模板


```cpp
#include <windows.h>
#include <string>

//---- 框架数据结构（与框架保持一致）----
struct ModInfo {
const char* name;
const char* version;
const char* author;
};

struct ModLoaderAPI {
uintptr_t (*FindPattern)(const char* pattern);
void      (*Log)(const char* text);
};

// ---- 全局变量 ----

static ModLoaderAPI* g_api = nullptr;

// ---- 工具函数 ----

static void Log(const std::string& msg)
{
if (g_api) g_api->Log(msg.c_str());
}

// ---- 你的 Mod 逻辑 ----

static void ApplyPatches()
{
// 示例：AOB 扫描并修改内存
uintptr_t addr = g_api->FindPattern("48 8B 05 ?? ?? ?? ?? 48 85 C0");
if (addr == 0) {
Log("ERROR: Target pattern not found.");
return;
}

&#x20;   Log("Pattern found, applying patch...");

    // 解除内存保护并写入
    DWORD oldProtect;
    VirtualProtect((LPVOID)addr, 8, PAGE_EXECUTE_READWRITE, &oldProtect);

    // 在此修改内存...

    VirtualProtect((LPVOID)addr, 8, oldProtect, &oldProtect);
    Log("Patch applied successfully.");

}

// ---- 导出函数 ----

extern "C" __declspec(dllexport) ModInfo* GetModInfo()
{
static ModInfo info = { "My Mod", "1.0", "YourName" };
return &info;
}

extern "C" __declspec(dllexport) void InitializeMod(ModLoaderAPI* api)
{
g_api = api;
Log("Initializing...");
ApplyPatches();
}
```

### 加载时序

```cpp
游戏启动
↓
version.dll 被加载（DLL_PROCESS_ATTACH）
↓
等待 2000ms（游戏引擎初始化）
↓
遍历 mods*.dll
↓
├─ LoadLibrary(mod.dll)
├─ 调用 GetModInfo()         ← 获取元信息，失败则拒绝
└─ 调用 InitializeMod(api)   ← 传入 API，执行 Mod 初始化
```


⚠️**注意**：`InitializeMod` 运行在独立线程中，若需要延迟操作（等待游戏特定系统就绪），请在 Mod 内部自行 `Sleep()` 或创建新线程。

### 注意事项

**导出函数命名**

必须使用 `extern "C"` 防止 C++ 名称修饰

**内存修改**

修改前务必用 `VirtualProtect` 解除页保护

**字符串生命周期**

`ModInfo` 中的字符串指针需为 `static` 或全局，不可指向栈变量

**游戏版本兼容**

AOB 特征码可能随游戏更新失效，建议在扫描失败时输出明确错误日志

**多 Mod 共存**

框架按文件名字母顺序加载，修改同一内存地址的 Mod 之间可能产生冲突

