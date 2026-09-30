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
// CUSTOM CHARACTERS
//   The plugin measures strings by DISPLAY COLUMNS, not bytes. A
//   "$Chr(N)" sequence that LCD Smartie has not yet expanded is
//   treated as one column, since it renders as one custom character.
//   Any other byte counts as one column. This keeps alignment correct
//   whether LCD Smartie expands $Chr(...) before or after the plugin.
//
//   LIMITATION: $Chr(0) produces a NUL byte when expanded. That
//   terminates the C string, so anything after a $Chr(0) is lost
//   before the plugin even sees it. This is a limitation of LCD
//   Smartie's plugin API, not the plugin. If you need custom char 0
//   in a string, use the byte value your display driver maps it to
//   (for the default HD44780 driver this is $Chr(176)).
//
// Edge cases:
//   - If `position` is <= 0, param1 is returned unchanged.
//   - If `position` is omitted, it defaults to 20.
//   - If `right` alone is wider than `position`, only its rightmost
//     `position` columns are shown.
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
// Column counter.
//
// Walks up to `maxBytes` bytes of `s`, counting:
//   - a literal "$Chr(...)" sequence as 1 column
//   - every other byte as 1 column
//
// This handles the common case where LCD Smartie has not yet expanded
// $Chr(N) into a raw byte before calling the plugin. If the sequence
// was already expanded, each raw byte is counted individually, which
// is the same thing (1 byte = 1 column).
//
// Stops at a NUL byte or after `maxBytes`.
// ---------------------------------------------------------------------------
static int visual_width(const char* s, int maxBytes)
{
    int width = 0;
    int i     = 0;

    while (i < maxBytes && s[i] != '\0') {
        // Detect "$Chr(" without reading past maxBytes.
        if (s[i] == '$' && i + 5 <= maxBytes &&
            s[i+1] == 'C' && s[i+2] == 'h' &&
            s[i+3] == 'r' && s[i+4] == '(')
        {
            // Find the matching ')' within bounds.
            int j = i + 5;
            while (j < maxBytes && s[j] != '\0' && s[j] != ')') {
                ++j;
            }
            if (j < maxBytes && s[j] == ')') {
                width += 1;          // entire $Chr(...) = 1 column
                i      = j + 1;
                continue;
            }
            // Unterminated $Chr( — fall through and count bytes.
        }
        width += 1;
        i     += 1;
    }

    return width;
}

// ---------------------------------------------------------------------------
// Helper: byte-length of a C string, capped at a maximum.
// Equivalent to strnlen, which MSVC does provide, but spelling it out
// keeps the code portable and self-documenting.
// ---------------------------------------------------------------------------
static int bounded_strlen(const char* s, int maxBytes)
{
    int i = 0;
    while (i < maxBytes && s[i] != '\0') {
        ++i;
    }
    return i;
}

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
        int textBytes = bounded_strlen(param1, MAX_OUTPUT - 1);
        int textWidth = visual_width(param1, textBytes);

        int padding = position - textWidth;
        if (padding < 0) padding = 0;                        // overflow right
        if (padding > MAX_OUTPUT - 1) padding = MAX_OUTPUT - 1;

        // Trim the text if padding + text would overflow the buffer.
        if (textBytes > MAX_OUTPUT - 1 - padding) {
            textBytes = MAX_OUTPUT - 1 - padding;
        }

        if (padding > 0) {
            memset(resultBuffer, ' ', padding);
        }
        memcpy(resultBuffer + padding, param1, textBytes);
        resultBuffer[padding + textBytes] = '\0';
        return resultBuffer;
    }

    // =====================================================================
    // MODE 2 — split on the delimiter and pin the right side.
    // =====================================================================
    int leftBytes = (int)(delim - param1);
    int leftWidth = visual_width(param1, leftBytes);

    const char* rightPtr = delim + 1;
    int rightBytes = bounded_strlen(rightPtr, MAX_OUTPUT - 1);
    int rightWidth = visual_width(rightPtr, rightBytes);

    // Space available for the left text (in display columns).
    int spaceForLeft = position - rightWidth;
    if (spaceForLeft < 0) spaceForLeft = 0;

    int pad = spaceForLeft - leftWidth;
    if (pad < 0) pad = 0;

    // Safety: the assembled string must fit in the result buffer.
    // If it would not, return param1 unchanged rather than truncating.
    if (leftBytes + pad + rightBytes >= MAX_OUTPUT) {
        strncpy(resultBuffer, param1, MAX_OUTPUT - 1);
        resultBuffer[MAX_OUTPUT - 1] = '\0';
        return resultBuffer;
    }

    // --- Assemble: left bytes, spaces, right bytes -----------------------
    int idx = 0;

    if (leftBytes > 0) {
        memcpy(resultBuffer + idx, param1, leftBytes);
        idx += leftBytes;
    }
    if (pad > 0) {
        memset(resultBuffer + idx, ' ', pad);
        idx += pad;
    }
    if (rightBytes > 0) {
        memcpy(resultBuffer + idx, rightPtr, rightBytes);
        idx += rightBytes;
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
