// AlignRight.cpp
// LCD Smartie plugin: right-aligns a text string to a given column position.
//
// Build with MSVC:
//   cl /LD /EHsc /O2 AlignRight.cpp /Fe:AlignRight.dll
//
// Usage in LCD Smartie:
//   $dll(AlignRight,1,<string>,<position>)
//
//   string   : the text to align (may contain any characters)
//   position : the column at which the string's LAST character should sit
//              (1-based; typically the width of your LCD row, e.g. 20)
//
// Examples (position = 20):
//   $dll(AlignRight,1,CPU: 72%,20)   ->  "            CPU: 72%"
//   $dll(AlignRight,1,CPU: 100%,20)  ->  "           CPU: 100%"
//
// In both cases the final character sits in column 20, so a value that
// grows or shrinks by a digit does not shift the rest of the line around.
//
// Edge cases:
//   - If `position` is <= 0, the string is returned unchanged.
//   - If `position` is omitted, it defaults to 20.
//   - If the string is longer than `position`, only its rightmost
//     `position` characters are shown, so the last character stays
//     pinned at the requested column.
//   - An empty or missing first parameter returns an empty string.
//
// The plugin is stateless. The result buffer is thread_local, so
// concurrent calls from different threads cannot overwrite each other.

#include <windows.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static const int MAX_OUTPUT   = 512;
static const int DEFAULT_POS  = 20;

// ---------------------------------------------------------------------------
// Thread-local result buffer. Because the function is pure (no shared
// state), each thread can safely keep its own copy without any locking.
// The buffer lives until the next call on the same thread, which is all
// LCD Smartie needs.
// ---------------------------------------------------------------------------
static thread_local char resultBuffer[MAX_OUTPUT];

// ---------------------------------------------------------------------------
// Plugin entry point
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) char* __stdcall function1(char* param1, char* param2)
{
    resultBuffer[0] = '\0';

    // Nothing to align.
    if (param1 == NULL || param1[0] == '\0') {
        return resultBuffer;
    }

    // Parse the target column.
    int position = DEFAULT_POS;
    if (param2 != NULL && param2[0] != '\0') {
        position = atoi(param2);
    }

    // Non-positive position: return the string unchanged.
    if (position <= 0) {
        strncpy(resultBuffer, param1, MAX_OUTPUT - 1);
        resultBuffer[MAX_OUTPUT - 1] = '\0';
        return resultBuffer;
    }

    // Clamp the position to something the buffer can actually hold.
    if (position >= MAX_OUTPUT) {
        position = MAX_OUTPUT - 1;
    }

    int         textLen = (int)strlen(param1);
    const char* text    = param1;

    // If the text is longer than the requested position, keep only the
    // rightmost `position` characters so the last character still lands
    // on the requested column.
    if (textLen > position) {
        text    = param1 + (textLen - position);
        textLen = position;
    }

    // Number of leading spaces needed to push the last character to
    // the requested column.
    int padding = position - textLen;
    if (padding < 0) {
        padding = 0;
    }

    // Assemble the output.
    if (padding > 0) {
        memset(resultBuffer, ' ', padding);
    }
    memcpy(resultBuffer + padding, text, textLen);
    resultBuffer[padding + textLen] = '\0';

    return resultBuffer;
}

// ---------------------------------------------------------------------------
// Lifecycle. The plugin has no persistent state, so there is nothing to
// do in these hooks, but LCD Smartie looks for them and will complain in
// its log if they are missing.
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) void __stdcall SmartieInit() {}
extern "C" __declspec(dllexport) void __stdcall SmartieFini() {}
