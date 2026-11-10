# openciv4-dll

The game DLL of *Civilization IV: Beyond the Sword* (`CvGameCoreDLL`, the 3.19 SDK Firaxis
released for modding) ported to **x64**, current MSVC and Python 2.7 x64, for
[openciv4](https://github.com/bssth/openciv4), an experimental x64 engine for the game.

It is also the base for porting a mod's own DLL: the compatibility layer here lets a mod's
sources build for x64 with a small set of changes, and the documents below explain those
changes step by step.

> **Issues go to the main repository:** <https://github.com/bssth/openciv4/issues>.
> Problems with this DLL, with porting a mod, or with the engine are all tracked there
> (mention `openciv4-dll` in the title). Issues are not tracked in this repository.

## What is here

| Path | What it is |
|---|---|
| `CvGameCoreDLL/` | The SDK sources, imported unmodified in the first commit, then ported (each change is marked `openciv4`) |
| `compat/` | What replaces the SDK's bundled third-party code: the Boost.Python 1.32 subset the SDK uses, implemented on pybind11; Boost.Bind/Function/SmartPtr/Thread on the standard library; a `stdext::hash_map` that iterates in the VC7.1 order |
| `GameCore.cmake` | `oc4_add_game_core()`: builds the SDK core or a mod's derivative of it as `CvGameCoreDLL-x64.dll` |
| `CMakeLists.txt` | The vanilla core, and optionally a mod's core (`OC4_MOD_SOURCE_DIR`) when built on its own |
| `tools/check-exports.ps1` | Checks that a DLL exports everything the engine imports from it |
| `docs/` | The documentation (below) |

## Documentation

* [Porting a mod's DLL to openciv4](docs/PORTING.md): the full walkthrough, from the mod's
  sources to a game that plays and saves, with the typical changes and examples.
* [What the engine expects of a DLL](docs/ENGINE-CONTRACT.md): exports, data layouts, the
  process the DLL runs in, the Python side.
* [Troubleshooting](docs/TROUBLESHOOTING.md): symptoms and their usual causes.
* [Porting with an AI assistant](docs/AI-PROMPT.md): a prompt to give a coding agent, and how
  to check its work.
* [Notes on the SDK port](docs/SDK-PORT.md): what was changed in the SDK and why, the compat
  layer's details, the speed changes.

## Building

Requirements: Windows x64, Visual Studio 2022 or newer with **Desktop development with C++**
(CMake and Ninja come with it), and Python 2.7:

* Python 2.7.18 x64 from python.org, extracted without installing:
  `msiexec /a python-2.7.18.amd64.msi /qn TARGETDIR=%CD%\external\python27`

pybind11 is included as the `third_party/pybind11` submodule, pinned to the
[Python 2.7-compatible fork fix](https://github.com/0x12A01DC/pybind11/commit/54ec6911f50353a866548a59c867290d674346e9)
for MSVC 14.52 with `/permissive-`. Clone with `git clone --recursive`, or run
`git submodule update --init --recursive` in an existing checkout. No manual archive
download is needed.

From an **x64 Native Tools Command Prompt**:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The DLL is `build\bin\CvGameCoreDLL-x64.dll` (with its `.pdb`). Another Python location: `-DOC4_PYTHON27_DIR=...`.
`OC4_PYBIND11_DIR` can override the bundled headers if needed; an override must retain
Python 2.7 support and the compiler fix. Link-time code generation is on by
default (`-DOC4_LTO=OFF` to turn it off, for faster links while porting).

A mod's DLL is built the same way, next to the vanilla one, into `build\mod\CvGameCoreDLL-x64.dll`,
which goes into the mod's `Assets` folder beside its 32-bit `CvGameCoreDLL.dll`:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DOC4_MOD_SOURCE_DIR=D:/MyMod/CvGameCoreDLL ^
    "-DOC4_MOD_DEFINITIONS=MY_FLAG;OTHER_FLAG" ^
    -DOC4_MOD_DEF_FILE=D:/MyMod/CvGameCoreDLL/CvGameCoreDLL64.def
cmake --build build
```

See [docs/PORTING.md](docs/PORTING.md) for what the options mean and what to change in the
mod's sources first.

Inside openciv4 this repository is the `gamecore` submodule; the engine's build includes it
and compiles itself against these headers.

## Downloads

Every push builds the DLL on GitHub Actions; the DLL and its `.pdb` are attached to the run
as the `CvGameCoreDLL-x64` artifact. Tags named `v*` are published as releases with a zip of
the same files.

## Status and differences from the original DLL

The vanilla core loads all of the game's XML, starts and plays games (AI autoplay is
deterministic per seed), and saves and loads `.CivBeyondSwordSave` files in the original
format, including saves made by the original game. Differences that matter to modders:

* Floating point is SSE2 instead of x87: float-based AI evaluations can round differently, so
  a game does not replay exactly as it would in the 32-bit DLL.
* A few bugs the 32-bit DLL survived by luck are fixed (pointers into temporaries, a wrong
  Python binding); see [docs/SDK-PORT.md](docs/SDK-PORT.md).
* A few hot paths are faster with identical game results.

## License

`CvGameCoreDLL/` is Firaxis' SDK source and remains under the SDK's terms (see the game's SDK
documentation); a mod built from it is under the same terms plus the mod's own. `compat/`, the
build files, the tools and the documentation are under the MIT License ([LICENSE](LICENSE)).
