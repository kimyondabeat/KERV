#pragma once
// Diagnostic log for the AAX test build (cmake -DKERV_DEBUG_LOG=ON).
// Writes to %TEMP%\KERV_log.txt on Windows. Does nothing in normal builds.

#if KERV_DEBUG_LOG && defined (_WIN32)
 #include <cstdio>
 #include <cstdlib>
 #include <ctime>
 #include <mutex>
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>

namespace kervdbg
{
    inline std::mutex& logMutex() { static std::mutex m; return m; }

    inline void log (const char* msg)
    {
        std::lock_guard<std::mutex> lock (logMutex());
        char path[MAX_PATH] {};
        const char* tmp = std::getenv ("TEMP");
        std::snprintf (path, sizeof (path), "%s\\KERV_log.txt", tmp != nullptr ? tmp : "C:\\Users\\Public");
        if (FILE* f = std::fopen (path, "a"))
        {
            std::fprintf (f, "[%lu ms] [thread %lu] %s\n", (unsigned long) GetTickCount(),
                          (unsigned long) GetCurrentThreadId(), msg);
            std::fclose (f);
        }
    }

    inline LONG CALLBACK onException (PEXCEPTION_POINTERS info)
    {
        static int count = 0;
        if (++count > 40)
            return EXCEPTION_CONTINUE_SEARCH;

        HMODULE mod = nullptr;
        GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR) info->ExceptionRecord->ExceptionAddress, &mod);
        HMODULE self = nullptr;
        GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR) &onException, &self);
        char modName[MAX_PATH] = "?";
        if (mod != nullptr) GetModuleFileNameA (mod, modName, MAX_PATH);

        char line[1024];
        std::snprintf (line, sizeof (line), "EXCEPTION code=0x%08lX at %p (module %s, offset 0x%llX)%s",
                       (unsigned long) info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress,
                       modName,
                       (unsigned long long) ((const char*) info->ExceptionRecord->ExceptionAddress - (const char*) mod),
                       mod == self ? "  <-- inside KERV" : "");
        log (line);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    struct Startup
    {
        Startup()
        {
            HMODULE self = nullptr;
            GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR) &onException, &self);
            char line[512];
            std::snprintf (line, sizeof (line), "---- KERV loaded (static init), module base %p ----", (void*) self);
            log (line);
            AddVectoredExceptionHandler (1, onException);
        }
    };
}

 #define KERV_LOG(msg) kervdbg::log (msg)
#else
 #define KERV_LOG(msg) ((void) 0)
#endif
