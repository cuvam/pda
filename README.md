```
/*
    Supported syntax:
        .: wildcard (any single character)
        ?: 0 or 1 of the previous character
        *: 0 or more of the previous character
        +: 1 or more of the previous character
        () Groups
        \: Escape next character (treat as literal)
*/
```

A grep-like tool which takes in a basic RegEx pattern and a filename and displays all matches to that pattern inside the file.

About 500 LOC, works in most cases and performs within about 2x grep's runtime.
