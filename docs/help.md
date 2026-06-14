# NppVim Help

Welcome to NppVim! This plugin brings Vim-like modal editing to Notepad++.

## Getting Started

Press `ESC` to enter Normal Mode. From there, you can use standard Vim motions like `h`, `j`, `k`, `l` to move the cursor.

Use `:` to enter Command Mode for file operations and configuration.

## Configuration

NppVim uses two separate files for configuration to keep editing preferences and UI behavior organized.

### nppvim.rc (Vim Environment)

This is your primary startup script, similar to a `.vimrc` file. It is used for **Vim-specific settings**, custom mappings, and executing commands on startup. 

**Note:** Any command-mode command (e.g., `:w`, `:split`, `:command`) can be placed in this file.

#### Supported Options in .rc

You can configure Vim-specific options in `nppvim.rc` using the `:set <option>` command (or `:set <option>=<value>` for parameters). Boolean options can be disabled by prefixing them with `no` (e.g., `set nonumber` or `set nornu`).

| Short Name | Long Name | Type | Default | Description |
|---|---|---|---|---|
| `nu` | `number` | Boolean | `false` | Enable absolute line numbering. Disable with `nonumber` / `nonu`. |
| `rnu` | `relativenumber` | Boolean | `false` | Enable relative line numbering. Disable with `norelativenu` / `nornu`. |
| - | `hlsearch` | Boolean | `true` | Highlight all search matches. Disable with `nohlsearch`. |
| - | `ignorecase` | Boolean | `false` | Ignore case in search patterns. Disable with `noignorecase`. |
| - | `smartcase` | Boolean | `false` | Override `ignorecase` if search pattern contains uppercase characters. Disable with `nosmartcase`. |
| - | `clipboard` | String | `unnamed` | Clipboard mode. Set to `unnamed` to share registers with Windows clipboard. |
| - | `expandtab` | Boolean | `false` | Expand tabs to spaces. Disable with `noexpandtab`. |
| - | `tabstop` | Number | `4` | Number of spaces that a `<Tab>` character in the file counts for. |
| - | `shiftwidth` | Number | `4` | Number of spaces to use for each step of (auto)indent. |
| - | `wrap` | Boolean | `false` | Visually wrap long lines. Disable with `nowrap`. |
| - | `cursorline` | Boolean | `false` | Highlight the line containing the cursor. Disable with `nocursorline`. |
| - | `list` | Boolean | `false` | Show whitespace characters (tabs and trailing spaces). Disable with `nolist`. |
| - | `scrolloff` | Number | `0` | Minimal number of screen lines to keep above and below the cursor. |
| - | `keylayout` | Boolean | `false` | Enable automatic keyboard layout switching. Disable with `nokeylayout`. |
| - | `normallayout` | String | `en-US` | Keyboard layout target for Normal mode. |
| - | `insertlayout` | String | `system` | Keyboard layout target for Insert mode. |
| - | `langmap` | String | `""` | Translate characters in Normal/Visual modes (e.g., for non-US keyboard layouts). |
| `tw` | `textwidth` | Number | `0` | Maximum width of text being inserted (draws a visual vertical boundary edge line in Scintilla at the column, e.g. `set tw=80`). Set to `0` to disable. |

##### Hybrid Line Numbering
To enable **Hybrid Line Numbering** (which shows the absolute line number on the current line and relative line numbers on all other lines), set both options:
```vim
set number
set relativenumber
```

#### Mappings in .rc

Define custom key mappings and aliases using the following syntax:

```vim
" Syntax: [n|v|i][nore]map <keys> <action>
" Syntax: command <Alias> <command>

nmap ; :
nnoremap <C-s> :w<CR>
command W w
```

### config.ini (UI & Shortcut Overrides)

This file stores settings managed via the **Configuration Dialog** (`Plugins -> NppVim -> Configuration Dialog`). These settings control how NppVim interacts with Notepad++ itself.

**Note:** Changes made in the Dialog box are saved to `config.ini` only. They are not added to your `.rc` file.

#### Supported Keys in `config.ini`

