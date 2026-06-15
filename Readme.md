# NppVim – Vim Mode for Notepad++

[![Release](https://img.shields.io/github/v/release/h-jangra/NppVim)](https://github.com/h-jangra/NppVim/releases)
[![Downloads](https://img.shields.io/github/downloads/h-jangra/NppVim/total)](https://github.com/h-jangra/NppVim/releases)
[![Stars](https://img.shields.io/github/stars/h-jangra/NppVim)](https://github.com/h-jangra/NppVim/stargazers)
[![Issues](https://img.shields.io/github/issues/h-jangra/NppVim)](https://github.com/h-jangra/NppVim/issues)

![Editor](https://img.shields.io/badge/Editor-Notepad++-90E59A?logo=notepadplusplus&logoColor=black)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows&logoColor=white)
![Vim](https://img.shields.io/badge/Vim-Modal%20Editing-019733?logo=vim&logoColor=white)
![Build](https://img.shields.io/badge/Build-Stable-brightgreen)
[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C?logo=paypal&logoColor=white)](https://paypal.me/h8imansh8u)

NppVim brings **Vim-like modal editing** to Notepad++.

## Features
- **Navigation:** `h j k l w b e 0 $ gg G { } ( ) %`
- **Insert/Edit:** `i a I A o O`
- **Delete/Yank/Paste:** `d c y p x X D C`
- **Visual Mode:** `v V <C-v>`
- **Search:** `/ ? n N * #`
- **Text Objects:** `iw aw i( a( i" a" ip ap`
- **Character Find:** `f F t T ; ,`
- **Undo/Repeat:** `u .`
- **Tabs & Macros:** `gt gT q @`
- **Command Mode:** `:w :q :wq :bn :bp`
- **Substitution:** `:s / %s`
- **Marks & Registers:** `m ' :reg`
- **Full Features**: `:tutor`/`t` or `:help`/`:h`

## Quick Start

| Mode | Key |
|-----|-----|
| Insert mode | `i` |
| Normal mode | `Esc` |
| Visual modes | `v`/`V`/`Ctrl+q` |
| Command mode | `:` |

## Configuration

NppVim uses two configuration files to manage editor behavior, custom mappings, and interface integrations:

1. **`nppvim.rc`** (Vim Settings): Used for Vim-style settings and mappings. Example: [NppVim RC file](docs/nppvim.rc)
   - Located by default at `%APPDATA%\Notepad++\plugins\Config\NppVim\nppvim.rc` (or `%USERPROFILE%\.nppvimrc` if it exists).
2. **`config.ini`** (Plugin & UI Settings): Used for general settings (e.g., custom escape keys, clipboard integration, and overriding conflicting Notepad++ shortcut keys like `Ctrl+D`, `Ctrl+U`, `Ctrl+V`).
   - Located at `%APPDATA%\Notepad++\plugins\Config\NppVim\config.ini`.

For a full list of supported settings and options, please refer to the [Help Documentation (Markdown)](docs/help.md) or the plain-text [Help File](docs/help.txt).

## Documentation

- [Help (Markdown)](docs/help.md)
- [Help (Plain Text)](docs/help.txt)
