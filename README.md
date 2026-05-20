# Prism

> [!IMPORTANT]  
> Used various LLMs as a playground for code generation and assistance in forking/structuring/coding the project.

Prism is a Qt 6 desktop app that turns supported shader inputs into installable KWin effects for KDE Plasma 6.

Supported inputs:

- two GLSL fragment shaders (`open_color` and `close_color`)
- a supported `.kdl` file with `window-open` and `window-close` custom shaders

Generated effects are installed under:

```text
~/.local/share/kwin/effects/kwin6_effect_glsl_<category-id>_<effect-name>
```

Prism reloads KWin so new effects appear in System Settings.

## UI

- **Import Effect**: name, category, shader/KDL input, install
- **Manage Effects**: list/search/remove Prism-installed effects
- **About**: version, author, project link

## Categories

- `open-close` (Window Open/Close Animation)
- `minimize-restore` (Minimize/Restore Animation)

## Import rules

### GLSL

- first shader must expose `open_color`
- second shader must expose `close_color`
- optional reuse of first shader for both directions

### KDL

Supported only when `.kdl` contains:

- `window-open` + `custom-shader`
- `window-close` + `custom-shader`

If `duration-ms` exists in `window-open`, Prism uses it as default duration.

## Requirements

- KDE Plasma 6 / KWin 6
- Qt 6 (`Core`, `DBus`, `Widgets`)
- CMake 3.20+
- C++20 compiler

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/prism
```

Use a fresh build dir if needed:

```bash
cmake -S . -B build-prism
cmake --build build-prism
./build-prism/prism
```

## CLI install

```bash
./build/prism --install --name "My Effect" --category open-close --first /path/open.glsl --second /path/close.glsl
```

Options:

- `--install`
- `--name <name>`
- `--category <open-close|minimize-restore>`
- `--kdl <path>`
- `--first <path>`
- `--second <path>`
- `--reuse-first-for-both`

Unknown or missing `--category` defaults to `open-close`.

## Notes

- Prism validates `open_color` and `close_color`; missing entry points fail install.
- Installed effects can be removed from **Manage Effects**.
- If KWin reload fails, Prism keeps the generated package and reports the failure.

## Version

- `0.4.0`
- Pete Vagiakos
- <https://www.github.com/Kombatant/prism>
