//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#if defined(ADLJACK_USE_CURSES)
#include <curses.h>
#include <memory>
#include <stdint.h>

enum {
    Colors_Background = 1,
    Colors_Highlight = 2,
    Colors_Select = 3,
    Colors_Frame = 4,
    Colors_ActiveVolume = 5,
    Colors_KeyDescription = 6,
    Colors_Instrument = 7,
    Colors_InstrumentEx = Colors_Highlight,
    Colors_ProgramNumber = 8,
};

void curses_interface_exec(void (*idle_proc)(void *), void *idle_data);

//------------------------------------------------------------------------------
struct Screen {
    Screen() {}
    ~Screen() { end(); }
    void init()
        { if (!active_) { initscr(); active_ = true; } }
    void end()
        { if (active_) { endwin(); active_ = false; } }
private:
    Screen(const Screen &) = delete;
    Screen &operator=(const Screen &) = delete;
    bool active_ = false;
};

struct WINDOW_deleter { void operator()(WINDOW *w) { delwin(w); } };
typedef std::unique_ptr<WINDOW, WINDOW_deleter> WINDOW_u;

int getrows(WINDOW *w);
int getcols(WINDOW *w);
WINDOW *subwin_s(WINDOW *orig, int lines, int cols, int y, int x);
WINDOW *derwin_s(WINDOW *orig, int lines, int cols, int y, int x);
WINDOW *linewin(WINDOW *w, int row, int col);
int init_color_rgb24(short id, uint32_t value);

//------------------------------------------------------------------------------
bool interface_interrupted();

#endif  // defined(ADLJACK_USE_CURSES)
