# Porting a mod's DLL to openciv4

openciv4 is a 64-bit engine, so it cannot load the 32-bit `CvGameCoreDLL.dll` a mod ships
in its `Assets` folder. A mod that has its own DLL needs that DLL rebuilt for x64 from its
sources, as `CvGameCoreDLL-x64.dll` beside the 32-bit one: the original game keeps loading
its DLL and openciv4 loads the other, so one copy of the mod serves both. Mods without a DLL
of their own (XML, Python and art only) need nothing from this guide: they run on the
vanilla core as they are.

The work, in order:

1. [Prepare](#1-prepare): the engine, the mod, its DLL sources, a git repository.
2. [First build](#2-first-build): the build flags, the file list.
3. [Compile errors](#3-compile-errors): VC7.1 code under a current compiler.
4. [64-bit correctness](#4-64-bit-correctness): what compiles but is wrong on x64.
5. [Exports](#5-exports-and-the-def-file): what the engine imports, the `.def` file.
6. [First run](#6-first-run-the-xml): the XML loads.
7. [Games](#7-new-games-and-autoplay): new games, AI autoplay, determinism.
8. [Saves](#8-saves): save, load, the original's saves.
9. [Memory bugs](#9-memory-bugs): what the 32-bit DLL survived by luck.
10. [Playing and shipping](#10-playing-and-shipping) the port.
11. [Mod updates](#11-keeping-up-with-the-mods-updates): merging a newer source drop.

Steps 3 to 5 take most of the time for a big mod; most of it is mechanical. If you want to
hand it to a coding agent, [AI-PROMPT.md](AI-PROMPT.md) has a prompt that follows this guide.

Problems, questions, a mod that will not port: open an issue in the main repository,
<https://github.com/bssth/openciv4/issues>, not in this one.

---

## 1. Prepare

You need:

* **Civilization IV: Beyond the Sword 3.19** installed (Steam, GOG or disc). openciv4 reads
  the game's files from the install and writes nothing into it.
* **An openciv4 engine folder**: `oc4core.exe` with `CvGameCoreDLL-x64.dll`, `python27.dll`, the
  `python` folder and the runtime DLLs next to it, from the
  [releases](https://github.com/bssth/openciv4/releases) or built from the openciv4 sources.
  The engine loads the mod's `Assets\CvGameCoreDLL-x64.dll`, or the vanilla one next to its
  own exe when the mod has none. One engine folder runs every mod.
* **The mod's DLL sources, matching the mod's XML and Python.** A DLL reads the XML schema it
  was written for; sources from another version of the mod load its XML partly or not at all.
  If the mod's author shares the sources but does not publish them, keep your port private
  too.
* **This repository** and its requirements (see the [README](../README.md#building)).

Put the mod's sources in a git repository of their own and **commit them unmodified first**,
including the `Makefile` or project files (they record the build flags). Every change after
that is a separate commit, and every changed line gets a short `openciv4` comment. Then
`git diff <first commit>` is the whole port at any time, which is what you need to merge the
mod's next version (section 11) and what reviewers need to trust it.

```
git init MyMod-x64 && cd MyMod-x64
xcopy /E /I "D:\MyMod\CvGameCoreDLL" CvGameCoreDLL
git add -A && git commit -m "Import MyMod DLL sources (unmodified)"
```

## 2. First build

The mod's build flags are in its `Makefile` or `.vcproj`/`.vcxproj`, in the configuration the
mod ships (usually `Final_Release`): look for `/D` options. Pass the mod's own defines, not
the SDK's standard ones (`WIN32`, `_WINDOWS`, `_USRDLL`, `CVGAMECOREDLL_EXPORTS`, `NDEBUG`,
`FINAL_RELEASE` are set by `oc4_add_game_core` already).

`oc4_add_game_core` compiles **every `.cpp` in the folder**. Compare that with the files the
project actually builds: some mods keep unused files around. List the extra ones in
`OC4_MOD_EXCLUDE` (it holds `CvTextScreens.cpp` by default, which no SDK-based project
builds).

```bat
cmake -S D:\openciv4-dll -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOC4_LTO=OFF ^
    -DOC4_MOD_SOURCE_DIR=D:/MyMod-x64/CvGameCoreDLL ^
    "-DOC4_MOD_DEFINITIONS=_MOD_FRACTRADE;_MOD_GOVWORKERS;USE_MY_FEATURE" ^
    "-DOC4_MOD_EXCLUDE=CvTextScreens.cpp;OldUnusedFile.cpp"
cmake --build build --target CvGameCoreDLL_mod -- -k 0
```

`-DOC4_LTO=OFF` keeps links fast while you port; turn it back on for the build you play
(it makes AI turns much faster). `-k 0` makes Ninja go on after errors, so one build lists
every file that fails.

What `oc4_add_game_core` sets up for the mod: C++23, MSVC conformance options
(`/permissive- /Zc:__cplusplus /Zc:preprocessor`), Windows-1252 as the source and execution character set,
the SDK's legacy warnings off, `CvGameCoreDLL.h` as the precompiled header, `compat/` first on
the include path (so `#include <boost/python.hpp>` and `<hash_map>` find the replacements),
and Python 2.7 x64 with pybind11.

If the mod bundles its own copies of Boost or Python headers, remove them from its include
path (or delete them in a port commit): the compat layer replaces them, and mixing the two
does not compile.

## 3. Compile errors

The SDK was written for Visual C++ 7.1 (2003). The game core now uses `/permissive-` to
catch non-standard C++ and `/Zc:preprocessor` for standard macro expansion and token
pasting. Fixing these diagnostics reduces dependence on MSVC extensions and makes later
Clang/GCC ports easier; these flags alone do not guarantee portability. `/Zc:__cplusplus`
also makes `__cplusplus` reflect the selected language standard, so feature checks work
correctly.

Examples encountered in the SDK port:

* **String literals passed to mutable pointers (C2664).** XML tag names, default strings,
  logging formats and debug allocation filenames are read-only inputs: change `char*`
  to `const char*` (and likewise for `TCHAR*` and `wchar*`) in both declarations and
  definitions. Update interface overrides too. Keep buffers that receive text writable;
  do not cast away constness to silence the error.
* **Ambiguous conditional expressions (C2445).** `CvString` and `CvWString` convert both
  to and from character pointers. For example, change
  `m_pUnit ? m_pUnit->getName() : L""` to
  `m_pUnit ? m_pUnit->getName().c_str() : L""` when returning a `std::wstring`.
  The return value copies the text; do not retain a pointer into a temporary string.
* **Invalid token pasting in art-info macros.** `CvArtInfo##name##*` and
  `CvArtInfo##name##&` try to paste an identifier and punctuation into one token.
  Use `CvArtInfo##name *` and `CvArtInfo##name &`: only the identifier needs pasting.
  Macro parameters inside string literals do not expand either; use adjacent literals
  and stringification, such as `"get" #name "ArtInfo"`, for a generated message.

Keep the conformance flags enabled while fixing the mod. Further common changes follow;
the SDK sources provide examples in `CvGameCoreDLL`.

**Implementation details of the old standard library.** `std::list<T>::_Alloc::size_type`
and similar names no longer exist. Use the type the code means:

```cpp
// before
ReplayMessageList::_Alloc::size_type iSize = m_listReplayMessages.size();
// after (saves store it as 32 bits anyway, see section 4)
unsigned int iSize = m_listReplayMessages.size();
```

**Headers that VC7.1 included transitively.** Add the include where the name is used:

```cpp
#include <algorithm>	// openciv4: VC7.1 pulled this in transitively
```

**Python bindings need complete types.** Boost.Python accepted a forward-declared class in a
`.def(...)`; pybind11 (under the compat layer) needs the full declaration:

```cpp
#include "CyUnit.h"	// openciv4: pybind11 needs complete types
#include "CyPlot.h"	// openciv4: pybind11 needs complete types
```

**Bindings through the wrong class.** VC7.1 accepted a member function of an unrelated class
in a binding; a current compiler does not. These are real bugs (the old binding called the
function on the wrong object), so fix them to what the script name says:

```cpp
// before: in CvEventInfo's binding
.def("getNumUnits", &CvEventTriggerInfo::getNumUnits, "int ()")
// after
.def("getNumUnits", &CvEventInfo::getNumUnits, "int ()")	// openciv4: was bound to CvEventTriggerInfo::getNumUnits
```

**Inline assembly.** x64 MSVC has none. The SDK uses it for breakpoints; mods also use it to
read the return address or the frame pointer:

```cpp
// before
{ _asm int 3 }
// after
{ __debugbreak(); }
// a return address: _ReturnAddress() from <intrin.h>
```

**Removed standard library functions.** `std::random_shuffle`, `std::auto_ptr`,
`std::bind1st`/`bind2nd`, `std::unary_function` are gone since C++17. Replace them with
something that gives **the same results**, not just something equivalent: if the game's
random numbers feed it, the order of calls to the game's random generator must not change.
For example a shuffle that took `rand()` becomes the same Fisher-Yates loop on `rand()`.

**Other VC7.1-only syntax.** C-style casts applied to class templates, `_THROW` and other
macros of the old library, `for` loop variables used after the loop, templates that need `typename`, C++20's reversed `operator==` candidates
making comparisons of the mod's own iterator classes ambiguous (declare the operator
`const`, with both operands the same type). A bundled `stdint.h` from the 2003 era conflicts
with the compiler's own: delete it.

**Boost beyond Python.** `compat/boost/` provides `bind`, `function`, `shared_ptr`,
`static_assert` and the parts of `thread` that SDK-based mods use, on the standard library.
If a mod uses more of Boost, add what it needs to `compat/boost/` the same way (and send it
upstream in an issue or pull request).

**Warnings.** The SDK's legacy warnings are off. Read the remaining ones, at least
`C4311`/`C4312` (pointer truncation) and `C4302`: they point at 64-bit bugs (section 4).

## 4. 64-bit correctness

These compile and are wrong. Search for each of them; the compiler does not find them all.

**A pointer in an `int`.** `int`, `long` and `DWORD` are 32 bits on 64-bit Windows; pointers
are 64. Casting a pointer to one of them and back loses the upper half, usually only once
memory above 4 GB is in use, which makes it intermittent.

```cpp
// before: (int) applies to m_pUnit, not to the result: the pointer is truncated and tested
return (int) m_pUnit ? m_pUnit->getBuildType() : (int) NO_BUILD;
// after
return m_pUnit ? (int) m_pUnit->getBuildType() : (int) NO_BUILD;	// openciv4: was (int) m_pUnit ? ..., truncating the pointer on x64
```

An integer that carries a pointer (Python passing `0` for `NULL`, ids stored in a `void*`)
goes through `intptr_t`:

```cpp
return m_pPlayer ? m_pPlayer->canRevolution((CivicTypes*)(intptr_t)paeNewCivics) : false;	// openciv4: scripts pass 0 (NULL)
```

Look for: `(int)` and `(long)` casts of pointers, `DWORD` holding pointers, pointers in
`int` members or arrays, `%d`/`%x` printing pointers, hash functions over pointer values that
truncate them.

**What goes into saves.** The save format is defined by what the 32-bit DLL wrote. Anything
whose size changed on x64 changes the format, and then neither original saves nor the
port's own saves (between builds) load. `size_t` is the common case: `vector::size()`
written to the stream. The SDK port keeps the format by writing `size_t` as 4 bytes in
`FDataStreamBase`:

```cpp
#ifdef _WIN64
	// openciv4: container sizes were 32-bit on the original x86 build; keep the
	// save format by writing size_t as 4 bytes (non-virtual, vtable unchanged).
	void				Write(size_t value) { Write(static_cast<unsigned int>(value)); }
#endif
```

A mod with its own save wrapper (tagged save formats, stream classes) needs the same in each
of its `Write` overloads. Also check `time_t` (64-bit since VC 2005), pointers written as
ids, and `sizeof(...)` of structs written as raw bytes: a struct with a pointer or a `size_t`
in it is bigger on x64.

**Layouts assumed in code.** Mods sometimes assert the size of a class
(`BOOST_STATIC_ASSERT(sizeof(CvSelectionGroup) == 0x...)`), copy objects with `memcpy` from
one member to the end of the object, or compute sizes as `sizeof(Derived) - sizeof(Base)`.
On x64, pointers double and alignment adds padding (between a base class and the first
member, for example). Keep size asserts for 32-bit builds only, and compute copy ranges from
the members' actual addresses:

```cpp
// a copy from m_iFirstField to the end of the object
memcpy(&m_iFirstField, &other.m_iFirstField,
	reinterpret_cast<const char*>(&other + 1) - reinterpret_cast<const char*>(&other.m_iFirstField));
```

**Undefined behaviour the old compiler happened to tolerate.** `strcpy` with overlapping
source and destination (an optimised x64 `strcpy` copies in blocks; use `memmove`), reads
of uninitialised locals, evaluation order of function arguments (two `getSorenRandNum` calls
in one expression run in an unspecified order: the result is the same only if both have the
same range). These show up as "works in Debug, not in Release" or as games that diverge
between builds; section 9 finds most of them.

**Floating point.** x64 uses SSE2, not x87. Results of float code can differ in the last
bits from the 32-bit DLL, so an AI decision based on a float comparison can go the other way.
That is expected: the port cannot replay a game exactly as the original DLL played it, only
consistently with itself.

## 5. Exports and the .def file

The engine calls the DLL through its exported C++ functions, by their mangled names (and
reads a few classes' data through inline accessors, see
[ENGINE-CONTRACT.md](ENGINE-CONTRACT.md)). Mangled names differ between x86 and x64
(`?getX@CvPlot@@QBEHXZ` on x86, `?getX@CvPlot@@QEBAHXZ` on x64), so:

* Functions the mod exports with `DllExport` in the headers just work: the compiler mangles
  them for x64.
* A mod with a **`.def` file** exports some names under other functions, typically to give
  the engine a different implementation than the game uses internally
  (`?getX@CvPlot@@QBEHXZ=?getViewportX@CvPlot@@QBEHXZ`). That file is in x86 mangling and
  must be rewritten in x64 mangling as `CvGameCoreDLL64.def`, passed as `OC4_MOD_DEF_FILE`.

To convert a `.def`, take each line apart into the exported name and its target, turn each
x86 name into its declaration with `undname` (in the Visual Studio tools folder), and find
the same declaration among the x64 DLL's own exports (`dumpbin /exports`, which shows the
x64 names; `undname` on those gives the same declaration with `__ptr64` added). The pair of
x64 names is the new line. Check each line: a `.def` entry whose signature does not match
the engine's import is a silent bug (the engine calls a function with other parameters).

Then check the result against the engine:

```powershell
tools\check-exports.ps1 -Exe <engine folder>\oc4core.exe -Dll build\mod\CvGameCoreDLL-x64.dll
```

It lists every name `oc4core.exe` imports from the DLL that the DLL does not export, with
its declaration. The engine refuses to start with a DLL missing any of them ("does not export
what the engine uses", with the names). Usual reasons:

* the mod removed `DllExport` from a function the original engine did not use but openciv4
  does (plot and unit help texts, for example): export it again, or add a `.def` line;
* the mod changed a signature (a parameter added, `CvUnit*` instead of `CvSelectionGroup*`):
  keep the old signature as a wrapper and export that;
* the mod moved a class's implementation and dropped one of its functions: restore it.

Never change the engine side to match a mod: the engine imports the SDK's interface, and
every mod has to provide it.

## 6. First run: the XML

Put the new `CvGameCoreDLL-x64.dll` (and its `.pdb`) into the mod's `Assets` folder and start
the engine headless with the mod:

```bat
oc4core.exe --mod "Mods\MyMod" --log-dir D:\logs
oc4core.exe --mod "D:\Mods\MyMod"
```

`--mod` takes the mod's folder under `Beyond the Sword\Mods`, or the full path of a mod that
is not installed. The game install is found by itself (registry, Steam); `--civ4 <folder>`
(the folder with `Assets`, `Warlords` and `Beyond the Sword`) if not. `--log-dir` turns on
the DLL's own logs (`xml.log` and the rest) into that folder.

The engine prints each loading stage with `ok` or `FAILED` and its time
(`LoadPlayerOptions` ... `LoadPostMenuGlobals`), then some counts read back through Python
(units, buildings, techs). The DLL's message boxes (failed asserts, XML errors) are printed
instead of shown, each answered with its default button. A stage that fails:

* **at the first stages**: the DLL does not find its XML. It looks relative to its own
  location and the working folder, which the engine sets up as the original does (see
  [ENGINE-CONTRACT.md](ENGINE-CONTRACT.md#the-process)); check the `--mod` path.
* **"more siblings than memory allocated"**, a type not found: the XML is of another version
  of the mod than the sources.
* **a crash in a `read` function**: usually an array written past its end because the XML
  has more entries than a fixed-size array holds (the 32-bit DLL overwrote the next member
  and went on). Make the loop stop at the array's size; only that many were ever used.

## 7. New games and autoplay

```bat
oc4core.exe --mod "Mods\MyMod" --map Continents --size SMALL --seed 3 --turns 20
```

starts a new game (map script, world size, seed; `--players N`, `--civ`, the difficulty and
the rest have defaults) and lets the AI play it for that many turns, printing a line per
turn: alive players, cities, population, units, **the game's sync checksum**, milliseconds.

Run it twice: the checksums must be the same, turn by turn. Then run it in the map window
(`--view --view-seconds 120`): again the same. A difference means undefined behaviour
(uninitialised memory, a pointer used as a number, evaluation order) or state the DLL keeps
outside the game: find the first turn that differs and look at what the AI decided there.
`--rand-log` with `--log-dir` logs every synchronised random number, so two runs can be
compared call by call (compare seeds and ranges, not the labels).

Play longer games on bigger maps, several seeds, all eras. Crashes print a stack trace with
the DLL's function names (keep the `.pdb` next to the DLL).

## 8. Saves

```bat
oc4core.exe --mod "Mods\MyMod" --map Continents --seed 3 --turns 20 --save-at 10 --save D:\test.CivBeyondSwordSave
oc4core.exe --mod "Mods\MyMod" --load D:\test.CivBeyondSwordSave --turns 10
```

The first saves the game at turn 10 and plays on to turn 20; the second loads it and plays
the same ten turns. With the vanilla core the checksums of turns 11 to 20 are the same in
both: a loaded game continues as if it had never stopped. A mod may legitimately differ
(some mods drop caches or rebuild data on load, and the AI then decides differently), but a
**load must read exactly what the save wrote**. The engine checks that and says so when a
load reads less or more than was written; when it does,

```bat
oc4core.exe --mod "Mods\MyMod" --probe-save D:\test.CivBeyondSwordSave
```

shows where reading each part (game, map, teams, players) ends, and the engine prints the
size of each part when it saves. A part that reads a different number of bytes than it wrote
has a write and read that do not match: a `size_t` (section 4), or a field written under a
condition and read under another.

Then load saves made by the original game with the mod's 32-bit DLL. If those load, the save
format is unchanged.

## 9. Memory bugs

The 32-bit DLL got away with writes past arrays, reads of freed memory and uninitialised
heap: the memory there happened to be harmless. On x64 the layout differs and the same bug
crashes or changes games. AddressSanitizer finds them. In an openciv4 source tree, with this
repository as its `gamecore` submodule and the mod's core added there:

```bat
cmake -S . -B build\asan -G Ninja -DCMAKE_BUILD_TYPE=Release -DOC4_LTO=OFF -DOC4_PGO=OFF ^
    "-DCMAKE_CXX_FLAGS=/fsanitize=address" "-DCMAKE_C_FLAGS=/fsanitize=address"
cmake --build build\asan --target oc4core <your mod's target>
```

Run that engine with the Visual Studio tools folder (for
`clang_rt.asan_dynamic-x86_64.dll`) on `PATH`. `ASAN_OPTIONS=continue_on_error=2` reports
every error of a run instead of stopping at the first. Load the XML, start a game, play some
turns, save and load.

What it typically finds in SDK-based mods:

* XML arrays of fixed size (`NUM_..._PREREQS`, `NUM_..._BONUSES`) filled from XML that has
  more entries;
* `GET_PLAYER(NO_PLAYER)` or `GET_TEAM(NO_TEAM)`: memory before the array, read for unowned
  plots or units;
* a pointer into a temporary: `cond ? m_aszTags[i] : NULL` with `CvString` elements makes a
  copy and returns its buffer (`.c_str()` on the element fixes it);
* a loop that deletes a list node and then reads it;
* custom allocators (`HeapAlloc` pools for plots, cities) handing out memory that is not
  zeroed, while the class leaves members uninitialised: the original heap happened to give
  zeros. Zero the allocation.

A good final check: the ASan build and the optimised build play the same game (same
checksums every turn). Different memory layouts then lead to the same results, so no
undefined behaviour is left on that path.

## 10. Playing and shipping

```bat
oc4core.exe --mod "Mods\MyMod" --menu
```

opens the main menu (new game, load, mods); `--play --load <save>` continues a game
directly. Started from the menu once, the engine remembers the mod
(`Documents\My Games\openciv4\mods.txt`), so a save of the mod loads with it, and the menu's
*Load a Mod* switches between mods.

To give the port to players, ship `CvGameCoreDLL-x64.dll` in the mod's `Assets` folder, next to
its 32-bit DLL; players start the engine with `--mod` (the engine's `Play Mod.bat` takes the
mod's folder by drag and drop). Respect the mod's license and its author's wishes about the
sources and the DLL.

## 11. Keeping up with the mod's updates

Keep three branches in the port's repository:

* `base`: the sources the port started from (the unmodified import);
* `x64`: `base` plus the port;
* `mod`: `base` with each newer source drop of the mod copied over it, one commit per drop.

For a new version: commit the drop on `mod`, then merge `x64` into it (or the drop into
`x64`). Git applies the port's changes to the new sources; conflicts are only where the mod
changed the same lines. Files a drop leaves out (some authors ship only changed files) stay
from `base`. Then repeat sections 5 to 8: the exports, the XML, a game, a save.

---

Found something this guide does not cover, or a step that did not work for your mod? Open
an issue: <https://github.com/bssth/openciv4/issues>.
