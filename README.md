# BEATREEL

**Turn any beat into a reel, right inside your DAW.**

BEATREEL sits on your master track and records your beat bit-for-bit. It turns the recording into a ready-to-post video: animated cards with the waveform, the bass, the beat and the MIDI, in 18 looks you can make your own.

- **One click REC**: records one loop of your DAW and stops by itself at the loop end. It also captures your DAW's export (offline render).
- **Load your .mid** for the piano-roll card. Add your own fonts (.ttf / .otf).
- **9:16, 4:5, 1:1 and 16:9** videos (1080p) with the original audio.
- **18 looks**, your own colours, backgrounds (solid, gradient, mesh, dots, grid, stripes, patterns, your own image) and presets.
- **RANDOMIZE** gives you a new look and card mix in one click.
- **14 intro animations**: SLAM, TYPEWRITER, GLITCH, COUNTDOWN, CARD DEAL and IRIS, plus simple ones (FADE IN, FADE FROM WHITE, ZOOM IN, ZOOM OUT, SLIDE UP, WIPE, SPIN). Preview one any time, and set its length and hand-over by dragging on a timeline (with optional snap to beats). It can play over the start of your beat, or before it as a silent lead-in.
- **Works offline** once activated (signed license keys, verified locally).
- Windows 10/11 (VST3), macOS 11+ Apple silicon & Intel (VST3 + AU), plus a standalone app.

## The 18 cards

| Card | Shows |
|---|---|
| BEAT | Scrolling 2-bar view: master, low, mid and high lanes, plus a hits lane with kicks highlighted. Bar numbers come from your song. |
| MIDI | Piano roll from your .mid. Notes light up as they play, with velocity lollipops and page counter. Without a .mid it shows an audio-derived key grid. |
| WAVE | Whole-loop waveform with playhead. |
| SCOPE | Triggered oscilloscope (raw signal plus smoothed low end). |
| BASS | Sub waveform with the detected 808/bass note (e.g. `D2`). |
| LEVELS | L/R meters, short-term LUFS and history, integrated LUFS. |
| VECTOR | Stereo goniometer. |
| RADIAL | Circular spectrum with a kick-pulsing centre. |
| SPECTRUM | 32-band analyser. |
| ORBIT | Planets orbiting in time with bars; the star pulses on kicks. |
| TRACK | Rhythm-game lane: your hits fall onto the line, with score and combo. |
| DOTS | LED dot-matrix spectrum. |
| COVER | Big title card with BPM and key. |
| PIANO | Keyboard lighting the notes that are playing (from your .mid, or detected from the audio). |
| TEMPO | Big BPM, bar.beat counter, beat squares and a swinging pendulum. |
| VINYL | Spinning record with your title on the label, bumping on kicks. |
| SPECTROGRAM | Scrolling heat map of the last 4 seconds of frequencies. |
| STEPS | 16-step grid of the current bar: kicks, hits, highs and lows light up as they play. |

There's also a header (title, BPM, detected key, subtitle), a key strip that lights the current note, a progress bar and a footer. Everything reacts to your kicks, and the amount is adjustable.

## Project layout

```
Source/
  PluginProcessor.*   capture (lock-free, allocation-free in realtime), loop detection, preview playback, state
  PluginEditor.*      the UI: REC, preview, LOOKS / CARDS / STYLE / INFO tabs, presets, export, activation
  Analysis.*          bands, onsets, LUFS (BS.1770), spectrum, chroma, key, bass pitch, MIDI loading
  Renderer.*          draws one video frame (cards, backgrounds, layouts for every aspect ratio)
  Style.*             the 12 looks, randomize, fonts, presets (JSON)
  Exporter.*          parallel frame rendering + ffmpeg encode with the original audio
  License.*           offline RSA-signed license keys
  Demo.*              synthesised demo beat so the looks can be browsed before the first capture
  PublicKey.h         public half of your license key pair (generated)
Resources/fonts/      10 OFL fonts embedded in the plugin
tools/render/         brl_render: make a reel from a WAV (+ .mid) on the command line
tools/keygen/         brl_keygen: create your key pair, sign customer keys  (never ship)
tools/captest/        brl_captest: automated bit-exact capture test
packaging/            installers, quick install guides, EULA template, third-party notices
.github/workflows/    builds the Windows and macOS packages in the cloud
```

## Build it

You need CMake 3.22+ and a C++17 compiler. JUCE 8 is fetched automatically, or you can drop a JUCE checkout into `./JUCE`.

