//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define EACH_PLAYER_TYPE(F, ...)                \
    F(OPL3, ##__VA_ARGS__)                      \
    F(OPN2, ##__VA_ARGS__)

enum class Player_Type {
    #define ENUMVAL(x) x,
    EACH_PLAYER_TYPE(ENUMVAL)
    #undef ENUMVAL
};

static constexpr Player_Type all_player_types[] {
    #define ARRAYVAL(x) Player_Type::x,
    EACH_PLAYER_TYPE(ARRAYVAL)
    #undef ARRAYVAL
};

template <Player_Type>
struct Player_Traits;

#include <adlmidi.h>

template <>
struct Player_Traits<Player_Type::OPL3>
{
    typedef ADL_MIDIPlayer player;
    typedef ADLMIDI_AudioFormat audio_format;
    typedef ADLMIDI_SampleType sample_type;

    static const char *name() { return "ADLMIDI"; }
    static constexpr auto &version = adl_linkedLibraryVersion;
    static constexpr auto &init = adl_init;
    static constexpr auto &close = adl_close;
    static constexpr auto &reset = adl_reset;
    static constexpr auto &panic = adl_panic;
    static constexpr auto &emulator_name = adl_chipEmulatorName;
    static constexpr auto &switch_emulator = adl_switchEmulator;
    static constexpr auto &get_num_chips = adl_getNumChips;
    static constexpr auto &set_num_chips = adl_setNumChips;
    static constexpr auto &open_bank_file = adl_openBankFile;
    static constexpr auto &generate = adl_generate;
    static constexpr auto &generate_format = adl_generateFormat;
    static constexpr auto &rt_note_on = adl_rt_noteOn;
    static constexpr auto &rt_note_off = adl_rt_noteOff;
    static constexpr auto &rt_note_aftertouch = adl_rt_noteAfterTouch;
    static constexpr auto &rt_channel_aftertouch = adl_rt_channelAfterTouch;
    static constexpr auto &rt_controller_change = adl_rt_controllerChange;
    static constexpr auto &rt_program_change = adl_rt_patchChange;
    static constexpr auto &rt_pitchbend_ml = adl_rt_pitchBendML;
};

#include <opnmidi.h>

template <>
struct Player_Traits<Player_Type::OPN2>
{
    typedef OPN2_MIDIPlayer player;
    typedef OPNMIDI_AudioFormat audio_format;
    typedef OPNMIDI_SampleType sample_type;

    static const char *name() { return "OPNMIDI"; }
    static constexpr auto &version = opn2_linkedLibraryVersion;
    static constexpr auto &init = opn2_init;
    static constexpr auto &close = opn2_close;
    static constexpr auto &reset = opn2_reset;
    static constexpr auto &panic = opn2_panic;
    static constexpr auto &emulator_name = opn2_chipEmulatorName;
    static constexpr auto &switch_emulator = opn2_switchEmulator;
    static constexpr auto &get_num_chips = opn2_getNumChips;
    static constexpr auto &set_num_chips = opn2_setNumChips;
    static constexpr auto &open_bank_file = opn2_openBankFile;
    static constexpr auto &generate = opn2_generate;
    static constexpr auto &generate_format = opn2_generateFormat;
    static constexpr auto &rt_note_on = opn2_rt_noteOn;
    static constexpr auto &rt_note_off = opn2_rt_noteOff;
    static constexpr auto &rt_note_aftertouch = opn2_rt_noteAfterTouch;
    static constexpr auto &rt_channel_aftertouch = opn2_rt_channelAfterTouch;
    static constexpr auto &rt_controller_change = opn2_rt_controllerChange;
    static constexpr auto &rt_program_change = opn2_rt_patchChange;
    static constexpr auto &rt_pitchbend_ml = opn2_rt_pitchBendML;
};
