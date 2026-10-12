# Undertow

A native C++20 / OpenGL screensaver with a calm abstract composition: broad rounded bars, warm flowing gradients and slow coordinated vertical movement. Columns vary in visible count as bars drift through the screen edges; three remains the hard maximum. No cutout shapes or random fades.

## Run

```sh
undertow
undertow --options
undertow --windowed --options
undertow --screensaver
```

`H` shows settings. Up/down selects a setting; left/right adjusts it. `P` cycles palettes, `M` cycles motion styles, `R` generates a new pattern, `Space` pauses, `F` toggles fullscreen, `Esc` exits. Changes made with the keyboard save to `${XDG_CONFIG_HOME:-~/.config}/undertow/settings`.

Palettes: Sunset, Ember, Orchid, Moss, Silver, Ocean. Motion styles: Drift, Glide, Still. Speed, motion depth, density, brightness and bar length are adjustable. The default bar length is 220 pixels; use `--length 60..300` or the Bar length option. `--help` lists all command-line options. Screensaver mode exits on keyboard, click, scroll or accumulated mouse movement; initial mouse motion is ignored for one second.

This is a visual screensaver, not a session lock. Launch manually or invoke `undertow --screensaver` from your existing idle manager. Installation does not change idle, lock, display or desktop settings. It runs on the current window's display; it does not span all monitors.

## Performance

Release build uses `-O3`, `NDEBUG`, and link-time optimization when supported. All bars render in one GPU instanced draw call, with a buffer that grows to accommodate the multi-row layout. Simulation runs at a fixed 120 Hz with interpolated rendering; time-based transitions are independent of render rate. Slow shared motion keeps neighboring bars related without cutout geometry. Visible counts change only when bars enter or leave the display. Each bar carries its own continuous two-color gradient, and palette changes crossfade. The older experimental settings are preserved as `settings.before-simplify` when migrated to the new defaults.

Default rendering is capped at 120 FPS with swap interval zero. Use `--vsync` to synchronize presentation to the display, or `--benchmark` for uncapped rendering. A 60 Hz display can show only 60 distinct frames each second. The compositor and driver ultimately control presentation, so no application can guarantee a refresh rate higher than the hardware supports.

Rendering targets 120 FPS; the simulation runs at 120 Hz. Printed frame statistics include capture and file-writing overhead when `--capture` is used. Performance depends on hardware and desktop load.

```sh
undertow --benchmark --density 2 --seconds 10
undertow --seconds 10 --seed 42 --capture preview.bmp
```

## Build and verify

Dependencies: CMake, C++20 compiler, SDL2, OpenGL 3.3, FreeType, DejaVu Sans Mono font. On Arch these are provided by `cmake`, `gcc`, `sdl2-compat` (or `sdl2`), `mesa`, `freetype2`, and `ttf-dejavu`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/undertow
```

Motion verification exercises all styles and density limits over 60 simulated seconds, checking reproducibility, finite positions, movement, varying visible counts and a hard maximum of three bars per column. Native rendering and frame pacing are also checked on the AMD GPU.

Source modules: `motion` handles fields and preferences, `renderer` handles instanced GPU bars and capture, `overlay` handles the font atlas and settings display, `main` handles input, fixed updates and pacing.
