# NppVim – Vim Mode for Notepad++

> Donate : (paypal)[https://paypal.me/h8imansh8u]

NppVim brings **Vim-like modal editing** to Notepad++.

## Features
 - Movement and navigation ```h, j, k, l, w, b, e, 0, $, ^, gg, G, {, }, H, L```
 - Insert & edit ```i,I,a,A,o,O```
 - Delete, copy, & paste ```d,y,p,x,X```
 - Delete/change to endline & join ```D, C, J```
 - Visual mode ```v,V```
 - Live Search ```/, n, N, *, #```
 - Replace & overwrite mode ```r, R```
 - Operations like join, undo, repeat ```J ,u , .```
 - Repeat counts ```3w, d2d, 5p```
 - Text objects editing ```iw, aw, iW, aW, i", a", i', a', i`, a`, i(, a(, i[, a[, i{, a{, i<, a<, is, as, ip, ap, it, at```
 - Character search ```f F ; , t T```
 - Command mode for file operations ```:w,:q,:wq,:number```
 - Real-time search match counting
 - Regex search capabilities `:s` or `?`
 - Marks (local and global) ```:marks,:delm!'```

## Text Objects

Text objects allow you to operate on structured text. All text objects work in both **Normal mode** (with operators like `d`, `c`, `y`) and **Visual mode** (with `v`).

### Word Text Objects
- `iw` / `aw` - inner word / a word (with surrounding space)
- `iW` / `aW` - inner big word / a big word (ignores punctuation, treats `hello-world` as one word)

### Quote Text Objects
- `i"` / `a"` - inside/around double quotes
- `i'` / `a'` - inside/around single quotes
- ``i` `` / ``a` `` - inside/around backticks

### Bracket Text Objects
- `i(` / `a(` - inside/around parentheses
- `i[` / `a[` - inside/around square brackets
- `i{` / `a{` - inside/around curly braces
- `i<` / `a<` - inside/around angle brackets

### Sentence and Paragraph
- `is` / `as` - inner sentence / a sentence
- `ip` / `ap` - inner paragraph / a paragraph

### Tags
- `it` / `at` - inside/around HTML/XML tags
