# NppVim

NppVim is a Notepad++ plugin that brings Vim-style modal editing and key bindings to Notepad++. It allows you to use familiar Vim navigation, editing, and selection commands directly in Notepad++.

## Features

- **Vim Modes:**  
  - **Normal Mode:** Navigate and manipulate text using Vim-like commands.
  - **Insert Mode:** Type and edit text as in standard Notepad++.
  - **Visual Mode:** Select text using Vim-style motions (character-wise and line-wise).

- **Mode Switching:**
  - `i` — Enter Insert mode.
  - `v` — Enter/exit Visual (character-wise) mode.
  - `V` — Enter/exit Visual Line mode.
  - `ESC` — Return to Normal mode from Insert or Visual mode.

- **Navigation (Normal/Visual mode):**
  - `h` — Move cursor left.
  - `l` — Move cursor right.
  - `j` — Move cursor down.
  - `k` — Move cursor up.
  - `w` — Move to start of next word.
  - `b` — Move to start of previous word.
  - `0` — Move to start of line (only if not entering a repeat count).
  - `$` — Move to end of line.

- **Repeat Counts:**
  - Prefix navigation and editing commands with a number to repeat them (e.g., `3j` moves down 3 lines).

- **Editing:**
  - `dd` — Delete current line (can be repeated, e.g., `3dd`).
  - `yy` — Yank (copy) current line (can be repeated).
  - `p` — Paste clipboard at cursor.
  - `D` — Delete from cursor to end of line.
  - `u` — Undo last change.
  - `o` — Open a new line below and enter Insert mode.

- **Visual Mode:**
  - Use navigation keys (`h`, `j`, `k`, `l`, `w`, `b`, `0`, `$`) to expand/shrink selection.
  - `v` — Exit Visual mode (if in character-wise).
  - `V` — Exit Visual mode (if in line-wise).

- **Other:**
  - **About:** Menu entry to show plugin information.
  - **Toggle Vim Mode:** Menu entry to switch between Normal and Insert mode.

## Installation

1. Build the plugin using Visual Studio 2022 (C++14).
2. Place the compiled DLL in the Notepad++ `plugins` directory.
3. Restart Notepad++.

## Usage

- After installation, use the menu entry **Toggle Vim Mode** to enable/disable Vim emulation.
- Use the key bindings above to navigate and edit text as you would in Vim.

## Notes

- Only a subset of Vim's features are implemented.
- The plugin is modal: most keys are interpreted as commands in Normal/Visual mode, and as text input in Insert mode.
- Repeat counts work for most navigation and editing commands.
- Visual mode supports both character-wise and line-wise selection.

## License

See [LICENSE](LICENSE) for details.

---

**Enjoy modal editing in Notepad++ with NppVim!**