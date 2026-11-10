# Porting with an AI assistant

Much of a port is mechanical (sections 3 to 5 of [PORTING.md](PORTING.md)), which suits a
coding agent: Claude Code, Codex, Cursor's agent or similar, running on your machine with
access to the sources, the compiler and the engine. This page gives a prompt for it and the
checks to make on what it produces. An agent is fast at the compile errors and good at
searching for the 64-bit patterns; it is also capable of "fixing" a crash by hiding it, so the
checks below are not optional.

Results, problems and improvements to this prompt: <https://github.com/bssth/openciv4/issues>.

## Before you start

* Do sections 1 and 2 of the guide yourself, or check that the agent did: the sources in git
  with an **unmodified first commit**, this repository next to them with submodules initialised recursively, Python 2.7
  in place, the engine folder and the game installed.
* Give the agent the paths. Tell it what it may run (the build, the engine) and what it must
  not touch (the game install, the original mod folder).
* Keep a copy of the mod's 32-bit DLL and some original saves for the comparisons.

## The prompt

Fill in the angle brackets and give it to the agent as its task.

```text
Port the Civilization IV: Beyond the Sword mod DLL in <path to the mod's CvGameCoreDLL sources>
to x64 so that it runs in the openciv4 engine.

Context:
- The sources are a git repository; its first commit is the unmodified original. Make every
  change in commits after it. Mark each changed line with a short comment containing
  "openciv4". Do not reformat, reorder or clean up code you do not have to change.
- openciv4-dll is at <path to openciv4-dll>. Read README.md, docs/PORTING.md,
  docs/ENGINE-CONTRACT.md and docs/TROUBLESHOOTING.md first and follow PORTING.md's steps
  in order. docs/SDK-PORT.md and `git log -p CvGameCoreDLL` there show how the vanilla SDK
  was ported: the same kinds of changes apply to this mod.
- Build flags: the mod's Makefile / project file, configuration <Final_Release or the one it
  ships>. Pass its /D defines as OC4_MOD_DEFINITIONS; exclude the .cpp files its project does
  not build with OC4_MOD_EXCLUDE. If the mod has a .def file, write CvGameCoreDLL64.def in x64
  mangling (PORTING.md section 5) and pass it as OC4_MOD_DEF_FILE.
- Build from an x64 Visual Studio developer prompt:
  cmake -S <openciv4-dll> -B <build dir> -G Ninja -DCMAKE_BUILD_TYPE=Release -DOC4_LTO=OFF
    -DOC4_MOD_SOURCE_DIR=<sources> "-DOC4_MOD_DEFINITIONS=<defines>" [-DOC4_MOD_DEF_FILE=<def>]
  cmake --build <build dir> --target CvGameCoreDLL_mod -- -k 0
- The engine: <engine folder>\oc4core.exe. The mod's content is at <mod folder>; put the built
  <build dir>\mod\CvGameCoreDLL-x64.dll (and its .pdb) into <mod folder>\Assets and run the
  engine with --mod "<mod folder>".
- The game install at <Civ4 folder> is read-only for you. Do not modify the engine or
  openciv4-dll to make the mod work; if something there seems wrong, stop and report it.

Rules:
- Never change game rules or AI behaviour. A port changes how the code compiles and runs on
  x64, not what it computes. Where the original only worked by luck (a write past an array,
  a read of freed memory), make it do what it evidently meant and note it in the commit message.
- Never silence a problem: no catch-all exception handlers, no removed asserts, no skipped
  XML entries, no disabled features to get past a crash. Find the cause.
- Keep the save format: anything written to saves keeps the size it had in the 32-bit
  build (size_t as 4 bytes, PORTING.md section 4).
- Do not replace code that uses the game's random numbers with code that calls the random
  generator a different number of times or in a different order.
- Commit after each step that builds and passes its check, with a message saying what and
  why.

Steps, each with its check:
1. Compile errors (PORTING.md section 3) until the DLL builds. Check: it links.
2. 64-bit review (section 4): search for pointer-to-int casts, int/long/DWORD holding pointers,
   size_t/time_t/pointers written to saves, sizeof-based layout code, inline asm, memcpy over
   object ranges. List what you found and changed. Check: the list, with file and line.
3. Exports (section 5): run
   <openciv4-dll>\tools\check-exports.ps1 -Exe <engine folder>\oc4core.exe -Dll <the DLL>.
   Check: 0 missing.
4. XML (section 6): run oc4core.exe --mod "<mod folder>". Check: every stage ok, no
   message boxes in the output (or each explained).
5. Games (section 7): oc4core.exe --mod "<mod folder>" --map Continents --size SMALL --seed 3
   --turns 50, twice. Check: identical checksums every turn in both runs; no crash. Then other
   seeds and sizes, and 200+ turns on a Standard map.
6. Saves (section 8): save at turn 25 of a 50-turn game, load and play the rest. Check: the
   load succeeds; report whether the checksums after loading match the uninterrupted game.
   Load <original saves> made with the mod's 32-bit DLL. Check: they load and play 10 turns.
7. Report: what you changed and why (by category), what you could not resolve, every place
   where you changed behaviour that the original relied on, and the checks' results with the
   commands you ran.
```

## Checking the agent's work

Read the diff (`git diff <first commit>`), not only the summary. Look for:

* **Changes that are not porting.** Reformatted functions, renamed variables, "improved"
  logic, removed code. Ask for them to be reverted.
* **Hidden failures.** `try`/`catch (...)`, `if (!p) return;` added where a pointer "should
  not" be null, asserts removed, loops that skip entries. Each needs a reason: what was null,
  why, and what the original did there.
* **Random numbers.** Any change near `getSorenRandNum` / `getMapRandNum` or in code that
  calls them: same calls, same order, same ranges?
* **Saves.** Any change to a `read` or `write` function, or to a type that is written.

Then run the checks yourself: the same seed twice (identical checksums), a save and load, an
original save. If you can, run the AddressSanitizer build (section 9): an agent's port often
compiles and plays while still writing past an array.

When the port works, consider sharing what the agent got wrong or what the prompt missed, in
an issue in the main repository: <https://github.com/bssth/openciv4/issues>.
