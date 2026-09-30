// AlignRight.cpp
// LCD Smartie plugin: right-aligns a text string to a given column position.
//
// Build with MSVC:
//   cl /LD /EHsc /O2 AlignRight.cpp /Fe:AlignRight.dll
//
// Two modes, chosen automatically by the presence of a '~' in the
// first parameter:
//
// MODE 1 — single string (no '~' in param1)
//   $dll(AlignRight,1,<string>,<position>)
//
//   Pads <string> with leading spaces so its last character sits at
//   column <position>. If <string> is longer than <position>, it is
//   returned unchanged (overflow to the right).
//
//   Example (position = 20):
//     $dll(AlignRight,1,72%,20)     ->  "                 72%"
//     $dll(AlignRight,1,100%,20)    ->  "                100%"
//
// MODE 2 — label plus value, pinned right edge (param1 contains '~')
//   $dll(AlignRight,1,<left>~<right>,<position>)
//
//   The plugin outputs <left>, then spaces, then <right>, so that the
//   whole thing is exactly <position> characters wide and <right>'s
//   last character always lands on column <position> — no matter how
//   <left> changes in length from one refresh to the next.
//
//   Example (position = 20):
//     $dll(AlignRight,1,CPU: ~72%,20)
//       ->  "CPU:            72%"
//     $dll(AlignRight,1,CPU: ~100%,20)
//       ->  "CPU:           100%"
//
//   In both cases the '%' is in column 20, and "CPU: " is at the
//   left. If <left> grows, the gap in the middle shrinks; if it grows
//   past the space available, it is truncated from its right end so
//   the value still fits.
//
//   IMPORTANT: because the plugin emits the entire line, the field
//   containing the $dll(...) call must contain nothing else, and the
//   screen's Centre0N and NoScroll0N settings should be 0 and 1
//   respectively, so LCD Smartie passes the output through verbatim.
//
//   A literal '~' cannot appear in <left>. It is treated as the
//   delimiter.
//
// Edge cases:
//   - If `position` is <= 0, param1 is returned unchanged.
//   - If `position` is omitted, it defaults to 20.
//   - If `right` alone is longer than `position`, only its rightmost
//     `position` characters are shown, so its last character still
//     lands on the requested column.
//   - An empty first parameter returns an empty string.
//
// The plugin is stateless. The result buffer is thread_local, so
// concurrent calls from different threads cannot overwrite each other.

#include <windows.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
static const int  MAX_OUTPUT   = 512;
static const int  DEFAULT_POS  = 20;
static const char DELIMITER    = '~';

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

    if (param1 == NULL || param1[0] == '\0') {
        return resultBuffer;
    }

    // --- Parse the target column ----------------------------------------
    int position = DEFAULT_POS;
    if (param2 != NULL && param2[0] != '\0') {
        position = atoi(param2);
    }

    if (position <= 0) {
        strncpy(resultBuffer, param1, MAX_OUTPUT - 1);
        resultBuffer[MAX_OUTPUT - 1] = '\0';
        return resultBuffer;
    }
    if (position >= MAX_OUTPUT) {
        position = MAX_OUTPUT - 1;
    }

    const char* delim = strchr(param1, DELIMITER);

    // =====================================================================
    // MODE 1 — no delimiter, right-align the whole string to `position`.
    // =====================================================================
    if (delim == NULL) {
        int textLen = (int)strlen(param1);
        if (textLen > MAX_OUTPUT - 1) textLen = MAX_OUTPUT - 1;

        int padding = position - textLen;
        if (padding < 0) padding = 0;    // overflow to the right

        if (padding > 0) {
            memset(resultBuffer, ' ', padding);
        }
        memcpy(resultBuffer + padding, param1, textLen);
        resultBuffer[padding + textLen] = '\0';
        return resultBuffer;
    }

    // =====================================================================
    // MODE 2 — split on the delimiter and pin the right side.
    // =====================================================================
    const char* leftPtr  = param1;
    int         leftLen  = (int)(delim - param1);

    const char* rightPtr = delim + 1;
    int         rightLen = (int)strlen(rightPtr);

    // Safety caps for the buffer.
    if (leftLen  > MAX_OUTPUT - 1) leftLen  = MAX_OUTPUT - 1;
    if (rightLen > MAX_OUTPUT - 1) rightLen = MAX_OUTPUT - 1;

    // If the right text alone does not fit, keep only its rightmost
    // characters, so its last character still lands on `position`.
    if (rightLen > position) {
        rightPtr += (rightLen - position);
        rightLen  = position;
    }

    // Space available for the left text.
    int spaceForLeft = position - rightLen;

    // If the left text is too long, keep its beginning (the start of
    // the label) and drop the rest. This is the friendlier failure
    // mode for a label followed by a value.
    int useLeftLen = leftLen;
    if (useLeftLen > spaceForLeft) useLeftLen = spaceForLeft;

    int pad = spaceForLeft - useLeftLen;
    if (pad < 0) pad = 0;

    // --- Assemble: left, spaces, right ----------------------------------
    int idx = 0;

    if (useLeftLen > 0) {
        memcpy(resultBuffer + idx, leftPtr, useLeftLen);
        idx += useLeftLen;
    }
    if (pad > 0) {
        memset(resultBuffer + idx, ' ', pad);
        idx += pad;
    }
    if (rightLen > 0) {
        memcpy(resultBuffer + idx, rightPtr, rightLen);
        idx += rightLen;
    }
    resultBuffer[idx] = '\0';

    return resultBuffer;
}

// ---------------------------------------------------------------------------
// Lifecycle. The plugin has no persistent state, so these hooks are empty,
// but LCD Smartie looks for them and will log a warning if they are missing.
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) void __stdcall SmartieInit() {}
extern "C" __declspec(dllexport) void __stdcall SmartieFini() {}
