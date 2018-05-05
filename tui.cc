//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_CURSES)
#include "tui.h"
#include "common.h"

struct TUI_context
{
    WINDOW_u win_inner;
    WINDOW_u win_playertitle;
    WINDOW_u win_emutitle;
    WINDOW_u win_volume[2];
};

static void setup_display(TUI_context &ctx);
static void update_display(TUI_context &ctx);

enum {
    Colors_ActiveVolume = 1,
};

void curses_interface_exec()
{
    initscr();
    raw();
    keypad(stdscr, true);
    noecho();
    timeout(50);
    curs_set(0);

    {
        TUI_context ctx;
        setup_display(ctx);

        bool quit = false;
        while (!quit) {
            if (is_term_resized(LINES, COLS)) {
                resizeterm(LINES, COLS);
                setup_display(ctx);
                endwin();
                refresh();
            }

            update_display(ctx);

            int ch = getch();
            switch (ch) {
            case 27:  // escape
            case 3:   // console break
                quit = true;
                break;
            case KEY_RESIZE:
                clear();
                break;
            }
        }
    }

    endwin();
}

static void setup_display(TUI_context &ctx)
{
    ctx = TUI_context();

    start_color();
    init_pair(Colors_ActiveVolume, COLOR_GREEN, COLOR_BLACK);

    WINDOW *inner = subwin(stdscr, LINES - 2, COLS - 2, 1, 1);
    if (!inner) return;
    ctx.win_inner.reset(inner);
    int row = 0;

    ctx.win_playertitle = linewin(inner, row++, 0);
    ctx.win_emutitle = linewin(inner, row++, 0);
    ++row;

    for (unsigned channel = 0; channel < 2; ++channel)
        ctx.win_volume[channel] = linewin(inner, row++, 0);
}

static void print_volume_bar(WINDOW *w, double vol)
{
    unsigned size = getcols(w);
    if (size < 2)
        return;
    mvwaddch(w, 0, 0, '[');
    for (unsigned i = 0; i < size - 2; ++i) {
        double ref = (double)i / (size - 2);
        bool gt = vol > ref;
        if (gt) wattron(w, A_BOLD|COLOR_PAIR(Colors_ActiveVolume));
        mvwaddch(w, 0, i + 1, gt ? '*' : '-');
        if (gt) wattroff(w, A_BOLD|COLOR_PAIR(Colors_ActiveVolume));
    }
    mvwaddch(w, 0, size - 1, ']');
}

static void update_display(TUI_context &ctx)
{
    Player_Type pt = ::player_type;

    if (WINDOW *w = ctx.win_playertitle.get()) {
        wclear(w);
        mvwprintw(w, 0, 0, "%s %s", player_name(pt), player_version(pt));
    }
    if (WINDOW *w = ctx.win_emutitle.get()) {
        wclear(w);
        mvwprintw(w, 0, 0, "%s", player_emulator_name(pt));
    }


    double channel_volumes[2] = {lvcurrent[0], lvcurrent[1]};
    const char *channel_names[2] = {"Left", "Right"};

    // enables logarithmic view for perceptual volume, otherwise linear.
    //  (better use linear to watch output for clipping)
    const bool logarithmic = false;

    for (unsigned channel = 0; channel < 2; ++channel) {
        WINDOW *w = ctx.win_volume[channel].get();
        if (!w) continue;

        double vol = channel_volumes[channel];
        if (logarithmic && vol > 0) {
            double db = 20 * log10(vol);
            const double dbmin = -60.0;
            vol = (db - dbmin) / (0 - dbmin);
        }

        mvwprintw(w, 0, 0, "%s", channel_names[channel]);

        WINDOW_u barw(subwin(w, 1, getcols(w) - 6, getbegy(w), getbegx(w) + 6));
        if (barw)
            print_volume_bar(barw.get(), vol);

        wrefresh(w);
    }
}

//------------------------------------------------------------------------------
int getrows(WINDOW *w)
{
    return getmaxy(w) - getbegy(w);
}

int getcols(WINDOW *w)
{
    return getmaxx(w) - getbegx(w);
}

WINDOW_u linewin(WINDOW *w, int row, int col)
{
    return WINDOW_u(subwin(w, 1, getcols(w) - col, getbegy(w) + row, getbegx(w) + col));
}

#endif  // defined(ADLJACK_USE_CURSES)
