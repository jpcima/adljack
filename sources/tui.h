//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#if defined(ADLJACK_USE_CURSES)
#include <curses.h>
#include <memory>

enum {
    Colors_Background = 1,
    Colors_Highlight,
    Colors_Select,
    Colors_Frame,
    Colors_ActiveVolume,
    Colors_KeyDescription,
};

void curses_interface_exec();

//------------------------------------------------------------------------------
struct WINDOW_deleter { void operator()(WINDOW *w) { delwin(w); } };
typedef std::unique_ptr<WINDOW, WINDOW_deleter> WINDOW_u;

int getrows(WINDOW *w);
int getcols(WINDOW *w);
WINDOW_u linewin(WINDOW *w, int row, int col);

//------------------------------------------------------------------------------
bool interface_interrupted();

#endif  // defined(ADLJACK_USE_CURSES)
