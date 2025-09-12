# NppVim

NppVim is a Notepad++ plugin that brings Vim-style modal editing and key bindings to Notepad++.

## Features

* **Modes:** Normal, Insert, Visual (character/line-wise).
* **Mode Switching:** `i` → Insert, `v` → Visual, `V` → Visual Line, `ESC` → Normal.
* **Navigation:** `h`, `j`, `k`, `l`, `w`, `b`, `0`, `$`.
* **Editing:** `dd` (delete line), `yy` (yank line), `p` (paste), `D` (delete to end), `u` (undo), `o` (new line).
* **Repeat Counts:** Prefix commands with numbers (e.g., `3j`).
* **Visual Mode:** Expand/shrink selection using navigation keys.
* **Extras:** Toggle Vim Mode and view About info from the menu.

## Installation

1. Build with Visual Studio 2022 (C++14).
2. Place DLL in Notepad++ `plugins` folder.
3. Restart Notepad++.

## Usage

Enable Vim emulation via **Toggle Vim Mode**. Use the key bindings as in Vim.

## Notes

* Partial Vim support.
* Modal behavior: commands in Normal/Visual, typing in Insert.
* Repeat counts supported for most commands.

## License

See [LICENSE](LICENSE).
