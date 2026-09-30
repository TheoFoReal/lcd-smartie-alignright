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
// In both cases the final character sits in column 20 of the plugin's
// own output, so a value that grows or shrinks by a digit does not shift
// the string around.
//
// NOTE: the position refers to the plugin's output, not to the whole
// screen field. If the field already contains literal text before the
// $dll(...) call, that text occupies columns to the left of the plugin's
// output, so the plugin's last character will not land on the same
// column of the physical display that you passed as `position`.
//
// For example:
//   CPU: $dll(AlignRight,1,$SysCPUUsage%,17)%
//
// produces 5 literal columns ("CPU: "), then the plugin's 17-column
// output, then a literal "%". The plugin's last character sits at
// column 22 of the field, and the "%" sits at column 23. If you want
// the "%" to land on column 20, pass position=19.
//
// Edge cases:
//   - If `position` is <= 0, the string is returned unchanged.
//   - If `position` is omitted, it defaults to 20.
//   - If the string is longer than `position`, it is returned as-is,
//     overflowing to the right past the requested column. This means
//     the "last character pinned to `position`" promise only holds when
//     the string fits within `position`.
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
// Thread-local result buffer. The function is pure, so each thread can
// safely keep its own copy without any locking.
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

    // Clamp the position to something the buffer can hold.
    if (position >= MAX_OUTPUT) {
        position = MAX_OUTPUT - 1;
    }

    int textLen = (int)strlen(param1);

    // If the source itself would overflow the result buffer, cap it so
    // the memcpy below cannot write past the end.
    if (textLen > MAX_OUTPUT - 1) {
        textLen = MAX_OUTPUT - 1;
    }

    // Leading padding needed to land the last character on `position`.
    // If the text is longer than `position`, padding is 0 and the text
    // simply overflows to the right past `position`.
    int padding = position - textLen;
    if (padding < 0) {
        padding = 0;
    }

    // Assemble the output.
    if (padding > 0) {
        memset(resultBuffer, ' ', padding);
    }
    memcpy(resultBuffer + padding, param1, textLen);
    resultBuffer[padding + textLen] = '\0';

    return resultBuffer;
}

// ---------------------------------------------------------------------------
// Lifecycle. The plugin has no persistent state, so these hooks are empty,
// but LCD Smartie looks for them and will log a warning if they are missing.
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) void __stdcall SmartieInit() {}
extern "C" __declspec(dllexport) void __stdcall SmartieFini() {}
