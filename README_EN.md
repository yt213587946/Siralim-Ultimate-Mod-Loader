# Siralim Ultimate Mod Loader (By 紫心醉梦)

A lightweight mod loading framework based on DLL hijacking (version.dll), no modification to the game itself required.

## 🎮 Players · Installation & Usage

### Requirements

- Windows 10 / 11 (64-bit)
- Siralim Ultimate
- Visual C++ Redistributable 2022 (x64)

### Installation Steps

**1. Place the framework files**

Copy the following file to the game root directory (same folder as `Siralim Ultimate.exe`):

```
Siralim Ultimate/
├── Siralim Ultimate.exe
├── version.dll   ← Framework main file, place here
└── ...
```

**2. Create the mods folder**

Create a new folder named `mods` in the game root directory:

```
Siralim Ultimate/
├── Siralim Ultimate.exe
├── version.dll
├── mods/         ← Create this folder
│   └── (Place mod .dll files here)
└── ...
```

**3. Install mods**

Put downloaded mod `.dll` files into the `mods` folder:

```
mods/
├── BetterGame.dll
├── AnotherMod.dll
└── ...
```

**4. Launch the game**

Launch the game normally via Steam. The framework will automatically load all mods inside the `mods` folder.

### Verification

A `mod_loader.log` log file will be generated in the game root directory. Open it with Notepad to confirm:

```
[2026-04-16 15:37:17.440] [ModLoader] === Mod Loader Started ===
[2026-04-16 15:37:17.446] [ModLoader] SUCCESS: Loaded [mod name] v1.0.0 by author
[2026-04-16 15:37:19.003] [ModLoader] --- Mod Load Sequence Finished ---
```

Seeing `SUCCESS: Loaded [Mod name]` means the mod loaded successfully.

### Uninstallation

- **Uninstall a single mod**: Delete the corresponding `.dll` from the `mods` folder.
- **Completely uninstall the framework**: Delete `version.dll` and the `mods` folder from the game root directory.

### Common Issues

**Issue**: `ERROR: 'mods' folder not found` appears in the log  
**Cause**: The `mods` folder was not created  
**Solution**: Manually create the `mods` folder

**Issue**: `REJECTED: xxx.dll (Missing GetModInfo)` appears in the log  
**Cause**: Mod file is corrupted or incompatible  
**Solution**: Contact the mod author

**Issue**: `Error: Could not load xxx.dll` appears in the log  
**Cause**: Missing runtime dependencies for the mod  
**Solution**: Install VC++ Redistributable

**Issue**: No `mod_loader.log` file is generated  
**Cause**: `version.dll` is not placed correctly  
**Solution**: Ensure it is in the same directory as the `.exe`

## 🔧 Mod Creators · Framework Integration Guide

### Project Configuration

Create a **64-bit Windows DLL** project (MSVC / MinGW both work):

- Platform: `x64`
- Output type: `.dll`
- Character set: `Multi-byte` or `Unicode` (both fine)

### Required Export Functions

The framework identifies and initializes mods through the following two export functions. **Mods missing `GetModInfo` will be rejected.**

#### 1. `GetModInfo` — Mod metadata (Required)

```cpp
#include <windows.h>

struct ModInfo {
    const char* name;      // Mod name
    const char* version;   // Version number
    const char* author;    // Author name
};

extern "C" __declspec(dllexport) ModInfo* GetModInfo()
{
    static ModInfo info = {
        "Better Game",   // Mod name
        "1.1",           // Version number
        "YourName"       // Author name
    };
    return &info;
}
```

#### 2. `InitializeMod` — Initialization entry (Optional but usually necessary)

After loading the mod, the framework calls this function and passes a `ModLoaderAPI` pointer for the mod to use:

```cpp
struct ModLoaderAPI {
    uintptr_t (*FindPattern)(const char* pattern);   // AOB signature scan
    void (*Log)(const char* text);                   // Write to framework log
};

extern "C" __declspec(dllexport) void InitializeMod(ModLoaderAPI* api)
{
    // Put all initialization logic here
    api->Log("Hello from MyMod!");
}
```

### API Usage

**api->Log(text);** // Log output

Writes information to `mod_loader.log`, automatically appends timestamp and `[ModLoader]` prefix.

Output example:
```
[2026-04-16 15:37:18.816] [ModLoader] Mod initialized successfully.
```

**api->FindPattern(pattern);** // AOB signature scan

Searches for a signature in the main game module's memory, returns the matching address, or 0 if not found:

```cpp
// Use ? or ?? as wildcards
uintptr_t addr = api->FindPattern("48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 8B 40");

if (addr == 0) {
    api->Log("ERROR: Pattern not found, game version mismatch?");
    return;
}

api->Log(("Pattern found at: " + std::to_string(addr)).c_str());
```

### Complete Mod Template

```cpp
#include <windows.h>
#include <string>

// ---- Framework data structures (keep consistent with framework) ----

struct ModInfo {
    const char* name;
    const char* version;
    const char* author;
};

struct ModLoaderAPI {
    uintptr_t (*FindPattern)(const char* pattern);
    void (*Log)(const char* text);
};

// ---- Global variables ----

static ModLoaderAPI* g_api = nullptr;

// ---- Helper functions ----

static void Log(const std::string& msg)
{
    if (g_api) g_api->Log(msg.c_str());
}

// ---- Your mod logic ----

static void ApplyPatches()
{
    // Example: AOB scan and memory modification
    uintptr_t addr = g_api->FindPattern("48 8B 05 ?? ?? ?? ?? 48 85 C0");
    if (addr == 0) {
        Log("ERROR: Target pattern not found.");
        return;
    }

    Log("Pattern found, applying patch...");

    // Unprotect memory and write
    DWORD oldProtect;
    VirtualProtect((LPVOID)addr, 8, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Modify memory here...

    VirtualProtect((LPVOID)addr, 8, oldProtect, &oldProtect);
    Log("Patch applied successfully.");
}

// ---- Export functions ----

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

### Loading Sequence

Understanding the framework's call order helps avoid timing issues:

```
Game starts
    ↓
version.dll is loaded (DLL_PROCESS_ATTACH)
    ↓
Wait 2000ms (game engine initialization)
    ↓
Iterate over mods/*.dll
    ↓
    ├─ LoadLibrary(mod.dll)
    ├─ Call GetModInfo()        ← Get metadata, reject if missing
    └─ Call InitializeMod(api)  ← Pass API, execute mod initialization
```

**Note**: `InitializeMod` runs in a separate thread. If you need delayed operations (waiting for specific game systems to be ready), call `Sleep()` or create a new thread inside your mod.

### Important Notes

**Export function naming**
Must use `extern "C"` to prevent C++ name mangling

**Memory modification**
Always use `VirtualProtect` to remove page protection before writing

**String lifetime**
String pointers in `ModInfo` must be static or global, never point to stack variables

**Game version compatibility**
AOB signatures may break with game updates; log clear errors when scanning fails

**Multiple mod coexistence**
The framework loads mods in alphabetical order by file name. Mods modifying the same memory address may conflict with each other.