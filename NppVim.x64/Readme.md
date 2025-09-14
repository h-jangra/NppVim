# NppVim

NppVim is a Notepad++ plugin that brings Vim-style modal editing and key bindings to Notepad++.

## Features

* **Modes:** Normal, Insert, Visual (character/line-wise).
* **Mode Switching:** `i` → Insert, `v` → Visual, `V` → Visual Line, `ESC` → Normal.
* **Navigation:** `h`, `j`, `k`, `l`, `w`, `b`, `0`, `$`,`o`,`O`.
* **Editing:** `dd` (delete line), `yy` (yank line), `p` (paste), `D` (delete to end), `u` (undo), `o` (new line).
* **Repeat Counts:** Prefix commands with numbers (e.g., `3j`,`d3d`).

## Notes

* Partial Vim support.
* Modal behavior: commands in Normal/Visual, typing in Insert.
* Repeat counts supported for most commands.

## License

See [LICENSE](LICENSE).
