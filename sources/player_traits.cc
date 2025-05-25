//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "player_traits.h"
#include <stdint.h>
#include <math.h>

const double Player_Traits<Player_Type::OPL3>::output_gain = pow(10.0, 3.0 / 20.0);

int player_opnmidi_set_bank(struct OPN2_MIDIPlayer *pl, int bank)
{
#pragma message("Using my own bank embed for OPN2. Remove this in the future.")
    static const uint8_t bankdata[] = {
        #include "embedded-banks/opn2.h"
    };
    return (bank != 0) ? -1 :
        Player_Traits<Player_Type::OPN2>::open_bank_data(pl, bankdata, sizeof(bankdata));
}
