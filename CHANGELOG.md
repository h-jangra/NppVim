
# 1.8.0.0
## Fixed

* `Visual mode stability` – Corrected selection behavior and motion consistency
* `iW / aW` – Improved big-word text object handling

---

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
- `e` / `edit` – Edit file
- `bp` / `bprev` / `bprevious` – Go to previous buffer
- `bd` / `bdelete` – Delete buffer
- `sp` / `split` – Horizontal split
- `vs` / `vsplit` – Vertical split
- `tabn` / `tabp` – Next / previous tab
- `tabnew` – Open new tab
- `tabclose` – Close current tab
