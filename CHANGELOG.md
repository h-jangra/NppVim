# 1.14.0.0

## Added

### Relative Line Numbers
- **Themed Integration**: Numbers now automatically match active Notepad++ colors and fonts.
- **Hybrid Mode**: Show absolute line number for the current line and relative for others (enable both `nu` and `rnu`).
- **Dynamic Width**: Gutter width now adjusts automatically based on document size.
- **Performance**: Optimized updates to eliminate flickering during cursor movement.
- **Instant Load**: Fixed numbering not showing immediately on file open or buffer switch.

### Configuration (.rc and .ini)
- **Startup Script**: `nppvim.rc` now supports executing any command-mode command (e.g., `:w`, `:split`, `:command`).
- **Improved Parsing**: Correctly handles `no` prefix for aliases (e.g., `set nonu`, `set nornu`).
- **Keyboard Layouts**: Support for `set normallayout` and `set insertlayout` directly in `.rc`.
- **Persistent States**: Plugin now correctly resets and reloads all options from scratch when reloading configuration.

## Fixed
- Fixed flickering when moving cursor with relative numbers enabled.
- Fixed configuration reload not disabling features removed from `nppvim.rc`.
- Fixed build failures in `CommandMode.cpp` related to member access and name ambiguity.

# 1.12.0.0

## Fixed

### Visual Line Mode
- `j` / `k` – Fixed selection not extending correctly
- View now scrolls to follow cursor during visual line selection
- `f/`t` - Fixed forward and till motions in visual mode.

### Visual Mode
- `o` – Fixed character visual mode losing selection on first switch

## Added

### Visual Mode
- `zz` / `zt` / `zb` – Center / top / bottom cursor in view
- `Ctrl+D` / `Ctrl+U` – Half page down / up (extends selection)
- `Ctrl+F` / `Ctrl+B` – Page forward / backward
- `Ctrl+E` / `Ctrl+Y` – Scroll window down / up
- `Ctrl+R` – Redo

# 1.11.0.0
- Fix h-jangra#40: escape (jj,jk) deletes extra character
- Add custom keymap for esc in config #41

---

# 1.10.0.0

## Added
`registers`/`reg`/`di`/`display` - vim like registers
`d`/`c` - in visual line `cut` text instead `clear`

---

# 1.9.0.0
## Added

`-` - goto line start
`za` - toggle fold
`q`/`@` - macros

---

# 1.8.0.0
## Fixed

* `Visual mode stability` – Corrected selection behavior and motion consistency
* `iW / aW` – Improved big-word text object handling

## Added

### Visual Mode
- `gq` / `gw` – Wrap selected text
- `U` / `u` – Convert selection to uppercase / lowercase
- `J` – Join selected lines
- `r` – Replace selected characters
- `S` – Substitute selection with input text

### Normal Mode
- `(` / `)` – Move between sentences
- `M` – Jump to middle of the screen
- `ge` – Move to end of previous word
- `gE` – Move to end of previous WORD
- `gf` – Open file under cursor
- `gx` – Open URL under cursor
- `J` – Join lines without extra space
- `zt` / `zb` – Scroll line to top / bottom of window
- `zc` / `zo` – Close / open fold
- `gi` – Jump to last insert position
- `[` / `]` – Structured navigation (blocks / sections)
- `gU` / `gu` – Uppercase / lowercase motion
- `gUU` / `guu` – Uppercase / lowercase entire line
- `gd` – Jump to first occurrence of symbol
- `Ctrl+E` / `Ctrl+Y` – Scroll window down / up
- `Ctrl+F` / `Ctrl+B` – Page forward / backward
- `gJ` – Join lines without inserting space
- `S` – Substitute entire line
- `|` – Jump to specific column
- `gI` – Insert at first non-blank character

### Command Mode
- `sort` / `sort!` – Sort lines (normal / reverse)
- `sort n` / `sort n!` – Numeric sort (normal / reverse)
- `noh` / `nohl` / `nohlsearch` – Clear search highlights
- `set nu` / `set number` – Enable line numbers
- `set nonu` / `set nonumber` – Disable line numbers
- `set tw=` – Set text width
- `wa` / `wall` – Write all buffers
- `qa` / `qall` – Quit all buffers
- `wqa` / `xa` / `xall` – Save and quit all
- `e` / `edit` Refresh file
- `bp` / `bprev` / `bprevious` – Go to previous buffer
- `bd` / `bdelete` – Delete buffer
- `sp` / `split` – Horizontal split
- `vs` / `vsplit` – Vertical split
- `tabn` / `tabp` – Next / previous tab
- `about` - Show about dialog
- `config` - Show configuration
- `h`/`help` - Open command help
- `t`/`tut`/`tutor` - Open tutor
- `s/old/new/` - Regex Substitute
- `s/foo/bar/g` - Replace all matches in line
- `s/foo/bar/c` - Replace with confirmation
- `s/foo/bar/gc` - Replace all in line (confirm)
- `s/foo/bar/i` - Case-insensitive replace
- `s/foo/bar/l` - Literal (no regex) replace
- `%s/foo/bar/gc` - Replace in whole file (confirm)
