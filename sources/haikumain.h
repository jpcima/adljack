//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <media/MediaDefs.h>
#include <media/SoundPlayer.h>
#include <midi2/MidiConsumer.h>
#include <support/ByteOrder.h>
#include <ring_buffer/ring_buffer.h>
#include <chrono>
#include <thread>
namespace stc = std::chrono;

struct Audio_Context
{
    Ring_Buffer *midi_rb = nullptr;
    double midi_delta = 0;
    bool midi_stream_started = false;
    double midi_timestamp_accum = 0;  // timestamp accumulation of skipped events
};
