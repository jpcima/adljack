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
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
#    include "te_virtual_midi.h"
#endif

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
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
    VM_MIDI_PORT *vmidi_port = nullptr;
    bool have_virtualmidi = false;
#endif
};
