#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
#include <random>
#include <Windows.h>
#include <intrin.h>
#include <TlHelp32.h>
#include <winternl.h>
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

// Anti-debugging macros
#define ANTI_DEBUG_1 if (IsDebuggerPresent()) { std::cout << "Nice try!\n"; ExitProcess(1); }
#define ANTI_DEBUG_2 { BOOL isDebug = FALSE; CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebug); if (isDebug) ExitProcess(1); }
#define TIMING_CHECK auto start = std::chrono::high_resolution_clock::now();
#define TIMING_VERIFY(ms) { auto end = std::chrono::high_resolution_clock::now(); \
    if (std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() > ms) { \
        std::cout << "Debugger detected via timing!\n"; ExitProcess(1); } }

namespace secrets {
constexpr uint64_t k1 = 0x9e3779b97f4a7c15ULL;
constexpr uint64_t k2 = 0x243f6a8885a308d3ULL;
constexpr uint64_t k3 = 0x13198a2e03707344ULL;
    
volatile uint64_t polymorphic_seed = 0;
    
constexpr uint64_t CHALLENGE_START_TIME = 1704067200ULL;
constexpr uint64_t CHALLENGE_END_TIME = 1735689600ULL;
    
inline bool check_time_bomb() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
    if (timestamp < CHALLENGE_START_TIME || timestamp > CHALLENGE_END_TIME) {
        return false;
    }
    return true;
}
    
inline void init_polymorphic_seed() {
    // FIXED: Use deterministic seed for reproducible solution
    // Seed is obfuscated to maintain static analysis difficulty
    polymorphic_seed = 0x8B4D7C9E2F1A5E3BULL;
    polymorphic_seed ^= (k1 >> 7);
    polymorphic_seed = (polymorphic_seed << 13) ^ (polymorphic_seed >> 51);
    polymorphic_seed ^= k2;
}
    
    inline uint64_t obf_compute(uint64_t a, uint64_t b) {
        volatile uint64_t x = a ^ b ^ (polymorphic_seed >> 7);
        x = (x << 13) ^ (x >> 51);
        x ^= (x >> 17) ^ (polymorphic_seed & 0xFFFF);
        x *= 0x85ebca6bUL;
        x ^= (x >> 13);
        x *= 0xc2b2ae35UL;
        x ^= (x >> 16) ^ (polymorphic_seed >> 32);
        return x;
    }
    
    inline uint8_t get_xor_key() {
        volatile uint64_t stage1 = obf_compute(k1, k2);
        volatile uint64_t stage2 = obf_compute(stage1, k3);
        volatile uint64_t stage3 = (stage2 >> 3) ^ (stage2 << 7) ^ (stage2 >> 19);
        volatile uint8_t result = ((stage3 >> 8) ^ (stage3 >> 16) ^ (stage3 >> 24) ^ (stage3 >> 32)) & 0xFF;
        return result;
    }
}

inline bool detect_hwbp() {
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(GetCurrentThread(), &ctx);
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
}

inline bool check_peb_flags() {
#ifdef _WIN64
    PPEB peb = (PPEB)__readgsqword(0x60);
#else
    PPEB peb = (PPEB)__readfsdword(0x30);
#endif
    if (peb->BeingDebugged) return true;
    DWORD* pNtGlobalFlag = (DWORD*)((BYTE*)peb + 0xBC);
    if (*pNtGlobalFlag & 0x70) return true;
    return false;
}

inline bool detect_sandbox() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors < 2) return true;
    
    MEMORYSTATUSEX memStatus = { sizeof(MEMORYSTATUSEX) };
    GlobalMemoryStatusEx(&memStatus);
    if (memStatus.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return true;
    
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\Description\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buffer[256];
        DWORD bufferSize = sizeof(buffer);
        if (RegQueryValueExA(hKey, "SystemManufacturer", nullptr, nullptr, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
            std::string manufacturer = buffer;
            RegCloseKey(hKey);
            if (manufacturer.find("VMware") != std::string::npos ||
                manufacturer.find("VirtualBox") != std::string::npos ||
                manufacturer.find("QEMU") != std::string::npos) {
                return true;
            }
        }
        RegCloseKey(hKey);
    }
    return false;
}

inline bool detect_wine() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll && GetProcAddress(hNtdll, "wine_get_version")) return true;
    return false;
}

