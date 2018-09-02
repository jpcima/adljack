//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <RtAudio.h>
#include <RtMidi.h>
#include <ring_buffer/ring_buffer.h>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <string.h>
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
#    include "te_virtual_midi.h"
#endif
namespace stc = std::chrono;

#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
struct VM_MIDI_PORT_Deleter {
    void operator()(VM_MIDI_PORT *x) { virtualMIDI::Shutdown(x); }
};
typedef std::unique_ptr<VM_MIDI_PORT, VM_MIDI_PORT_Deleter> VM_MIDI_PORT_u;
#endif

struct Audio_Context
{
    Ring_Buffer *midi_rb = nullptr;
    RtAudio *audio_client = nullptr;
    RtMidiIn *midi_client = nullptr;
    unsigned sample_rate = 0;
    double midi_delta = 0;
    bool midi_stream_started = false;
    double midi_timestamp_accum = 0;  // timestamp accumulation of skipped events
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
    VM_MIDI_PORT *vmidi_port = nullptr;
    bool have_virtualmidi = false;
    bool vmidi_have_last_event = false;
    stc::steady_clock::time_point vmidi_last_event_time;
#endif
};

static const char *audio_api_ids[] = {
    "unspecified",
    "alsa",
    "pulse",
    "oss",
    "jack",
    "core",
    "wasapi",
    "asio",
    "ds",
    "dummy"
};

static constexpr size_t audio_api_count =
    sizeof(audio_api_ids) / sizeof(audio_api_ids[0]);

static const char *midi_api_ids[] = {
    "unspecified",
    "core",
    "alsa",
    "jack",
    "mm",
    "dummy"
};

static constexpr size_t midi_api_count =
    sizeof(midi_api_ids) / sizeof(midi_api_ids[0]);

inline const char *audio_api_id(RtAudio::Api api)
{
    return (api < audio_api_count) ? audio_api_ids[api] : nullptr;
}

inline const char *midi_api_id(RtMidi::Api api)
{
    return (api < midi_api_count) ? midi_api_ids[api] : nullptr;
}

inline RtAudio::Api find_audio_api(const char *id)
{
    for (size_t i = 0; i < audio_api_count; ++i)
        if (!strcmp(id, audio_api_ids[i]))
            return (RtAudio::Api)i;
    return RtAudio::UNSPECIFIED;
}

inline RtMidi::Api find_midi_api(const char *id)
{
    for (size_t i = 0; i < midi_api_count; ++i)
        if (!strcmp(id, midi_api_ids[i]))
            return (RtMidi::Api)i;
    return RtMidi::UNSPECIFIED;
}

inline bool is_compiled_audio_api(RtAudio::Api api)
{
    std::vector<RtAudio::Api> apis;
    RtAudio::getCompiledApi(apis);
    for (RtAudio::Api current : apis)
        if (api == current)
            return true;
    return false;
}

inline bool is_compiled_midi_api(RtMidi::Api api)
{
    std::vector<RtMidi::Api> apis;
    RtMidi::getCompiledApi(apis);
    for (RtMidi::Api current : apis)
        if (api == current)
            return true;
    return false;
}