**Easiest: let GitHub build both platforms.** Push this folder to a GitHub repo, open **Actions → Build BEATREEL → Run workflow**, and download `BEATREEL-Windows` and `BEATREEL-macOS` from the run. Each one is a ready-to-ship folder with the plugin, ffmpeg, the installer, the quick install guide and the license.

**Windows (Visual Studio 2022):**
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target BeatReel_VST3 BeatReel_Standalone
```
**macOS (Xcode):**
```
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target BeatReel_VST3 BeatReel_AU BeatReel_Standalone
```
**Linux** (development/testing): `cmake -B build && cmake --build build`. You'll need the JUCE Linux deps: ALSA, X11, freetype and fontconfig dev packages.

Output goes to `build/BeatReel_artefacts/Release/{VST3,AU,Standalone}`.

## How the capture works

- REC arms the plugin. Recording starts on the first audio block where the DAW reports *playing*, and stores the tempo, time signature and song position.
- It stops at whichever comes first:
  - the exact loop-end sample, when the DAW reports loop points
  - the DAW jumping back to the loop start
  - transport stop
  - REC pressed again
  - the bar limit chosen in CARDS
  - two seconds of silence from the host after an offline render
- Audio goes into pre-allocated 32-bit float chunks with no locks or allocation on the audio thread. The plugin passes audio through untouched.
- The capture is saved as a 32-bit float WAV in the BEATREEL data folder, so reopening your project brings it back.

`brl_captest` feeds random audio through the processor with a fake DAW. It checks three cases: loop points reported, loop wrap without loop points, and an offline render ending with a transport stop. In each case the capture must be bit-identical to the input, have the exact expected length, and match the WAV on disk. All three pass.

## License keys (offline activation)

Keys are RSA-signed (1536-bit): the plugin only contains the **public** key, so nobody can forge keys from it.

```
brl_keygen genkeys seller_keys           # once: writes private.key, public.key, PublicKey.h
cp seller_keys/PublicKey.h Source/       # then rebuild
brl_keygen sign seller_keys/private.key customer@mail.com ORDER-1234
```
Send the printed `BRL-...` key to the customer. Hook `sign` into your store (Gumroad/Lemon Squeezy/Shopify webhooks) to automate it.

**Keep `private.key` secret and out of git** (`.gitignore` already excludes `seller_keys/`).

**"Up to 3 computers"** is a license term, not something the plugin enforces. Fully offline activation can't count machines, so a key works on any machine it's pasted into. If you want hard enforcement, add a small activation server that records machine IDs per key. JUCE's `juce_product_unlocking` module (`OnlineUnlockStatus`) is built for exactly this. Then sign a machine-bound key on first activation, which keeps the "works offline once activated" promise.

## ffmpeg

The MP4 is encoded by ffmpeg, run as a separate program. BEATREEL looks for it in this order:

1. the path set under INFO → LOCATE FFMPEG
2. the BEATREEL data folder
3. next to or inside the plugin bundle
4. Homebrew
5. `PATH`

The installers put it in the data folder. BEATREEL uses `libx264` when available, otherwise hardware H.264 (VideoToolbox / Media Foundation / NVENC / QSV / AMF). The bundled builds are GPL. Shipping them as a separate executable is common practice, but you should keep the notice in `THIRD_PARTY_NOTICES.txt` and link to their source.

## Selling it: checklist

- **JUCE licence**: closed-source sales are covered by JUCE's free *Starter* licence while your JUCE-related revenue stays under $20k per 12 months. Above that you need *Indie* ($40/month) or *Pro*. Check juce.com/legal for the current terms.
- **macOS signing**: the CI uses an ad-hoc signature (it loads, but Gatekeeper will warn on download). For smooth installs, join the Apple Developer Program, then sign with your *Developer ID Application* certificate and notarize (`xcrun notarytool`). Add those as CI secrets.
- **Windows signing** is optional, but an OV/EV code-signing certificate stops SmartScreen warnings.
- **EULA**: fill in `packaging/LICENSE.txt`.
- Replace the `example.com` website and the `BeatReel` company name in `CMakeLists.txt`. Keep the plugin codes (`Btrl`/`Brl1`) unchanged after release, or DAWs will treat it as a new plugin.

## Command-line renderer

```
brl_render --demo reel.mp4 --look 3 --aspect 9:16
brl_render mybeat.wav reel.mp4 --mid melody.mid --bpm 150 --title "HEAVEN" --random 42
brl_render mybeat.wav --still frame.png 12.5 --look 11 --aspect 16:9
brl_render --demo reel.mp4 --intro 1 --intro-len 2.4 --intro-fade 0.5 --intro-before   # 1-6: SLAM .. IRIS, 7-13: FADE IN .. SPIN
```
