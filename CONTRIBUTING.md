# Contributing to MyKam

Thanks for helping. MyKam is a small plugin, so the rules are short.

## Build

You need Windows 10 or 11 and Visual Studio 2022 or the Build Tools with the "Desktop development with C++" workload.

```
msbuild MyKam.sln /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nologo /v:m
build\tests\Release\mykam_tests.exe
```

(Run from a "x64 Native Tools Command Prompt for VS 2022", or call `MSBuild.exe` by its full path.)

## Rules

- All unit tests must pass and the build must be warning-free: warnings are errors (level 4).
- C++17.
- Pure logic goes in `src/core` and has unit tests in `tests/`. The core never calls X-Plane; it talks to the sim through `ISim`, which the tests fake.
- Keep the XPLM layer in `src/plugin` thin: it forwards to the core and makes no decisions.
- A new feature or a bug fix comes with a test when the logic is in the core.
- Match the style of the surrounding code. There is no formatter configured.
- Every command is named `mykam/...` (the list is in `src/core/command_names.h` and a test checks it).
- Do not add third-party code without its licence: add it under `third_party/`, to `third_party/VERSIONS.md` and to `THIRD_PARTY_NOTICES.md`.
- Do not copy code from other camera plugins.

## Pull requests

Describe what changed and how you checked it in X-Plane (the in-sim steps are in `docs/TESTING-IN-XPLANE.md`).
Please keep one change per pull request.

## Reporting problems

Use the issue templates. Include the `[MyKam]` lines from `Log.txt`.
