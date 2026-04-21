# NVIDIA ACE — Kairos Sample Project

> **NPC Conversation** extension by UoA eResearch adds Speech-to-Text (STT), Large Language Model
> (LLM) inference, and Text-to-Speech (TTS) so players can hold real-time voice conversations with
> MetaHuman NPCs.

For details on NVIDIA ACE, please visit the main [ACE documentation website](https://docs.nvidia.com/ace/latest/index.html).

The original Kairos Sample Project documentation is available at the [Kairos section here](https://docs.nvidia.com/ace/latest/workflows/kairos/kairos-unreal-sample-project.html).

---

## Table of Contents

1. [Feature Overview](#feature-overview)
2. [Architecture](#architecture)
3. [Plugin: NPCConversation](#plugin-npcconversation)
4. [Quick-Start Setup](#quick-start-setup)
5. [Project Settings Reference](#project-settings-reference)
6. [Blueprint Usage](#blueprint-usage)
7. [Provider Reference](#provider-reference)
   - [LLM Providers](#llm-providers)
   - [TTS Providers](#tts-providers)
   - [STT Providers](#stt-providers)
8. [Full Conversation Pipeline Example](#full-conversation-pipeline-example)
9. [Platform Notes](#platform-notes)
10. [Troubleshooting](#troubleshooting)

---

## Feature Overview

| Capability | Primary provider | Fallback |
|---|---|---|
| **Speech-to-Text (STT)** | Whisper-compatible REST API (OpenAI, local Faster-Whisper, …) | — |
| **LLM inference** | OpenAI-compatible chat completions (Cerebras, OpenAI, Ollama, …) | — |
| **Text-to-Speech (TTS)** | ElevenLabs REST API | Platform system TTS (Windows SAPI, macOS `say`, Linux `espeak-ng`) |

All three features are exposed as **async Blueprint nodes** that fire success/failure delegate pins,
making it straightforward to wire them together inside a Blueprint graph without writing any C++.

---

## Architecture

```
Player speaks
     │
     ▼
[STT] UNPCSTTAsync::AsyncRecordAndTranscribe
     │  records microphone → WAV → Whisper API
     │  OnSuccess → transcribed text
     ▼
[LLM] UNPCLLMAsync::AsyncSendToLLM
     │  chat completions (Cerebras / OpenAI / Ollama / …)
     │  OnSuccess → response text
     ▼
[TTS] UNPCTTSAsync::AsyncSpeakText
     │  ElevenLabs API (or system TTS fallback)
     │  OnSuccess → WavFilePath
     ▼
[ACE] Animate Character From Wav File Async  (NV_ACE_Reference plugin)
     │  Audio2Face-3D drives MetaHuman lip-sync + facial animation
     ▼
NPC speaks with synced facial animation
```

---

## Plugin: NPCConversation

Located at `Plugins/NPCConversation/`.  The plugin is a single Runtime module that adds three async
Blueprint nodes and a `Developer Settings` panel.  It has **no dependency on NV_ACE_Reference** so
it can be used in other Unreal projects as-is.

### Blueprint nodes added

| Node | Category | Description |
|---|---|---|
| `Record and Transcribe (STT Async)` | NPC Conversation \| STT | Capture microphone for N seconds → Whisper API → text |
| `Send Message to LLM (Async)` | NPC Conversation \| LLM | text → chat completions → response text |
| `Speak Text (TTS Async)` | NPC Conversation \| TTS | text → WAV file (ElevenLabs or system fallback) |

---

## Quick-Start Setup

### 1 — Enable the plugin

The plugin is already enabled in `KairosSample.uproject`.  If you copy it to another project add:

```json
{ "Name": "NPCConversation", "Enabled": true }
```

to the `Plugins` array in your `.uproject` file.

### 2 — Obtain API keys

| Provider | URL | Notes |
|---|---|---|
| **Cerebras** (LLM) | <https://cloud.cerebras.ai> | Free tier available. Copy the API key from your dashboard. |
| **OpenAI** (LLM / STT) | <https://platform.openai.com> | Covers both `gpt-*` models and `whisper-1`. |
| **ElevenLabs** (TTS) | <https://elevenlabs.io> | Free tier includes 10 000 characters/month. |

### 3 — Enter API keys in Project Settings

Open **Edit → Project Settings → Plugins → NPC Conversation** and fill in:

- **LLM API Key** — Cerebras or OpenAI key
- **ElevenLabs API Key** — ElevenLabs key
- **Whisper API Key** — OpenAI key (or leave empty for local servers)

> **Security note** — API keys are stored in `Config/DefaultEngine.ini`.  Do **not** commit that
> file to a public repository when it contains real keys.  Consider using environment variable
> substitution or a secrets-management solution for production builds.

### 4 — Allow microphone access

On Windows, ensure the app has **Microphone** permission under
*Settings → Privacy → Microphone*.

On macOS add the `NSMicrophoneUsageDescription` key to `Info.plist`.

### 5 — Connect the Blueprint nodes

See [Blueprint Usage](#blueprint-usage) below.

---

## Project Settings Reference

All settings live under **Edit → Project Settings → Plugins → NPC Conversation**.

### LLM

| Setting | Default | Description |
|---|---|---|
| **LLM Base URL** | `https://api.cerebras.ai/v1` | Base URL of any OpenAI-compatible chat completions API. |
| **LLM API Key** | *(empty)* | Bearer token sent in the `Authorization` header. |
| **LLM Model** | `llama-3.3-70b` | Model name forwarded in the request body. |
| **Default System Prompt** | "You are a helpful NPC …" | Injected as the `system` message before each turn. Override per-call in Blueprints. |
| **Max Tokens** | `200` | Maximum tokens the model may generate per reply. |
| **LLM Timeout (s)** | `30` | HTTP timeout for LLM calls. |

### TTS

| Setting | Default | Description |
|---|---|---|
| **TTS Provider** | `ElevenLabs (with System fallback)` | Use `System TTS only` to bypass ElevenLabs entirely. |
| **ElevenLabs API Key** | *(empty)* | If empty, the plugin skips ElevenLabs and uses system TTS. |
| **ElevenLabs Voice ID** | `21m00Tcm4TlvDq8ikWAM` (Rachel) | Voice ID from the ElevenLabs voice library. |
| **ElevenLabs Model ID** | `eleven_turbo_v2_5` | ElevenLabs model. `eleven_turbo_v2_5` is fast; `eleven_multilingual_v2` supports more languages. |
| **TTS Timeout (s)** | `30` | HTTP timeout for ElevenLabs calls. |

### STT

| Setting | Default | Description |
|---|---|---|
| **STT Provider** | `Whisper API` | Currently the only option; reserved for future expansion. |
| **Whisper API Base URL** | `https://api.openai.com/v1` | Base URL for a Whisper-compatible server. |
| **Whisper API Key** | *(empty)* | Bearer token. Leave empty for local servers that don't require auth. |
| **Whisper Model** | `whisper-1` | Model forwarded in the multipart request. |
| **Default Recording Duration (s)** | `5.0` | How long to record when no duration is passed to the Blueprint node. |
| **STT Timeout (s)** | `60` | HTTP timeout for transcription calls (allow extra time for large audio). |

---

## Blueprint Usage

### STT node — Record and Transcribe

```
[Record and Transcribe (STT Async)]
  RecordingDurationSeconds: 5.0
  ├── OnSuccess → TranscribedText (FString), bSuccess
  └── OnFailure → TranscribedText (""), bSuccess (false)
```

`RecordingDurationSeconds = 0` uses the project-default from settings.

### LLM node — Send Message to LLM

```
[Send Message to LLM (Async)]
  UserMessage: <TranscribedText from STT>
  SystemPromptOverride: ""          ← leave empty to use the project default
  ├── OnSuccess → ResponseText (FString), bSuccess
  └── OnFailure → ResponseText (""), bSuccess (false)
```

### TTS node — Speak Text

```
[Speak Text (TTS Async)]
  Text: <ResponseText from LLM>
  ├── OnSuccess → WavFilePath (FString), bSuccess
  └── OnFailure → WavFilePath (""), bSuccess (false)
```

`WavFilePath` is a temporary file on disk.  Pass it directly to the ACE plugin's
`Animate Character From Wav File Async` node.  You are responsible for deleting the file when
done (use the `File Manager` utility Blueprint or `Delete File` from a helper library).

### ACE integration — drive MetaHuman lip-sync

```
[Animate Character From Wav File Async]  ← NV_ACE_Reference plugin
  WorldContextObject: self
  Character: <your MetaHuman actor reference>
  PathToWav: <WavFilePath from TTS>
  ├── AudioSendCompleted → bSuccess
```

---

## Full Conversation Pipeline Example

Below is the minimal Blueprint graph for a single conversation turn.

```
[Input Action "Talk"]
    │
    ▼
[Record and Transcribe (STT Async)]  Duration=5s
    │ OnSuccess
    ▼
[Send Message to LLM (Async)]  UserMessage=TranscribedText
    │ OnSuccess
    ▼
[Speak Text (TTS Async)]  Text=ResponseText
    │ OnSuccess
    ▼
[Animate Character From Wav File Async]  PathToWav=WavFilePath  Character=MetaHumanRef
    │ AudioSendCompleted
    ▼
[Delete File]  FilePath=WavFilePath   ← clean up temp file
```

For a continuous conversation (the NPC remembers context) you need to maintain a **conversation
history array** and pass it as additional `system`/`assistant` messages.  The `SystemPromptOverride`
parameter can carry a JSON-serialised history string, or you can extend the plugin with a stateful
conversation manager component.

---

## Provider Reference

### LLM Providers

Any OpenAI-compatible `/chat/completions` endpoint works.  Set **LLM Base URL** accordingly.

| Provider | Base URL | Recommended model |
|---|---|---|
| **Cerebras** | `https://api.cerebras.ai/v1` | `llama-3.3-70b` (fast, free tier) |
| **OpenAI** | `https://api.openai.com/v1` | `gpt-4o-mini` |
| **Ollama** (local) | `http://localhost:11434/v1` | any pulled model, e.g. `llama3` |
| **LM Studio** (local) | `http://localhost:1234/v1` | any loaded model |
| **Groq** | `https://api.groq.com/openai/v1` | `llama-3.3-70b-versatile` |

### TTS Providers

#### ElevenLabs (primary)

- **Endpoint**: `https://api.elevenlabs.io/v1/text-to-speech/{voice_id}?output_format=pcm_22050`
- **Auth**: `xi-api-key` header
- **Output**: raw int16 PCM at 22 050 Hz, mono, wrapped in a WAV container by the plugin
- **Voice library**: <https://elevenlabs.io/voice-library>

#### System TTS (fallback)

When ElevenLabs is unavailable (no key, HTTP error), the plugin falls back to the platform's
built-in speech synthesizer.

| Platform | Tool used | Notes |
|---|---|---|
| Windows | PowerShell + `System.Speech.Synthesis` | No extra install needed. ~2–3 s startup latency. |
| macOS | `/usr/bin/say` + `afconvert` | Built-in. Supports many voices via *System Preferences → Accessibility → Spoken Content*. |
| Linux | `espeak-ng` | Install with `sudo apt install espeak-ng`. Uses `-f` (file input) and `-w` (WAV output) flags. |

### STT Providers

#### Whisper API (OpenAI-compatible)

- **Endpoint**: `POST {base_url}/audio/transcriptions` (multipart/form-data)
- **Field `file`**: WAV audio, 16-bit PCM, mono — auto-generated by the plugin from the microphone
- **Field `model`**: as configured in Project Settings
- **Response**: `{ "text": "transcribed text" }`

Compatible servers:

| Option | URL | Notes |
|---|---|---|
| **OpenAI** | `https://api.openai.com/v1` | Hosted, `whisper-1` model |
| **Faster-Whisper server** | `http://localhost:9000` | Local GPU inference, very fast |
| **Whisper.cpp server** | `http://localhost:8080` | CPU-friendly, OpenAI-compatible API |
| **Groq** | `https://api.groq.com/openai/v1` | Free tier, `whisper-large-v3-turbo` |

---

## Platform Notes

- **Windows (Win64)** — Fully tested path.  Microphone capture uses the Windows Audio Session API
  (WASAPI) via UE's `AudioCapture` module.

- **macOS** — Microphone capture works via CoreAudio.  System TTS uses `say`/`afconvert` (built-in).
  Ensure `NSMicrophoneUsageDescription` is set in `Info.plist`.

- **Linux** — Microphone capture works via PulseAudio/ALSA.  System TTS requires `espeak-ng`.

- **Other platforms** (Android, iOS, Console) — LLM and TTS (ElevenLabs) work on any platform that
  has internet access.  Microphone capture (STT) and system TTS fallback depend on platform support.

---

## Troubleshooting

### "Failed to open default capture stream"

- Check microphone permissions (Windows Privacy settings / macOS System Settings).
- Make sure no other application has an exclusive lock on the microphone.
- Try a different microphone device.

### LLM returns HTTP 401 / 403

- Double-check the **LLM API Key** in Project Settings.
- For Cerebras, ensure the key starts with `csk-`.  For OpenAI, it starts with `sk-`.

### ElevenLabs returns HTTP 401

- Verify the **ElevenLabs API Key** in Project Settings.
- Check your ElevenLabs quota at <https://elevenlabs.io/subscription>.

### Whisper returns HTTP 413 (payload too large)

- Reduce **Default Recording Duration** in Project Settings (OpenAI limit is 25 MB / ~16 min).

### System TTS produces no audio on Windows

- Verify PowerShell is available: run `powershell -Command "Write-Host OK"` in a terminal.
- Check that `System.Speech` assembly is present (it ships with .NET Framework ≥ 3.5).

### Temp WAV files accumulate on disk

- The plugin writes temp files to the OS temp directory (`%TEMP%` / `/tmp`).
- Call **Delete File** (or use `IFileManager::Get().Delete()`) on `WavFilePath` after the ACE
  animation node completes.
