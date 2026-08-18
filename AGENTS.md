# Repository Guidelines

## Project Structure & Module Organization

MusiCue is a JUCE 8 desktop audio-cue application written in C++17 and built with CMake. Application code lives in `Source/`: `Main.cpp` owns JUCE startup, `MainComponent.*` composes the UI, and focused component pairs such as `CueListComponent.*`, `InspectorComponent.*`, and `ToolbarComponent.*` own their respective surfaces. `AudioEngine.*` contains playback/device work; `WorkspaceFile.*` handles workspace persistence. Shared UI constants and helpers belong in headers such as `Palette.h`, `Icons.h`, and `Cue.h`.

SVG icons are in `Assets/icons/` and registered in `CMakeLists.txt` through `juce_add_binary_data`. The generated `build/` directory contains CMake and fetched JUCE dependencies; never edit or commit its contents.

## Build, Test, and Development Commands

```bash
cmake -B build                 # configure; fetches JUCE 8 on first run
cmake --build build -j8        # compile the app
open build/MusiCue_artefacts/MusiCue.app  # launch macOS build
```

Requirements: CMake 3.22+, a C++17 compiler, and platform toolchain (Xcode on macOS or MSVC 2022 on Windows). No automated test target exists yet. Manually verify cue loading, overlapping playback, GO/Stop/Panic, and workspace save/load after relevant changes.

## Coding Style & Naming Conventions

Follow nearby JUCE C++ style: four-space indentation, braces on a new line for classes/functions, and `#pragma once` in headers. Name classes in `PascalCase`, methods and variables in `camelCase`, and files after their main class (`AudioEngine.cpp`, `AudioEngine.h`). Keep declarations in headers and implementation in matching `.cpp` files. Prefer JUCE types and lifecycle patterns when interacting with framework APIs.

Audio-thread work must remain real-time safe: no allocations, blocking locks shared with the message thread, or direct UI/model access. Preserve existing `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` declarations for JUCE-owned components.

## Change, Commit & Pull Request Guidelines

The repo lives on GitHub (`origin/main`) with a single `main` branch and no established commit convention. Use concise imperative subjects, for example `Add cue fade validation`. The `build/` directory is gitignored and must stay untracked. Keep each change focused. Pull requests should state behavior changed, testing performed, linked issue if applicable, and include screenshots or a short recording for UI changes. Do not commit user workspaces, audio files, local settings, or generated build outputs.

## Configuration & Dependencies

Add new source files and binary assets to `CMakeLists.txt`; otherwise they will not build or be embedded. JUCE is pinned to `8.0.7` via `FetchContent`; update that pin deliberately and validate a clean configure/build.
