# qBittorrent - Modern UI fork

A personal fork of [qBittorrent] with a native, modern dark interface and a
leaner, configurable build. It tracks upstream qBittorrent (C++ / Qt, powered by
[libtorrent] by Arvid Norberg) and adds a reworked desktop UI plus compile-time
options to strip features you don't need.

> This is an unofficial fork maintained for personal use. For the official,
> supported client, releases and support, see the [upstream project](https://www.qbittorrent.org).

---

## Highlights

**Interface**

- **Native custom title bar** - a frameless, dark title bar with the app menu and
  integrated window controls, implemented with plain Win32 (`dwmapi`), with no
  extra dependencies. Windows 11 Snap Layouts and native move/resize are kept.
- **Modern dark theme by default** - a built-in dark stylesheet with blue accents,
  flat rounded progress bars, a card-style properties panel and a 2D piece map.
  The theme system is preserved, so custom themes still work.
- **Left vertical toolbar** with a **top filter bar** (search box above the list).
- **Configurable layout** - a richer *View* menu lets you hide the status bar, the
  filters sidebar (or just its *Status / Categories / Tags / Trackers* sections),
  the top filter bar, the details panel and the tab bar.
- **Filters as comboboxes** - optionally replace the left sidebar with
  *Status / Categories / Tags* dropdowns in the top bar.
- **Customizable toolbar** - right-click the toolbar to show or hide individual
  buttons (persisted).
- Floating speed graph, Windows taskbar progress + jump list, and a compact-rows
  density toggle.

**Behavior**

- **Default download options** - optionally enable *Download in sequential order*
  and *Download first and last pieces first* by default for every new torrent
  (applied at the session level: Add dialog, drag & drop, magnet links, WebAPI).

**Build**

- **Compile-time feature flags** to produce a lighter, faster-starting binary -
  RSS, search engine, WebUI and the Lua plugin engine can each be removed at
  compile time (see [Build options](#build-options)). A GUI-only build drops from
  ~13 MB to ~9 MB.

---

## Building from source (Windows)

The fork is developed and built on Windows with the MSVC toolchain.

### Prerequisites

| Component            | Version / notes                                            |
| -------------------- | ---------------------------------------------------------- |
| Visual Studio        | 2022 Build Tools (MSVC v143) or newer                      |
| CMake + Ninja        | CMake >= 3.x, Ninja                                         |
| Qt                   | 6.8.3 (`msvc2022_64`)                                      |
| vcpkg                | with `boost`, `openssl`, `zlib` (x64-windows triplet)      |
| libtorrent-rasterbar | 2.0.x, built and installed (e.g. to `C:\libtorrent`)       |

### Configure & build

Run the following from a developer command prompt (so MSVC, CMake and Ninja are on
`PATH`). Adjust the paths to match your Qt, vcpkg and libtorrent locations.

```bat
cmake -B build -S . -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DQt6_DIR=C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6 ^
  -DLibtorrentRasterbar_DIR=C:/libtorrent/lib/cmake/LibtorrentRasterbar ^
  "-DCMAKE_PREFIX_PATH=C:/libtorrent;C:/Qt/6.8.3/msvc2022_64"

cmake --build build
```

To run the result, deploy the Qt runtime next to the executable:

```bat
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release build\qbittorrent.exe
```

Then copy `libssl-3-x64.dll` (from vcpkg) and `torrent-rasterbar.dll` (from your
libtorrent install) beside `build\qbittorrent.exe`.

> On other platforms, the standard upstream qBittorrent build instructions apply;
> see the [INSTALL](INSTALL) file.

### Build options

All feature flags default to **ON**; turn one **OFF** to remove that feature from
the binary entirely.

| Option     | Default | Effect when `OFF`                              |
| ---------- | ------- | ---------------------------------------------- |
| `GUI`      | ON      | Build the headless `qbittorrent-nox` instead   |
| `WEBUI`    | ON      | Remove the built-in HTTP remote-control server |
| `PLUGINS`  | ON      | Remove the Lua plugin engine                   |
| `RSS`      | ON      | Remove RSS feeds support                       |
| `SEARCH`   | ON      | Remove the search-engine integration           |

For example, a minimal desktop client - GUI only, no WebUI / plugins / RSS / search:

```bat
cmake -B build-min -S . -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DRSS=OFF -DSEARCH=OFF -DWEBUI=OFF -DPLUGINS=OFF ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DQt6_DIR=C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6 ^
  -DLibtorrentRasterbar_DIR=C:/libtorrent/lib/cmake/LibtorrentRasterbar ^
  "-DCMAKE_PREFIX_PATH=C:/libtorrent;C:/Qt/6.8.3/msvc2022_64"

cmake --build build-min
```

---

## Credits

This fork is built on top of the excellent work of the upstream
[qBittorrent] project and its contributors, and of the [libtorrent] library by
Arvid Norberg. All of the core BitTorrent functionality comes from them.

The free [IP to Country Lite database] by [DB-IP] is used for resolving the
countries of peers, under the
[Creative Commons Attribution 4.0 International License].

## License

Same as upstream qBittorrent: the source code is licensed under the GNU General
Public License, version 2 or (at your option) any later version (GPLv2+); binary
distributions are GPLv3+ because they bundle GPLv3+ assets. See [COPYING] for the
full text and the OpenSSL linking exception.

[qBittorrent]: https://www.qbittorrent.org
[libtorrent]: https://www.libtorrent.org
[IP to Country Lite database]: https://db-ip.com/db/download/ip-to-country-lite
[DB-IP]: https://db-ip.com/
[Creative Commons Attribution 4.0 International License]: https://creativecommons.org/licenses/by/4.0/
[COPYING]: COPYING
