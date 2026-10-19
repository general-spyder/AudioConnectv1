# Audio Connect - by MiKeys Dsz Studios

Route any DAW track, bus or master straight to specific outputs of your audio interface
and/or the PC headphone port, at the same time.

## Build (Windows)
1. Install Visual Studio 2022 (C++ workload) and CMake.
2. Download Poppins (Google Fonts, OFL) and copy `Poppins-Regular.ttf` and
   `Poppins-SemiBold.ttf` into `Assets/`.
3. `cmake -B build -G "Visual Studio 17 2022" -A x64`
   `cmake --build build --config Release`
4. Outputs: `build/AudioConnect_artefacts/Release/Standalone/Audio Connect.exe`
   and `.../VST3/Audio Connect.vst3` (copy to `C:\Program Files\Common Files\VST3`).

Or push to GitHub: the included workflow builds both and uploads them as artifacts.

Optional: `-DASIO_SDK_PATH=...` (ASIO drivers), `-DAAX_SDK_PATH=...` (Pro Tools AAX).
On macOS the build also produces an AU for Logic Pro.

## Using it
* Put Audio Connect on a track, bus or master. Name the instance (Loops, Stems...).
* Route A: pick Audio Interface or PC / Headphones, then the output pair
  (Out 1+2, Out 3+4 ...). Pair count comes from the detected interface.
* "Also send to" adds a second destination (e.g. interface AND headphones).
* "Auto-assign free output pair" picks the next pair no other instance uses.
* The Output Map shows which instance feeds which pair across all instances.
* Pass-through is off by default so audio isn't also heard on the DAW's own output.

## Important limits
* Audio Connect opens its own audio device. Most ASIO drivers allow one client, so the
  DAW and Audio Connect cannot both open the same interface through ASIO. Run the DAW on a
  different driver/device (or a driver that supports multiple clients) and let Audio Connect
  own the interface.
* Audio passes through a small drift-compensating buffer (default 20 ms), so it is delayed
  relative to the DAW's own output. Lower the buffer for less latency.
* VST2 is not included: Steinberg no longer licenses the VST2 SDK and JUCE 8 dropped it.
  VST3 is supported by Reaper, FL Studio, Studio One and Cubase. Logic needs the AU build
  (macOS); Pro Tools needs AAX (Avid developer account + signing).
* Hosts that sandbox each plugin in its own process can't share routing between instances.
* Untested code: expect to fix compile errors on first build.
