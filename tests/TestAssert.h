#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#if defined (_MSC_VER)
    #define NOMINMAX
    #include <Windows.h>
    #include <crtdbg.h>
    #include <cstdlib>
#endif

inline void sampleBenchTestLog (const std::string& message)
{
    std::ofstream log { "sample-bench-test-progress.txt", std::ios::app };
    log << message << '\n';
    log.flush();
    std::cerr << message << std::endl;
}

inline void configureSampleBenchTestProcess()
{
#if defined (_MSC_VER)
    SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_error_mode (_OUT_TO_STDERR);
    _CrtSetReportMode (_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile (_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode (_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile (_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    sampleBenchTestLog ("test process started");
}

inline void sampleBenchTestAssert (bool condition,
                                   const char* expression,
                                   const char* file,
                                   int line)
{
    if (condition)
        return;

    sampleBenchTestLog (std::string { file } + ":" + std::to_string (line) + ": assertion failed: " + expression);
    std::exit (1);
}

#undef assert
#define assert(expression) sampleBenchTestAssert (static_cast<bool> (expression), #expression, __FILE__, __LINE__)