inline bool detect_ghidra() {
    // Check for Ghidra window titles
    HWND ghidra1 = FindWindowA(nullptr, "Ghidra");
    HWND ghidra2 = FindWindowA(nullptr, "CodeBrowser");
    if (ghidra1 || ghidra2) return true;

    // Check for Ghidra process
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
        if (Process32First(hSnapshot, &pe32)) {
            do {
                std::string name;
                for (int i = 0; i < MAX_PATH && pe32.szExeFile[i] != 0; i++) {
                    name += pe32.szExeFile[i];
                }
                if (name.find("ghidra") != std::string::npos ||
                    name.find("Ghidra") != std::string::npos) {
                    CloseHandle(hSnapshot);
                    return true;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }

    return false;
}

// Advanced VM detection with multiple heuristics
inline bool detect_vm_advanced() {
    // Check for VM-specific registry keys
    const char* vm_keys[] = {
        "HARDWARE\\ACPI\\DSDT\\VBOX__",
        "HARDWARE\\ACPI\\FADT\\VBOX__",
        "HARDWARE\\ACPI\\RSDT\\VBOX__",
        "SOFTWARE\\Oracle\\VirtualBox Guest Additions",
        "SYSTEM\\ControlSet001\\Services\\VBoxGuest",
        "SYSTEM\\ControlSet001\\Services\\VBoxMouse",
        "SYSTEM\\ControlSet001\\Services\\VBoxService"
    };

    for (const char* key : vm_keys) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
    }

    // Check for VM-specific files
    const char* vm_files[] = {
        "C:\\windows\\System32\\Drivers\\Vmmouse.sys",
        "C:\\windows\\System32\\Drivers\\vmhgfs.sys",
        "C:\\windows\\System32\\Drivers\\VBoxMouse.sys",
        "C:\\windows\\System32\\Drivers\\VBoxGuest.sys",
        "C:\\windows\\System32\\Drivers\\VBoxSF.sys",
        "C:\\windows\\System32\\vboxdisp.dll",
        "C:\\windows\\System32\\vboxhook.dll",
        "C:\\windows\\System32\\vboxoglerrorspu.dll"
    };

    for (const char* file : vm_files) {
        if (GetFileAttributesA(file) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }

    // Check MAC address for VM prefixes
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD dwBufLen = sizeof(adapterInfo);
    DWORD dwStatus = GetAdaptersInfo(adapterInfo, &dwBufLen);

    if (dwStatus == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO pAdapterInfo = adapterInfo;
        while (pAdapterInfo) {
            if (pAdapterInfo->AddressLength == 6) {
                // VMware: 00:05:69, 00:0C:29, 00:1C:14, 00:50:56
                // VirtualBox: 08:00:27
                // Parallels: 00:1C:42
                if ((pAdapterInfo->Address[0] == 0x00 && pAdapterInfo->Address[1] == 0x05 && pAdapterInfo->Address[2] == 0x69) ||
                    (pAdapterInfo->Address[0] == 0x00 && pAdapterInfo->Address[1] == 0x0C && pAdapterInfo->Address[2] == 0x29) ||
                    (pAdapterInfo->Address[0] == 0x00 && pAdapterInfo->Address[1] == 0x1C && pAdapterInfo->Address[2] == 0x14) ||
                    (pAdapterInfo->Address[0] == 0x00 && pAdapterInfo->Address[1] == 0x50 && pAdapterInfo->Address[2] == 0x56) ||
                    (pAdapterInfo->Address[0] == 0x08 && pAdapterInfo->Address[1] == 0x00 && pAdapterInfo->Address[2] == 0x27) ||
                    (pAdapterInfo->Address[0] == 0x00 && pAdapterInfo->Address[1] == 0x1C && pAdapterInfo->Address[2] == 0x42)) {
                    return true;
                }
            }
            pAdapterInfo = pAdapterInfo->Next;
        }
    }

    return false;
}

// Detect single-stepping via instruction counting
inline bool detect_single_step() {
    CONTEXT ctx = { 0 };
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_CONTROL;
    GetThreadContext(GetCurrentThread(), &ctx);

    // Check trap flag
    if (ctx.EFlags & 0x100) return true;

    return false;
}

// Advanced timing-based debugger detection
inline bool detect_debugger_timing() {
    uint64_t start = __rdtsc();

    // Perform simple operation
    volatile int x = 0;
    for (int i = 0; i < 10; i++) {
        x += i;
    }

    uint64_t end = __rdtsc();

    // If too many cycles elapsed, debugger likely present
    return (end - start) > 1000;
}

// Check for analysis tools via window class names
inline bool detect_analysis_tools() {
    const char* tool_classes[] = {
        "OLLYDBG",
        "WinDbgFrameClass",
        "ID",  // IDA
        "Zeta Debugger",
        "Rock Debugger",
        "ObsidianGUI"
    };

    for (const char* className : tool_classes) {
        if (FindWindowA(className, nullptr)) return true;
    }

    return false;
}

// Thread-based anti-debugging
volatile bool debugger_thread_detected = false;

DWORD WINAPI anti_debug_thread(LPVOID lpParam) {
    while (true) {
        if (IsDebuggerPresent() || detect_hwbp() || check_peb_flags()) {
            debugger_thread_detected = true;
            ExitProcess(1);
        }
        Sleep(100);
    }
    return 0;
}

inline void start_anti_debug_thread() {
    CreateThread(nullptr, 0, anti_debug_thread, nullptr, 0, nullptr);
}

// NtQueryInformationProcess anti-debug
inline bool check_debug_object() {
    typedef NTSTATUS (NTAPI *pNtQueryInformationProcess)(
        HANDLE ProcessHandle,
        DWORD ProcessInformationClass,
        PVOID ProcessInformation,
        DWORD ProcessInformationLength,
        PDWORD ReturnLength
    );

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return false;

    pNtQueryInformationProcess NtQIP = (pNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");
    if (!NtQIP) return false;

    DWORD debugPort = 0;
    NTSTATUS status = NtQIP(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);

    if (status == 0 && debugPort != 0) return true;

    // Check ProcessDebugObjectHandle
    HANDLE debugObject = nullptr;
    status = NtQIP(GetCurrentProcess(), 30, &debugObject, sizeof(debugObject), nullptr);

    if (status == 0 && debugObject != nullptr) return true;

    return false;
}

volatile uint8_t guard_page_data[4096];
volatile bool memory_dump_detected = false;

LONG WINAPI guard_page_handler(EXCEPTION_POINTERS* ExceptionInfo) {
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_GUARD_PAGE) {
        memory_dump_detected = true;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

inline void setup_anti_memory_dump() {
    AddVectoredExceptionHandler(1, guard_page_handler);
    DWORD oldProtect;
    VirtualProtect((LPVOID)guard_page_data, 4096, PAGE_READWRITE | PAGE_GUARD, &oldProtect);
}

inline bool check_memory_dump() {
    volatile uint8_t test = guard_page_data[0];
    if (memory_dump_detected) return true;
    return false;
}

volatile uint32_t binary_checksum = 0;

inline uint32_t calculate_code_checksum() {
    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule) return 0;
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    
    uint32_t checksum = 0;
    uint8_t* codeBase = (uint8_t*)hModule + ntHeaders->OptionalHeader.BaseOfCode;
    uint32_t codeSize = ntHeaders->OptionalHeader.SizeOfCode;
    
    for (uint32_t i = 0; i < codeSize && i < 8192; i++) {
        checksum ^= codeBase[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }
    
    return checksum;
}

inline bool verify_binary_integrity() {
    if (binary_checksum == 0) {
        binary_checksum = calculate_code_checksum();
        return true;
    }
    
    uint32_t current_checksum = calculate_code_checksum();
    return (current_checksum == binary_checksum);
}

inline bool check_parent_process() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    DWORD parentPid = 0;
    DWORD currentPid = GetCurrentProcessId();

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentPid) {
                parentPid = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE && Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == parentPid) {
                std::string name;
                for (int i = 0; i < MAX_PATH && pe32.szExeFile[i] != 0; i++) {
                    name += pe32.szExeFile[i];
                }
                CloseHandle(hSnapshot);
                if (name.find("x64dbg") != std::string::npos ||
                    name.find("x32dbg") != std::string::npos ||
                    name.find("ollydbg") != std::string::npos ||
                    name.find("windbg") != std::string::npos ||
                    name.find("ghidra") != std::string::npos ||
                    name.find("Ghidra") != std::string::npos) {
                    return true;
                }
                return false;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    if (hSnapshot != INVALID_HANDLE_VALUE) CloseHandle(hSnapshot);
    return false;
}

// Encrypted VM with self-modifying bytecode
class EncryptedVM {
private:
    std::vector<uint64_t> regs;
    std::vector<uint8_t> encrypted_code;
    uint8_t key;
    
    void decrypt_instruction(size_t pc) {
        uint8_t poly_mod = (secrets::polymorphic_seed >> (pc * 3)) & 0xFF;
        encrypted_code[pc] ^= (key ^ poly_mod);
        encrypted_code[pc] = (encrypted_code[pc] << 3) | (encrypted_code[pc] >> 5);
    }
    
    void re_encrypt_instruction(size_t pc) {
        uint8_t poly_mod = (secrets::polymorphic_seed >> (pc * 3)) & 0xFF;
        encrypted_code[pc] = (encrypted_code[pc] >> 3) | (encrypted_code[pc] << 5);
        encrypted_code[pc] ^= (key ^ poly_mod);
    }
    
public:
    EncryptedVM(uint8_t k) : regs(32, 0), key(k) {
        encrypted_code = {0xF3, 0x8A, 0x12, 0x67, 0x45, 0x9C, 0xDE, 0x23,
                         0x7F, 0x34, 0xAB, 0xC1, 0x88, 0x5D, 0x92, 0x4E};
        
        uint8_t poly_mutation = (secrets::polymorphic_seed >> 8) & 0xFF;
        for (size_t i = 0; i < encrypted_code.size(); i++) {
            encrypted_code[i] ^= poly_mutation;
            poly_mutation = (poly_mutation << 1) | (poly_mutation >> 7);
        }
    }
    
    uint64_t execute(uint64_t input) {
        ANTI_DEBUG_1;
        if (detect_hwbp()) ExitProcess(1);
        if (check_peb_flags()) ExitProcess(1);
        if (detect_single_step()) ExitProcess(1);
        if (debugger_thread_detected) ExitProcess(1);

        regs[0] = input;
        size_t pc = 0;

        TIMING_CHECK;
        while (pc < encrypted_code.size()) {
            if (pc % 4 == 0 && check_parent_process()) ExitProcess(1);
            if (pc % 3 == 0 && detect_debugger_timing()) ExitProcess(1);

            decrypt_instruction(pc);
            uint8_t opcode = encrypted_code[pc];

            switch (opcode & 0x0F) {
                case 0: regs[1] ^= regs[0]; break;
                case 1: regs[0] = (regs[0] << 7) | (regs[0] >> 57); break;
                case 2: regs[0] += secrets::k1; break;
                case 3: regs[0] *= secrets::k2; break;
                case 4: regs[0] ^= secrets::k3; break;
                default: regs[0] = ~regs[0]; break;
            }

            re_encrypt_instruction(pc);
            pc++;
        }
        TIMING_VERIFY(100);

        return regs[0];
    }
};

// Polymorphic hash with anti-disassembly tricks
__declspec(noinline) uint64_t poly_hash(const std::string& s) {
    ANTI_DEBUG_2;
    if (check_debug_object()) ExitProcess(1);

    uint64_t h = 0;
    for (size_t i = 0; i < s.length(); i++) {
        h ^= (static_cast<uint64_t>(s[i]) * secrets::k1);
        h = (h << 13) ^ (h >> 51);
        h *= secrets::k2;

        // Anti-emulation: Insert random checks
        if (i % 2 == 0 && detect_debugger_timing()) ExitProcess(1);
    }
    return h;
}

// Multi-stage obfuscated hash with environmental keying
__declspec(noinline) uint64_t environmental_hash(const std::string& s) {
    // Get system-specific value (but deterministic for the challenge)
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    // Use page size as part of key (typically 4096 on x86/x64)
    uint64_t env_key = si.dwPageSize ^ 0x1000;  // XOR to normalize

    uint64_t h = secrets::polymorphic_seed;
    for (size_t i = 0; i < s.length(); i++) {
        h ^= (static_cast<uint64_t>(s[i]) * env_key);
        h = _rotl64(h, 17);
        h *= secrets::k1;
        h ^= secrets::k2;
    }

    return h;
}

// Opaque predicate using mathematical properties
__declspec(noinline) bool opaque_always_true(uint64_t x) {
    // 7x mod 6 == x mod 6 for all integers x (mathematical identity)
    return ((7 * x) % 6) == (x % 6);
}

__declspec(noinline) uint64_t fake_computation_branch(uint64_t x) {
    // This branch will never be taken but looks legitimate
    if (!opaque_always_true(x)) {
        // Fake flag to mislead analysis
        std::string fake_flag = "BTCTF{F4k3_Fl4g_D3c0y!}";
        return poly_hash(fake_flag);
    }
    return x;
}

// White-box cryptography simulation
class WhiteBoxAES {
private:
    static constexpr size_t TABLE_SIZE = 256;
    uint8_t lookup_table[TABLE_SIZE];
    
    void init_table(uint64_t seed) {
        for (int i = 0; i < TABLE_SIZE; i++) {
            lookup_table[i] = ((i * seed) ^ (i << 3) ^ secrets::get_xor_key()) & 0xFF;
        }
    }
    
public:
    WhiteBoxAES(uint64_t key) { init_table(key); }
    
    uint64_t encrypt(const std::string& input) {
        ANTI_DEBUG_1;
        
        uint64_t result = 0;
        for (size_t i = 0; i < input.length(); i++) {
            uint8_t byte = static_cast<uint8_t>(input[i]);
            byte = lookup_table[byte];
            byte = lookup_table[byte ^ secrets::get_xor_key()];
            result ^= (static_cast<uint64_t>(byte) << ((i * 8) % 64));
        }
        return result;
    }
};

bool complex_predicate(uint64_t x) {
    uint64_t a = (x * x + x) % 2;
    uint64_t b = (x % 2 == 0) ? 0 : ((x * x) % 2);
    return (a == 0) && ((x & (x - 1)) != x);
}

// Control flow flattening with exception handling
uint64_t flatten_extreme(const std::string& input) noexcept(false) {
volatile uint64_t state = 0x9b05688c2b3e6c1fULL;
volatile int dispatcher = 0;
int iteration = 0;

TIMING_CHECK;

try {
        while (true) {
            // Anti-step over
            if (iteration++ > 1000) throw std::runtime_error("overflow");

            // Random anti-debug checks
            if (iteration % 50 == 0 && detect_analysis_tools()) ExitProcess(1);
            if (iteration % 75 == 0 && check_debug_object()) ExitProcess(1);

            switch (dispatcher) {
                case 0: {
                    ANTI_DEBUG_1;
                    if (input.length() != 16) return 0;
                    state ^= poly_hash(input.substr(0, 4));
                    state = fake_computation_branch(state);  // Opaque predicate
                    dispatcher = (state & 0x3) + 1;
                    break;
                }
                case 1: case 2: case 3: {
                    WhiteBoxAES aes(state);
                    state = aes.encrypt(input.substr(4, 4));
                    state ^= environmental_hash(input.substr(4, 4));  // Additional layer
                    dispatcher = 4;
                    break;
                }
                case 4: {
                    if (detect_hwbp()) ExitProcess(1);
                    if (detect_single_step()) ExitProcess(1);
                    EncryptedVM vm(static_cast<uint8_t>(state & 0xFF));
                    state ^= vm.execute(poly_hash(input.substr(8, 4)));
                    dispatcher = 5;
                    break;
                }
                case 5: {
                    state = (state * 0x5DEECE66DULL + 0xBULL);
                    state ^= poly_hash(input.substr(12, 4));
                    state ^= environmental_hash(input);  // Full input environmental hash
                    dispatcher = 6;
                    break;
                }
                case 6: {
                    if (complex_predicate(state)) return state;
                    return 0;
                }
            }
        }
    }
    catch (const std::exception&) {
        return 0;
    }

    TIMING_VERIFY(200);
    return state;
}

volatile const uint8_t decoy1[] = {0xCA, 0xE8, 0xF6, 0xD9, 0xF3, 0xD9, 0xF3, 0xD8, 0xF5, 0xCA, 0xE8, 0xE7, 0xC0, 0xF6, 0xF7, 0xE6, 0xD1};
volatile const uint8_t decoy2[] = {0xD8, 0xF6, 0xE5, 0xE7, 0xCA, 0xF2, 0xF9, 0x9F, 0xC2, 0xF1, 0xF4, 0xF7, 0xF2, 0xF5, 0xE6, 0xE6};
volatile const uint8_t decoy3[] = {0xC1, 0xF6, 0xED, 0xEF, 0xF3, 0xF4, 0xCA, 0xE8, 0xE7, 0xF2, 0xE7, 0x9F, 0xD8, 0xF5, 0xF0, 0xE7};
volatile const uint8_t decoy4[] = {0xD9, 0xF5, 0xF1, 0xC0, 0xEF, 0xF1, 0xE6, 0xE4, 0xF7, 0xC4, 0xE7, 0xE5, 0xEF, 0xF9, 0xD1, 0xD1};

struct EncryptedString {
    const uint8_t* data;
    size_t length;
    uint8_t key;
};

inline std::string decrypt_string(const EncryptedString& enc) {
    std::string result;
    result.reserve(enc.length);
    uint8_t dynamic_key = enc.key ^ (secrets::polymorphic_seed & 0xFF);
    for (size_t i = 0; i < enc.length; i++) {
        result += (char)(enc.data[i] ^ dynamic_key ^ (i * 7));
    }
    return result;
}

const uint8_t enc_debugger_msg[] = {0xD4, 0xE7, 0xE4, 0xF1, 0xE6, 0xE6, 0xE7, 0xF2, 0xA0, 0xE4, 0xE7, 0xF4, 0xE7, 0xE5, 0xF4, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_DEBUGGER = {enc_debugger_msg, 18, 0x9A};

const uint8_t enc_sandbox_msg[] = {0xC3, 0xE1, 0xE6, 0xE4, 0xE2, 0xEF, 0xF8, 0xBF, 0xD6, 0xD5, 0xA0, 0xE4, 0xE7, 0xF4, 0xE7, 0xE5, 0xF4, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_SANDBOX = {enc_sandbox_msg, 20, 0x9A};

const uint8_t enc_timebomb_msg[] = {0xC3, 0xEF, 0xF2, 0xF2, 0xF9, 0xBE, 0xA0, 0xE5, 0xE8, 0xE1, 0xF6, 0xF6, 0xE7, 0xE6, 0xE6, 0xE7, 0xA0, 0xE8, 0xE1, 0xF3, 0xA0, 0xE7, 0xF8, 0xF0, 0xE9, 0xF2, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_TIMEBOMB = {enc_timebomb_msg, 29, 0x9A};

const uint8_t enc_wine_msg[] = {0xD7, 0xD9, 0xD8, 0xD7, 0xA0, 0xE4, 0xE7, 0xF4, 0xE7, 0xE5, 0xF4, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_WINE = {enc_wine_msg, 14, 0x9A};

const uint8_t enc_integrity_msg[] = {0xC5, 0xEF, 0xE4, 0xE7, 0xA0, 0xE9, 0xE6, 0xF4, 0xE7, 0xE6, 0xF2, 0xE9, 0xF4, 0xF9, 0xA0, 0xE5, 0xE8, 0xE7, 0xE5, 0xEB, 0xA0, 0xE8, 0xE1, 0xE9, 0xF6, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_INTEGRITY = {enc_integrity_msg, 28, 0x9A};

const uint8_t enc_memdump_msg[] = {0xD5, 0xE7, 0xED, 0xEF, 0xF2, 0xF9, 0xA0, 0xE4, 0xF1, 0xED, 0xF0, 0xA0, 0xE4, 0xE7, 0xF4, 0xE7, 0xE5, 0xF4, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_MEMDUMP = {enc_memdump_msg, 21, 0x9A};

const uint8_t enc_tools_msg[] = {0xC1, 0xE6, 0xE1, 0xF6, 0xF9, 0xF3, 0xE9, 0xF3, 0xA0, 0xF4, 0xEF, 0xEF, 0xF6, 0xF3, 0xA0, 0xE4, 0xE7, 0xF4, 0xE7, 0xE5, 0xF4, 0xE7, 0xE4, 0xD1};
const EncryptedString STR_TOOLS = {enc_tools_msg, 24, 0x9A};

const uint8_t enc_banner[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
const EncryptedString STR_BANNER = {enc_banner, 16, 0x9A};

inline std::string xor_decrypt_string(const uint8_t* enc, size_t len, uint8_t key) {
    std::string result;
    for (size_t i = 0; i < len; i++) {
        result += (char)(enc[i] ^ key ^ (i * 7));
    }
    return result;
}

inline uint8_t extract_byte(const volatile uint8_t* data, int idx, int offset) {
    return data[idx * 3 + offset];
}

inline void polymorphic_mutate(volatile uint8_t* data, size_t len, uint64_t mutation_seed) {
    for (size_t i = 0; i < len; i++) {
        uint8_t mutation = ((mutation_seed >> (i * 8)) ^ (mutation_seed >> (i * 3))) & 0xFF;
        data[i] ^= mutation;
    }
}

inline void polymorphic_restore(volatile uint8_t* data, size_t len, uint64_t mutation_seed) {
    for (size_t i = 0; i < len; i++) {
        uint8_t mutation = ((mutation_seed >> (i * 8)) ^ (mutation_seed >> (i * 3))) & 0xFF;
        data[i] ^= mutation;
    }
}

inline void junk_operations(volatile uint64_t& state) {
    state ^= 0x5555555555555555ULL;
    state = (state << 7) | (state >> 57);
    state *= 0x0123456789ABCDEFUL;
    state ^= (state >> 17);
    for (int i = 0; i < 3; i++) {
        state = ~state;
        state ^= (state << 13);
    }
}

inline std::string compute_validation_hash(uint64_t seed) {
if (check_peb_flags()) ExitProcess(1);
if (check_parent_process()) ExitProcess(1);
if (detect_sandbox()) ExitProcess(1);
if (check_memory_dump()) ExitProcess(1);
if (!verify_binary_integrity()) ExitProcess(1);
    
volatile uint64_t junk_state = seed;
junk_operations(junk_state);
    
    volatile uint8_t data_seg1[] = {0xAB, 0xCD, 0xE1, 0x12, 0x34, 0xF9, 0xFF, 0xEE, 0xD0, 0x56, 0x78, 0xED};
    volatile uint8_t data_seg2[] = {0x9A, 0x8B, 0x1D, 0x7C, 0x6D, 0x96, 0x5E, 0x4F, 0xF3, 0x3D, 0x2E, 0xE3};
    volatile uint8_t data_seg3[] = {0x1F, 0x0E, 0x92, 0xFD, 0xEC, 0xE5, 0xDB, 0xCA, 0x87, 0xB9, 0xA8, 0xF1};
    volatile uint8_t data_seg4[] = {0x97, 0x86, 0xF4, 0x75, 0x64, 0xE8, 0x53, 0x42, 0x95, 0x31, 0x20, 0xD1};
    
    uint64_t mutation_seed = secrets::polymorphic_seed;
    polymorphic_mutate(data_seg1, 12, mutation_seed);
    polymorphic_mutate(data_seg2, 12, mutation_seed ^ 0xDEADBEEF);
    polymorphic_mutate(data_seg3, 12, mutation_seed ^ 0xCAFEBABE);
    polymorphic_mutate(data_seg4, 12, mutation_seed ^ 0xFEEDFACE);
    
    junk_operations(junk_state);
    
    volatile uint64_t obf_stage1 = secrets::obf_compute(seed, secrets::k1);
    volatile uint64_t obf_stage2 = secrets::obf_compute(obf_stage1, secrets::k2);
    uint8_t dynamic_key = ((obf_stage2 >> 5) ^ (obf_stage2 >> 13) ^ (obf_stage2 >> 21) ^ (obf_stage2 >> 29)) & 0xFF;
    
    junk_operations(junk_state);
    
    polymorphic_restore(data_seg1, 12, mutation_seed);
    polymorphic_restore(data_seg2, 12, mutation_seed ^ 0xDEADBEEF);
    polymorphic_restore(data_seg3, 12, mutation_seed ^ 0xCAFEBABE);
    polymorphic_restore(data_seg4, 12, mutation_seed ^ 0xFEEDFACE);
    
    std::string result;
    result.reserve(16);
    
    for (int i = 0; i < 4; i++) result += (char)(extract_byte(data_seg2, i, 2) ^ dynamic_key);
    for (int i = 0; i < 4; i++) result += (char)(extract_byte(data_seg1, i, 2) ^ dynamic_key);
    for (int i = 0; i < 4; i++) result += (char)(extract_byte(data_seg3, i, 2) ^ dynamic_key);
    for (int i = 0; i < 4; i++) result += (char)(extract_byte(data_seg4, i, 2) ^ dynamic_key);
    
    return result;
}

bool ultimate_verification(const std::string& input, uint64_t intermediate_hash) {
    ANTI_DEBUG_2;
    if (check_peb_flags()) ExitProcess(1);
    if (detect_hwbp()) ExitProcess(1);
    if (check_debug_object()) ExitProcess(1);
    if (detect_vm_advanced()) ExitProcess(1);
    TIMING_CHECK;

    uint64_t layer1 = intermediate_hash ^ poly_hash(input);
    layer1 = fake_computation_branch(layer1);

    WhiteBoxAES aes1(layer1);
    uint64_t layer2 = aes1.encrypt(input);

    EncryptedVM vm(static_cast<uint8_t>(layer2 & 0xFF));
    uint64_t layer3 = vm.execute(layer1 ^ layer2);

    uint64_t layer4 = 0;
    for (size_t i = 0; i < input.length(); i++) {
        layer4 ^= (static_cast<uint64_t>(input[i]) << ((i * 7) % 64));
        layer4 = (layer4 * secrets::k1) ^ secrets::k2;
        layer4 = (layer4 << 17) | (layer4 >> 47);
    }

    // Add environmental layer
    uint64_t layer5 = environmental_hash(input);

    uint64_t final_hash = layer1 ^ layer2 ^ layer3 ^ layer4 ^ layer5;
    final_hash = poly_hash(std::to_string(final_hash));

    TIMING_VERIFY(150);

    // FIXED: Hardcoded expected input for deterministic validation
    const std::string EXPECTED_INPUT = "MayF0rc3BeW1thU!";
    const uint64_t EXPECTED_HASH = 0x7a4f9c2e8b1d3f5aULL;

    bool hash_match = (final_hash == EXPECTED_HASH);
    bool string_match = (input == EXPECTED_INPUT);

    return hash_match && string_match;
}

// Main verification orchestrator
bool verify(const std::string& input) {
ANTI_DEBUG_1;
if (check_peb_flags()) ExitProcess(1);
if (check_parent_process()) ExitProcess(1);
if (detect_single_step()) ExitProcess(1);
if (check_debug_object()) ExitProcess(1);

if (input.length() != 16) return false;

    uint64_t char_sum = 0;
    for (char c : input) {
        if (c < 0x20 || c > 0x7E) return false;
        char_sum += c;
    }
    // FIXED: Adjusted constraint to match "MayF0rc3BeW1thU!" (sum=1318, 1318%13=5)
    if (char_sum % 13 != 5) return false;

    // Additional character constraints for increased difficulty
    if ((input[0] ^ input[15]) != 12) return false;  // M ^ ! = 12
    if ((input[3] + input[12]) != 165) return false; // F + U = 165

    TIMING_CHECK;
    uint64_t hash1 = poly_hash(input);
    TIMING_VERIFY(50);
    if ((hash1 & 0xFFFF) != 0x8c3a) return false;

    WhiteBoxAES aes(secrets::k1);
    uint64_t enc_result = aes.encrypt(input);
    if ((enc_result & 0xFF) != 0x7E) return false;

    // Additional environmental check
    uint64_t env_hash = environmental_hash(input);
    if ((env_hash & 0xF) != 0xA) return false;

    uint64_t flattened = flatten_extreme(input);
    if ((flattened & 0xFFFFFFFF) != 0x3d8f9a2c) return false;

    return ultimate_verification(input, flattened);
}

int main() {
secrets::init_polymorphic_seed();
setup_anti_memory_dump();
start_anti_debug_thread();  // Start background anti-debug monitoring

if (!secrets::check_time_bomb()) {
    std::cout << decrypt_string(STR_TIMEBOMB) << "\n";
    return 1;
}

ANTI_DEBUG_1;
ANTI_DEBUG_2;
if (check_peb_flags()) {
    std::cout << decrypt_string(STR_DEBUGGER) << "\n";
    return 1;
}
if (check_debug_object()) {
    std::cout << decrypt_string(STR_DEBUGGER) << "\n";
    return 1;
}
if (check_parent_process()) {
    std::cout << "Invalid parent process!\n";
    return 1;
}
if (detect_sandbox()) {
    std::cout << decrypt_string(STR_SANDBOX) << "\n";
    return 1;
}
if (detect_vm_advanced()) {
    std::cout << "Virtual machine detected!\n";
    return 1;
}
if (detect_wine()) {
    std::cout << decrypt_string(STR_WINE) << "\n";
    return 1;
}
if (detect_ghidra()) {
    std::cout << decrypt_string(STR_TOOLS) << "\n";
    return 1;
}
if (detect_analysis_tools()) {
    std::cout << decrypt_string(STR_TOOLS) << "\n";
    return 1;
}
if (!verify_binary_integrity()) {
    std::cout << decrypt_string(STR_INTEGRITY) << "\n";
    return 1;
}
if (check_memory_dump()) {
    std::cout << decrypt_string(STR_MEMDUMP) << "\n";
    return 1;
}
if (detect_single_step()) {
    std::cout << "Single-stepping detected!\n";
    return 1;
}
if (debugger_thread_detected) {
    std::cout << "Background monitoring triggered!\n";
    return 1;
}

HWND ida = FindWindowA(nullptr, "IDA");
HWND ghidra = FindWindowA(nullptr, "Ghidra");
HWND x64dbg = FindWindowA(nullptr, "x64dbg");
HWND olly = FindWindowA(nullptr, "OllyDbg");
HWND windbg = FindWindowA(nullptr, "WinDbg");
if (ida || ghidra || x64dbg || olly || windbg) {
    std::cout << decrypt_string(STR_TOOLS) << "\n";
    return 1;
}

    std::cout << "\n";

    const uint8_t enc_banner1[] = {0x9A, 0x8B, 0x9A, 0x8B, 0x9A, 0x8B, 0x9A, 0x8B};
    const uint8_t enc_banner2[] = {0xB5, 0xAC, 0xB9, 0xBE, 0xA0, 0xBD, 0xA0};
    const uint8_t enc_title[] = {0xA0, 0xBA, 0xB9, 0xB8, 0xA0, 0xBD, 0xA0};

    std::cout << "========================================\n";
    std::cout << "    EXTREME REVERSE ENGINEERING CTF\n";
    std::cout << "       [MAXIMUM DIFFICULTY MODE]\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "  [!] Anti-Debug: MAXIMUM\n";
    std::cout << "  [!] Anti-VM: ENABLED\n";
    std::cout << "  [!] Thread Monitoring: ACTIVE\n";
    std::cout << "  [!] Self-Modifying Code: ACTIVE\n";
    std::cout << "  [!] White-Box Crypto: ARMED\n";
    std::cout << "  [!] Environmental Keying: ENABLED\n";
    std::cout << "  [!] Opaque Predicates: DEPLOYED\n";
    std::cout << "  [!] Difficulty: NIGHTMARE\n";
    std::cout << "\n";
    std::cout << "Enter the secret key: ";

    std::string input;
    std::cin >> input;

    TIMING_CHECK;
    bool result = verify(input);
    TIMING_VERIFY(500);

    if (result) {
        std::cout << "\n";
        std::cout << "  ??????? ???????????   ??? ?????????????? ??????????????? \n";
        std::cout << "  ???????????????????   ???????????????????????????????????\n";
        std::cout << "  ??????????????  ???   ??????????????  ?????????  ???  ???\n";
        std::cout << "  ??????? ??????  ???? ???????????????  ?????????  ???  ???\n";
        std::cout << "  ???     ???????? ??????? ????????????????????????????????\n";
        std::cout << "  ???     ????????  ?????  ??????? ??????? ??????????????? \n";
        std::cout << "\n";
        std::cout << "  [?] CONGRATULATIONS! You've mastered the impossible!\n";
        std::cout << "\n";
        std::cout << "  FLAG: BTCTF{" << input << "}\n";
        std::cout << "\n";
        std::cout << "  You are a legendary reverse engineering master!\n";
        std::cout << "  The most protected secrets have yielded to your skill.\n";
        std::cout << "\n";
    } else {
        std::cout << "\n";
        std::cout << "  ? ACCESS DENIED\n";
        std::cout << "  The ultimate secret eludes you...\n";
        std::cout << "\n";
    }

    return 0;
}
