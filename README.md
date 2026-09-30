# lcd-smartie-alignright

# Description:
Ensures that the last character of a text string stays in place even if the string's character count changes. If the character count of a text string to its left changes, the gap between them lengthens or shortens accordingly so the right-aligned text string doesn't move.

# Formatting:
$dll(AlignRight,1,[string],[right-most position]) **OR** $dll(AlignRight,1,[stringLeft]~[stringRight],[right-most position])

# Caveat:
[string] will not display $Char(0) or anything following it. Other custom characters display normally.