Below is the list of keys recognized under the `[General]` section in `config.ini`:

##### General Settings
| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` or `vim_enabled` | Boolean (`1`/`0` or `true`/`false`) | `1` | Enable or disable the NppVim emulator. |
| `show_status_bar` | Boolean (`1`/`0` or `true`/`false`) | `1` | Toggles visibility of the Notepad++ status bar. |
| `debug_logging` | Boolean (`1`/`0` or `true`/`false`) | `0` | Enables debug logging to a text file for troubleshooting. |
| `rc_file` | String (Path) | *(empty)* | Custom path to the startup script file. If empty, NppVim looks for `%USERPROFILE%\.nppvimrc` first, then falls back to `%APPDATA%\Notepad++\plugins\Config\NppVim\nppvim.rc`. |
| `escape_key` | String | `esc` | Main escape key/sequence. Allowed values: `esc`, `jj`, `jk`, `kj`, or `custom`. |
| `escape_timeout` | Number (ms) | `100` | Timeout in milliseconds (between 100 and 1000) for matching multi-key escape sequences (like `jj`). |
| `custom_escape` | String | *(empty)* | Custom escape key sequence if `escape_key` is set to `custom`. |

##### Clipboard Integration Settings
| Key | Type | Default | Description |
|---|---|---|---|
| `x_store_clipboard` | Boolean (`1`/`0` or `true`/`false`) | `0` | If enabled, deleting a character with `x`/`X` updates the system clipboard. |
| `d_store_clipboard` | Boolean (`1`/`0` or `true`/`false`) | `0` | If enabled, deleting text with `d`/`D` (or related motions) updates the system clipboard. |
| `c_store_clipboard` | Boolean (`1`/`0` or `true`/`false`) | `0` | If enabled, changing text with `c`/`C` (or related motions) updates the system clipboard. |

##### Shortcut Override Settings
These keys resolve conflicts with native Notepad++ shortcuts. When set to `1` or `true`, NppVim overrides the native Notepad++ shortcut in Normal and Visual modes to use the Vim functionality instead:

| Key | Vim Function | Native Notepad++ Function | Default |
|---|---|---|---|
| `override_ctrl_d` | Half-page down | Duplicate Line | `0` |
| `override_ctrl_u` | Half-page up | Convert to Lowercase | `0` |
| `override_ctrl_r` | Redo | Redo | `0` |
| `override_ctrl_f` | Page forward | Find Dialog | `0` |
| `override_ctrl_b` | Page backward | Select/Match Bracket | `0` |
| `override_ctrl_o` | Jump backward in jump list | Open File Dialog | `0` |
| `override_ctrl_i` | Jump forward in jump list | *(custom)* | `0` |
| `override_ctrl_v` | Enter Visual Block mode | Paste | `0` |
| `override_ctrl_a` | Increment number under cursor | Select All | `0` |
| `override_ctrl_x` | Decrement number under cursor | Cut | `0` |

##### Keyboard Layout Settings
Used to configure automatic keyboard layout switching:
| Key | Type | Default | Description |
|---|---|---|---|
| `keyboard_layout_switching` or `keylayout` | Boolean (`1`/`0` or `true`/`false`) | `0` | Enables automatic layout switching when moving between modes. |
| `normallayout` | String | `en-US` | Keyboard layout target for Normal mode (e.g. `en-US`). |
| `insertlayout` | String | `system` | Keyboard layout target for Insert mode (`system` uses the last active layout). |

---

## Relative Line Numbers

NppVim provides a high-performance, themed relative numbering system. It automatically integrates with Scintilla's line number margin, calculating relative offsets dynamically.

## Keyboard Layout Switching

NppVim can automatically switch your Windows keyboard layout when entering and exiting Insert mode.

### Configuration Example in `.rc`

```vim
set keylayout " Enable switching
set normallayout=en-US
set insertlayout=ru-RU
```

Common Layout Codes:
- `en-US` (English - US)
- `en-GB` (English - UK)
- `ru-RU` (Russian)
- `hi-IN` (Hindi)
- `ko-KR` (Korean)
- `ja-JP` (Japanese)
