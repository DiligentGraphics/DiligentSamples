# Browser golden-image tests

These tests run the samples/tutorials from `manifest.json` in headless Chrome.
The list follows the native sample capture suite. Unsupported Web features are
listed with reasons in the manifest; known failures are in `exclude.json` and
can be retried with `--no-exclusions`.

One Chrome process runs the suite. Each sample has its own Wasm executable and
tab, which is closed before the next sample. An abort or timeout fails that case
and the runner continues. Missing modules or reference images fail rather than
silently skipping tests. A run with no successful comparisons also fails.

## Build and run

Build the samples normally with Emscripten. Tests use the same artifacts as the
website, with no separate test build configuration. Web builds export filesystem
access for loading reference PNGs and collecting captures. Test mode is selected
at runtime through the sample arguments. Browser execution does not require
JSPI: capture readiness is polled across animation frames.

```sh
python3 -B DiligentSamples/Tests/browser/run_tests.py \
    --build-dir build/DiligentSamples \
    --reference-directory DiligentTestData/GoldenImages/Chrome \
    --browser /path/to/github-action/common/chrome_swiftshader.sh \
    --output SamplesBrowserResults
```

Python uses only its standard library. Chrome/Chromium is discovered from PATH
unless `--browser` is provided. The shared action wrapper selects SwiftShader.
The default backend is WebGPU; `--mode webgl` is available for investigation.
Use `--filter 'Tutorials/Tutorial02_*'` to select cases. See `--help` for image
dimensions, tolerance, browser arguments, and per-case timeouts.

## Capture and comparison

The native sample code performs GPU readback, image comparison, and PNG encoding;
these are not screenshots of the browser window. Like native capture tests,
animation time is zero and two renders initialize UI layout before capture.
The browser keeps processing callbacks while the readback is pending.

Images are 512 by 512 by default. The native comparison allows a per-channel
difference of at most 2 and no pixels above that threshold. The runner uses
`compare_update` inside the virtual filesystem so it can save the actual PNG
after comparison; it never changes host reference files during comparison.

Reference PNGs belong to DiligentTestData's **master** branch:
`GoldenImages/Chrome/<Tutorials-or-Samples>/<name>/<name>_wgpu.png`.
To create or replace them, explicitly add `--update-golden-images`. Only successful
captures replace references. `metadata_wgpu.json` records browser/adapter details,
dimensions, tolerance, and capture results. Browser version differences are
reported as metadata; pixel comparison determines whether a case passes.

Each case prints its native log and final status. The output directory contains
captured PNGs, reference PNGs for failures, per-case `tests.log`, `chrome.log`, `results.json`, and JUnit
`results.xml`. CI runs Debug and Release on Linux through the
`run-sample-browser-tests` action and uploads these artifacts even on failure.

## Known WebGPU failures

- **Tutorial23_CommandQueues:** attempts synchronous adapter enumeration, which
  is unavailable on Web, and creates an `R16_FLOAT` storage texture that WebGPU
  rejects. Shader conversion also reports inconsistent formats for `g_NormalMapUAV`.
- **Atmosphere:** with the Tint fix, Linux Chrome/SwiftShader still does not
  complete a capture within 300 seconds in Debug or Release. Hardware WebGPU
  captures succeed. Shadows is enabled with the fixed Tint parser.

These failures are excluded explicitly, not counted as passes. Geometry shaders,
tessellation, mesh shaders, ray tracing, and the command-line state packager are
separate unsupported-feature skips in the manifest. Tutorial16 runs its ordinary
resource-binding fallback when bindless resources are unavailable.
