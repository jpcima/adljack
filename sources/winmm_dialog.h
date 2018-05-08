//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#if defined(_WIN32)
#include "rtmain.h"
#include <windows.h>

// returns >=0 for system port, -2 for virtual port, -1 for none
int dlg_select_midi_port(Audio_Context &ctx);

#endif  // defined(_WIN32)
