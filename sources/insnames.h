//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

enum class Midi_Spec {
    GM, GS, SC88, MT32, XG,
};

const char *midi_spec_name(Midi_Spec spec);

struct Midi_Program_Ex {
    Midi_Program_Ex()
        {}
    Midi_Program_Ex(Midi_Spec spec, const char *name)
        : spec(spec), name(name) {}
    Midi_Spec spec {};
    const char *name = nullptr;
};

extern const char *midi_instrument_name[128];
extern const Midi_Program_Ex midi_percussion[128];

const Midi_Program_Ex *midi_program_ex_find(
    unsigned msb, unsigned lsb, unsigned pgm);
