# lcd-smartie-alignright

# Description:
Ensures that the last character of a text string stays in place even if the string's character count changes. If the character count of a text string to its left [stringLeft] changes, the gap between them lengthens or shortens accordingly so the right-aligned text [stringRight] doesn't move out of place.

# Formatting:
$dll(AlignRight,1,[string],[right-most position])  
**Or**  
$dll(AlignRight,1,[stringLeft]~[stringRight],[right-most position])

# Caveat:
- [string], [stringLeft], and [stringRight] will not display $Char(0) or anything following it. Other custom characters display normally.
- If $Char(0) must be used, the workaround would involve placing it outside of the function entirely.
