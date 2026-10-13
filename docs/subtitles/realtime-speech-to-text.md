---
title: Realtime Speech-to-Text
description: "Generate live subtitles for OvenMediaEngine streams with real-time speech-to-text on the CPU."
sidebar_position: 34
---

OvenMediaEngine (OME) version 0.20.0 and later supports real-time automatic subtitles through integration with whisper.cpp. This feature converts live audio streams to text in real time and can optionally translate the recognized speech into English.

Transcription runs on the CPU, so no GPU is required. How many streams a server can transcribe depends on the model you choose and how many CPU cores you can spare — see [Choosing a Model](#choosing-a-model).

![](../images/realtime-speech-to-text.png)

## Prerequisites

### Build and Install whisper.cpp

Install the prerequisites (including `whisper.cpp`) from the OME source root:

```
$ cmake -P cmake/InstallPrerequisites.cmake
```

By default `whisper.cpp` is built for a portable x86-64 baseline (AVX2/FMA/F16C, Haswell and
newer), so the resulting binary runs on any reasonably modern server. If you build OME on the
same machine you run it on, you can trade that portability for speed:

```
$ cmake -DOME_WHISPER_NATIVE=ON -P cmake/InstallPrerequisites.cmake
```

`OME_WHISPER_NATIVE=ON` compiles ggml with `-march=native`, which is noticeably faster on CPUs
with AVX-512 or AMX. The binary then only runs on CPUs that support the same instructions, so do
not use it for packages you distribute to other machines.

On aarch64 the portable build stays on the compiler default (`armv8-a`, NEON only) so that it also
runs on Cortex-A72 class boards such as the Raspberry Pi 4. That costs a lot on ARM servers: on an
8-vCPU Graviton2 (Neoverse-N1) the portable build needs about 2.6 s to transcribe a 10-second
`tiny.en` window with 2 threads, while the `OME_WHISPER_NATIVE=ON` build (which turns on `dotprod`
and FP16 arithmetic) needs 1.1 s. Use `OME_WHISPER_NATIVE=ON` on ARM servers such as Graviton2 and
newer, Ampere Altra or Apple silicon, including when you build your own arm64 Docker image. The
published arm64 image is the portable build.

The option takes effect when whisper.cpp is installed. An installation that already matches the
required version is kept as it is, so to switch an existing machine between the portable and the
native build, rebuild just whisper:

```
$ cmake -DOME_WHISPER_NATIVE=ON -DTARGET=whisper -P cmake/InstallPrerequisites.cmake
```

## Configuration

STT configuration is split across two sections:

* **`<Modules><Whisper>`** in `Server.xml` — preloads model files at startup and caps the total inference thread usage.
* **`<Application><Subtitles>`** — defines subtitle renditions (label, language, etc.) that STT output will be written to.
* **`<Application><OutputProfiles><MediaOptions><STT>`** — connects an input audio track to a subtitle rendition via an STT engine.


:::warning

**Breaking change:** The `<Transcription>` element inside `<Subtitles><Rendition>` has been removed. If your existing configuration uses `<Subtitles><Rendition><Transcription>`, it will no longer work. Please migrate to `<OutputProfiles><MediaOptions><STT><Rendition>` as described below.

:::


### Step 1: Preload Models (Server.xml)

Declare the Whisper model files to load at server startup inside `<Modules><Whisper>`. Multiple `<PreloadModel>` entries are allowed. Models are loaded in descending file-size order.

`<Modules><Whisper>` has the following fields:

| Key | Description |
|---|---|
| PreloadModel | A model to load at startup. Repeatable. |
| PreloadModel > Path | Path to the model file. Can be absolute or relative to the config directory. |
| MaxThreads | CPU threads Whisper may use for inference across every STT track on this server. If omitted or `0`, the number of hardware threads is used. Active tracks share this budget equally, each keeping at least one thread, so with more tracks than threads the total can exceed it. Lower it to reserve cores for transcoding. |

```xml
<Server>
    <Modules>
        <Whisper>
            <!-- Keep 8 of the machine's threads for transcription at most -->
            <MaxThreads>8</MaxThreads>

            <PreloadModel>
                <Path>whisper_model/ggml-small.bin</Path>
            </PreloadModel>
            <PreloadModel>
                <Path>whisper_model/ggml-base.bin</Path>
            </PreloadModel>
        </Whisper>
    </Modules>
</Server>
```

:::info

The `<Devices>` element used to pick a GPU to preload onto. It is still accepted so existing
configurations keep working, but it is ignored and logs a warning.

:::


:::info

`<PreloadModel>` is optional. If omitted, models are loaded on demand when the first stream that uses them is published. Preloading is recommended for production to avoid a delay on the first stream.

:::


### Step 2: Define Subtitle Renditions

Define the subtitle tracks that will receive STT output. For more details on `<Subtitles>`, refer to the [Subtitles](./README.md) section.

```xml
<Application>
    <Subtitles>
        <Enable>true</Enable>
        <DefaultLabel>Korean</DefaultLabel>
        <Rendition>
            <Language>ko</Language>
            <Label>Korean</Label>
            <AutoSelect>true</AutoSelect>
            <Forced>false</Forced>
        </Rendition>
        <Rendition>
            <Language>en</Language>
            <Label>English</Label>
        </Rendition>
    </Subtitles>
</Application>
```

### Step 3: Configure STT in OutputProfiles

Under `<OutputProfiles><MediaOptions><STT>`, add a `<Rendition>` for each audio-to-subtitle mapping. The `<OutputSubtitleLabel>` must match a `<Label>` defined in `<Subtitles>`.

```xml
<Application>
    <OutputProfiles>
        <MediaOptions>
            <STT>
                <!-- Korean STT -->
                <Rendition>
                    <Engine>whisper</Engine>
                    <Model>whisper_model/ggml-base.bin</Model>
                    <InputAudioIndex>0</InputAudioIndex>
                    <OutputSubtitleLabel>Korean</OutputSubtitleLabel>
                    <SourceLanguage>auto</SourceLanguage>
                    <Translation>false</Translation>
                    <!-- Optional: CPU threads for this rendition -->
                    <ThreadCount>4</ThreadCount>
                    <!-- Optional: sliding-window tuning -->
                    <StepMs>2000</StepMs>
                    <LengthMs>10000</LengthMs>
                    <KeepMs>1500</KeepMs>
                </Rendition>
                <!-- English translation of the same audio track -->
                <Rendition>
                    <Engine>whisper</Engine>
                    <Model>whisper_model/ggml-base.bin</Model>
                    <InputAudioIndex>0</InputAudioIndex>
                    <OutputSubtitleLabel>English</OutputSubtitleLabel>
                    <SourceLanguage>auto</SourceLanguage>
                    <Translation>true</Translation>
                </Rendition>
            </STT>
        </MediaOptions>
    </OutputProfiles>
</Application>
```

The `<STT><Rendition>` configuration includes the following options:

<table><thead><tr><th width="192">Key</th><th>Description</th></tr></thead><tbody><tr><td>Engine</td><td>The STT engine to use. Currently, only `whisper` is supported.</td></tr><tr><td>Model</td><td>Path to the whisper.cpp model file. Can be absolute or relative to the configuration directory (where Server.xml is located).</td></tr><tr><td>InputAudioIndex</td><td>Index of the audio track in the input stream to transcribe. Default is `0` (first audio track).</td></tr><tr><td>OutputSubtitleLabel</td><td>Label of the subtitle rendition (defined in `&lt;Subtitles&gt;`) to write the transcription output to.</td></tr><tr><td>SourceLanguage</td><td>Language of the input audio, as an ISO 639-1 code (e.g., `ko`, `en`, `ja`) or a language name (e.g., `korean`). Case and surrounding spaces are ignored. Set to `auto` to enable automatic detection. An unsupported value falls back to `auto` with a warning.</td></tr><tr><td>Translation</td><td>When set to `true`, translates the recognized text into English. Whisper currently supports translation to English only.</td></tr><tr><td>StepMs</td><td>Amount of new audio (in milliseconds) collected before the next subtitle is generated. Default is `2000`. Lower values reduce subtitle latency but need more computation.</td></tr><tr><td>LengthMs</td><td>How long (in milliseconds) the audio window, the audio used to generate each subtitle, keeps growing. The window grows by `StepMs` of new audio each time and then starts over from its last `KeepMs`, so the longest window is about `KeepMs` + `LengthMs` − `StepMs` when `LengthMs` is at least twice `StepMs` (9500 ms with the defaults). The configured range is logged as `window_ms` in `Whisper sliding window configured` when the track starts; actual windows can be longer by up to one input audio frame per step. Default is `10000`. Each subtitle shows all the text recognized in the window, so longer windows also show more text at once. A longer window can also enlarge the encoder context, in steps of about 5 s, which raises accuracy and computation (see [Trading Accuracy for Compute](#trading-accuracy-for-compute)). If the longest window exceeds 29800 ms, the STT track is disabled.</td></tr><tr><td>KeepMs</td><td>Amount of audio (in milliseconds) from the end of the audio window that is kept when the window starts over (see `LengthMs`). Default is `1500`. Helps avoid cut-off words at window boundaries. A longer `KeepMs` also makes every window longer, so like `LengthMs` it can raise computation.</td></tr><tr><td>ThreadCount</td><td>Upper bound on the CPU threads this rendition uses for inference. If omitted or `0`, a default derived from the number of hardware threads is used. The rendition actually gets this value or its equal share of `&lt;Modules&gt;&lt;Whisper&gt;&lt;MaxThreads&gt;` among the active STT tracks, whichever is smaller.</td></tr><tr><td>Modules</td><td>Deprecated. It used to select a GPU (e.g. `nv:0`). Whisper runs on the CPU, so the value is accepted for compatibility, ignored, and logged as a warning.</td></tr></tbody></table>

### Model

Model files can be downloaded from [https://huggingface.co/ggerganov/whisper.cpp](https://huggingface.co/ggerganov/whisper.cpp). For example:

```
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin
$ wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin
```

The `.en` variants are English-only and run faster than the multilingual model of the same size.
Use a multilingual model (`ggml-small.bin` and friends) only when the audio is not English or when
`<SourceLanguage>auto</SourceLanguage>` has to detect it.

### Choosing a Model

Inference has to finish within `<StepMs>` or subtitles fall behind the live audio. Larger models
are more accurate but need more CPU, so the model and the thread count have to be picked together.

| Model | Size | Threads per stream | Notes |
|---|---|---|---|
| tiny / tiny.en | 75 MB | 2 | Lowest accuracy. Many concurrent streams per server. |
| base / base.en | 142 MB | 2–4 | Good default for a busy server. |
| small / small.en | 466 MB | 4–8 | Recommended when accuracy matters. Limit concurrency by core count. |
| medium / medium.en | 1.5 GB | — | Not recommended; it cannot keep up with live audio. |

The numbers above target roughly twice real-time speed, which leaves headroom for the transcoding
that shares the same CPU. Multilingual models are somewhat slower than their `.en` counterparts,
and a server CPU with AVX-512 or AMX is faster than the baseline build.

On ARM the table assumes the `OME_WHISPER_NATIVE=ON` build. Measured on an 8-vCPU Graviton2 with
two STT tracks sharing the machine, the native build keeps up with `base.en` at 4 threads per
track (1.1 s per 10-second window) but not with `small.en` (2.9 s), which needs the whole machine
for a single track. The portable build does not keep up with `tiny.en` at 2 threads (2.6 s) or
`base.en` at 4 threads (2.9 s); `small.en` takes 9 s per window and its warm-up alone delays the
server start by about a minute.

Set `<Modules><Whisper><MaxThreads>` to the total you are willing to spend on transcription. Active
STT tracks share that budget equally and the share is recomputed as tracks start and stop, so a
track never keeps threads another one needs. Each track always gets at least one thread; a track
that gets less than its `<ThreadCount>` logs a warning.

If a model turns out to be too large for the machine, OME logs:

```
Whisper inference is slower than real time (2480 ms for a 2000 ms step) and subtitles will fall
behind. Use a smaller model, raise <ThreadCount>, use a shorter <LengthMs> or <KeepMs>, or a
longer <StepMs>, or reduce the number of concurrent STT tracks.
```

Memory is checked before each model and each per-stream state is allocated — inside a container,
against the cgroup limit rather than the host total. A model needs roughly twice its file size in RAM.

### Trading Accuracy for Compute

The model has the largest effect on accuracy and cost, so choose it first (see
[Choosing a Model](#choosing-a-model)). For a given model, the window settings move accuracy and
CPU together, and `<ThreadCount>` keeps the track in real time.

| Accuracy | CPU | `StepMs` / `LengthMs` / `KeepMs` | Latency |
|---|---|---|---|
| Slightly lower | About ¾ | 3000 / 10000 / 1500 | Higher: subtitles update every 3 s |
| Good | Base | 2000 / 10000 / 1500 (default) | Low |
| Higher | About 1.6× | 2000 / 12000 / 1500 | Low |

- **Moving down the table** raises accuracy and CPU together, and each subtitle shows more text
  (the longest window grows from 9.5 s to 11.5 s). If OME then logs
  `Whisper inference is slower than real time`, raise `<ThreadCount>`, use a smaller model, or go
  back up the table.
- **A `<LengthMs>` shorter than the default** does not save CPU, because the encoder context does
  not go below 10.24 s, but it lowers accuracy.

The encoder context in use is logged as `audio_ctx` (positions of 20 ms) in
`Whisper sliding window configured` when the track starts.

## Runtime Control via REST API

STT can be paused and resumed at runtime without restarting the server or recreating the stream. This is useful for temporarily disabling transcription for a specific stream (e.g., during ad breaks or when the stream is not speech-heavy) to save CPU resources.

For full API reference including request/response details and error codes, see [STT Control](../rest-api/v1/virtualhost/application/stream/stt-control.md).

| Endpoint | Description |
|---|---|
| `POST :enableStt` | Resume STT inference for the stream |
| `POST :disableStt` | Pause STT inference, dropping audio frames without running inference |
| `POST :sttStatus` | Get current enabled state and per-rendition configuration |

### Disabling STT at Startup

STT can be started in the disabled (paused) state by setting `<Enable>false</Enable>` inside the `<STT>` block. The shared model is still loaded (or reused if another stream already loaded it), but no inference runs and no per-stream inference state or threads are held until the stream receives an `:enableStt` call.

```xml
<STT>
    <Enable>false</Enable>
    <Rendition>
        ...
    </Rendition>
</STT>
```

