# Installing TVIDE

## Prerequisites

| Requirement | Notes |
|-------------|-------|
| C++17 compiler | GCC 7+ or Clang 5+ |
| C99 compiler | for libvterm |
| CMake | 3.16 or newer |
| ncurses (wide) | terminal rendering |
| GPM | mouse support on Linux console (optional but recommended) |
| Perl | required at build time by libvterm (encoding-table generator) |
| `<pty.h>` (libutil) | Linux/glibc — for `forkpty()` in terminal windows |

### Debian / Ubuntu

```bash
sudo apt install build-essential cmake libncurses-dev libgpm-dev perl
```

### Fedora / RHEL

```bash
sudo dnf install gcc-c++ cmake ncurses-devel gpm-devel perl
```

### Arch / Manjaro

```bash
sudo pacman -S base-devel cmake ncurses gpm perl
```

### macOS (Homebrew)

```bash
brew install cmake ncurses
# GPM is Linux-only; harmless to skip on macOS.
```

## Build

This project uses [tvision 2.0](https://github.com/magiblot/tvision) as a git
submodule. Clone with `--recursive`:

```bash
git clone --recursive https://github.com/macino/tvide.git
cd tvide
```

If you cloned without `--recursive`, fetch submodules now:

```bash
git submodule update --init --recursive
```

Configure and build (Release):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The binary lands at `./build/tvide`.

## Run

```bash
./build/tvide                # empty IDE
./build/tvide path/to/file   # open a file
./build/tvide path/to/proj   # open a project directory
```

## Optional: install to system

```bash
sudo cmake --install build --prefix /usr/local
```

Installs `tvide` into `/usr/local/bin`.

## Troubleshooting

- **`forkpty was not declared`** — your include path is shadowing system
  `pty.h`. Make sure no `pty.h` exists in the `include/` directory.
- **`Perl was not found`** — install perl; libvterm's encoding tables are
  generated at build time.
- **No mouse in Linux console** — install `libgpm-dev` and run `gpm` daemon,
  or run TVIDE under a terminal emulator that natively supports mouse events.
- **Box-drawing characters look like garbage** — your terminal locale is not
  UTF-8. Set `LANG=en_US.UTF-8` (or your locale's UTF-8 variant).
- **Submodule URL changed / "tvision missing"** — re-init submodules:

  ```bash
  git submodule sync --recursive
  git submodule update --init --recursive
  ```

## What's bundled

- `deps/tvision` — pinned commit of magiblot/tvision (submodule)
- `libvterm/` — vendored copy of neovim/libvterm + custom CMakeLists
- `src/tvterm-core/` — PTY/vterm glue adapted from tvterm

You don't need separately-installed copies of any of these.
