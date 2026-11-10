# Troubleshooting

Symptoms seen while porting DLLs, and where they usually come from. Sections refer to
[PORTING.md](PORTING.md). Not listed here? Open an issue in the main repository with the
engine's log and what you ran: <https://github.com/bssth/openciv4/issues>.

## Building

| Symptom | Usual cause |
|---|---|
| `boost/python.hpp` errors about templates deep in Boost | the mod's own Boost copy is on the include path ahead of `compat/`: remove it |
| `C2664` converting a string literal to `char*` / `TCHAR*` | make read-only inputs const in declarations, definitions and overrides; keep output buffers mutable (section 3) |
| `C2445` on a `CvString` / `CvWString` conditional | ambiguous string/pointer conversions: use compatible branch types, e.g. `.c_str()` when the result is copied into a string (section 3) |
| invalid token pasting with `/Zc:preprocessor` | `##` must produce one valid token: change `CvArtInfo##name##*` to `CvArtInfo##name *`, likewise for `&` (section 3) |
| `'_Alloc': is not a member` | old standard library internals; use the plain type (section 3) |
| `use of undefined type 'CyUnit'` in a `Cy*Interface.cpp` | pybind11 needs the class's header included there (section 3) |
| `cannot convert ... CvEventTriggerInfo::*` in a binding | the binding names a member of another class: a bug in the original, fix it (section 3) |
| `'_asm': not supported on x64` | inline assembly: intrinsics (`__debugbreak`, `_ReturnAddress`) (section 3) |
| `'random_shuffle': is not a member of 'std'` | removed in C++17: the same algorithm on the same random source (section 3) |
| `ambiguous` `operator==` on the mod's iterators | C++20 reversed candidates: make the operator `const`, both operands the same type |
| conflicts in `stdint.h` | a bundled 2003 copy: delete it |
| `LNK2001` for a `Cy*` or Boost symbol | a part of Boost.Python or Boost the compat layer lacks: add it to `compat/` (and report it) |
| link errors about a `.def` entry | a `.def` in x86 mangling, or a target name that does not exist in x64 mangling (section 5) |

## Starting

| Symptom | Usual cause |
|---|---|
| "does not export what the engine uses" | the DLL lacks names the engine imports: `tools/check-exports.ps1` (section 5) |
| "cannot load the game core ... (error 193)" | a 32-bit DLL named `CvGameCoreDLL-x64.dll`: use your x64 build |
| XML errors at an early stage with a mod that has its own DLL | no `CvGameCoreDLL-x64.dll` in the mod's `Assets`: the vanilla DLL reads the mod's XML |
| `FAILED` at an early XML stage | the DLL does not find its files: the `--mod` path, or the mod expects to be in `Mods\` under its exact folder name |
| "more siblings than memory allocated", types not found | the XML belongs to another version of the mod than the sources |
| a crash in an info class's `read` | an XML list longer than the fixed array it is read into (section 6) |
| a message box text in the log, then the engine goes on | the DLL's asserts and errors: they were dialogs in the original; read them |

## Playing

| Symptom | Usual cause |
|---|---|
| two runs of the same seed give different checksums | undefined behaviour: uninitialised memory, a pointer used as a number, argument evaluation order (sections 4, 9) |
| headless and window runs differ | state the DLL changes when the interface asks it things: an accessor that writes, a cache filled differently |
| crashes after many turns, in different places | heap corruption from an earlier write past an array: AddressSanitizer (section 9) |
| crashes only above some memory use | a pointer truncated to 32 bits (section 4) |
| the map does not show a city's new buildings | `setLayoutDirty(true)` missing where the mod changes what a city shows |
| texts with wrong characters | the mod converts through the ANSI code page; the engine answers 1252, check the mod's own conversion tables |

## Saves

| Symptom | Usual cause |
|---|---|
| "game data does not match this game core" | the load read less or more than the save holds: `--probe-save` shows which part (section 8) |
| the port's saves load, the original's do not | a size that changed on x64 is written: `size_t`, `time_t`, a struct with pointers (section 4) |
| saves of an older build of the port do not load | the mod versions its saves; or a debug and a release build write different data |
| a loaded game plays differently from the uninterrupted one | caches dropped or rebuilt on load. The vanilla core continues exactly; a mod may not, which is not a port bug if the original DLL did the same |
| "belongs to mod ..." when loading | a save of another mod: the engine starts the right one if it has run that mod from the menu before; otherwise start it with `--mod` |
