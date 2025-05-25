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
    Colors_MidiCh1 = 9,
    Colors_MidiCh16 = Colors_MidiCh1 + 15,
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

typedef struct TUI_context* TUI_contextP;
class Player;

bool handle_toplevel_key_p(TUI_contextP ctx, int key);
bool handle_anylevel_key_p(TUI_contextP ctx, int key);
void show_status_p(TUI_contextP ctx, std::string text, unsigned timeout = 10);
bool update_bank_mtime_p(TUI_contextP ctx);

void handle_ctx_quit(TUI_contextP ctx);
Player *handle_ctx_get_player(TUI_contextP ctx);
std::string &handle_ctx_bank_directory(TUI_contextP ctx);

#endif  // defined(ADLJACK_USE_CURSES)
