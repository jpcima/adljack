//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_CURSES)
#include "tui.h"
#include "tui_fileselect.h"
#include "insnames.h"
#include "common.h"
#include <cmath>
#include <limits.h>
#include <unistd.h>

extern std::string get_program_title();

struct TUI_context
{
    WINDOW_u win_inner;
    WINDOW_u win_playertitle;
    WINDOW_u win_emutitle;
    WINDOW_u win_chipcount;
    WINDOW_u win_cpuratio;
    WINDOW_u win_volume[2];
    WINDOW_u win_instrument[16];
    WINDOW_u win_keydesc1;
};

static void setup_display(TUI_context &ctx);
static void update_display(TUI_context &ctx);

void curses_interface_exec()
{
    initscr();
    raw();
    keypad(stdscr, true);
    noecho();
    const unsigned timeout_ms = 50;
    timeout(timeout_ms);
    curs_set(0);
    set_escdelay(25);

    std::string bank_directory;
    {
        char pathbuf[PATH_MAX + 1];
        if (char *path = getcwd(pathbuf, sizeof(pathbuf)))
            bank_directory.assign(path);
    }

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
            case 'q':
            case 'Q':
            case 3:   // console break
                quit = true;
                break;
            case '<': {
                Player_Type pt = ::player_type;
                unsigned emulator = player_emulator(pt);
                if (emulator > 0)
                    player_dynamic_set_emulator(pt, emulator - 1);
                break;
            }
            case '>': {
                Player_Type pt = ::player_type;
                unsigned emulator = player_emulator(pt);
                player_dynamic_set_emulator(pt, emulator + 1);
                break;
            }
            case '[': {
                Player_Type pt = ::player_type;
                unsigned nchips = player_chip_count(pt);
                if (nchips > 1)
                    player_dynamic_set_chip_count(pt, nchips - 1);
                break;
            }
            case ']': {
                Player_Type pt = ::player_type;
                unsigned nchips = player_chip_count(pt);
                player_dynamic_set_chip_count(pt, nchips + 1);
                break;
            }
            case 'b':
            case 'B': {
                WINDOW_u w(newwin(getrows(stdscr), getcols(stdscr), 0, 0));
                if (w) {
                    File_Selection_Options fopts;
                    fopts.title = "Load bank";
                    fopts.directory = bank_directory;
                    timeout(-1);
                    File_Selection_Code code = fileselect(w.get(), fopts);
                    timeout(timeout_ms);
                    if (code == File_Selection_Code::Break)
                        quit = true;
                    else if (code == File_Selection_Code::Ok) {
                        Player_Type pt = ::player_type;
                        player_dynamic_load_bank(pt, fopts.filepath.c_str());
                        bank_directory = fopts.directory;
                    }
                    clear();
                }
                break;
            }
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
    init_pair(Colors_Highlight, COLOR_YELLOW, COLOR_BLACK);
    init_pair(Colors_Select, COLOR_BLACK, COLOR_WHITE);
    init_pair(Colors_Frame, COLOR_BLUE, COLOR_BLACK);
    init_pair(Colors_ActiveVolume, COLOR_GREEN, COLOR_BLACK);
    init_pair(Colors_KeyDescription, COLOR_BLACK, COLOR_WHITE);

    WINDOW *inner = subwin(stdscr, LINES - 2, COLS - 2, 1, 1);
    if (!inner) return;
    ctx.win_inner.reset(inner);

    unsigned cols = getcols(inner);
    unsigned rows = getrows(inner);

    int row = 0;

    ctx.win_playertitle = linewin(inner, row++, 0);
    ctx.win_emutitle = linewin(inner, row++, 0);
    ctx.win_chipcount = linewin(inner, row++, 0);
    ctx.win_cpuratio = linewin(inner, row++, 0);
    ++row;

    for (unsigned channel = 0; channel < 2; ++channel)
        ctx.win_volume[channel] = linewin(inner, row++, 0);
    ++row;

    for (unsigned midichannel = 0; midichannel < 16; ++midichannel) {
        unsigned width = cols / 2;
        unsigned row2 = row + midichannel % 8;
        unsigned col = (midichannel < 8) ? 0 : width;
        ctx.win_instrument[midichannel].reset(derwin(inner, 1, width, row2, col));
    }
    row += 8;

    ctx.win_keydesc1.reset(derwin(inner, 1, cols, rows - 1, 0));
}

static void print_bar(WINDOW *w, double vol, char ch_on, char ch_off, int attr_on)
{
    unsigned size = getcols(w);
    if (size < 2)
        return;
    mvwaddch(w, 0, 0, '[');
    for (unsigned i = 0; i < size - 2; ++i) {
        double ref = (double)i / (size - 2);
        bool gt = vol > ref;
        if (gt) wattron(w, attr_on);
        mvwaddch(w, 0, i + 1, gt ? ch_on : ch_off);
        if (gt) wattroff(w, attr_on);
    }
    mvwaddch(w, 0, size - 1, ']');
}

static void update_display(TUI_context &ctx)
{
    Player_Type pt = ::player_type;

    {
        std::string title = get_program_title();
        size_t titlesize = title.size();

        attron(A_BOLD|COLOR_PAIR(Colors_Frame));
        border(' ', ' ', '-', ' ', '-', '-', ' ', ' ');
        attroff(A_BOLD|COLOR_PAIR(Colors_Frame));

        unsigned cols = getcols(stdscr);
        if (cols >= titlesize + 2) {
            unsigned x = (cols - (titlesize + 2)) / 2;
            attron(A_BOLD|COLOR_PAIR(Colors_Frame));
            mvaddch(0, x, '(');
            mvaddch(0, x + titlesize + 1, ')');
            attroff(A_BOLD|COLOR_PAIR(Colors_Frame));
            mvaddstr(0, x + 1, title.c_str());
        }
    }

    if (WINDOW *w = ctx.win_playertitle.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "Player");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwprintw(w, 0, 10, "%s %s", player_name(pt), player_version(pt));
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_emutitle.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "Emulator");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwaddstr(w, 0, 10, player_emulator_name(pt));
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_chipcount.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "Chips");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwprintw(w, 0, 10, "%u", player_chip_count(pt));
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_cpuratio.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "CPU");

        WINDOW_u barw(derwin(w, 1, 25, 0, 10));
        if (barw)
            print_bar(barw.get(), cpuratio, '*', '-', COLOR_PAIR(Colors_Highlight));

        // wattron(w, COLOR_PAIR(Colors_Highlight));
        // mvwprintw(w, 0, 10, "%ld%%", std::lround(::cpuratio * 100));
        // wattroff(w, COLOR_PAIR(Colors_Highlight));
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

        mvwaddstr(w, 0, 0, channel_names[channel]);

        WINDOW_u barw(derwin(w, 1, getcols(w) - 6, 0, 6));
        if (barw)
            print_bar(barw.get(), vol, '*', '-', A_BOLD|COLOR_PAIR(Colors_ActiveVolume));

        wrefresh(w);
    }

    for (unsigned midichannel = 0; midichannel < 16; ++midichannel) {
        WINDOW *w = ctx.win_instrument[midichannel].get();
        if (!w) continue;
        wclear(w);
        const Program &pgm = channel_map[midichannel];
        mvwprintw(w, 0, 0, "%2u: [%3u]", midichannel + 1, pgm.gm);
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwaddstr(w, 0, 12, midi_instrument_name[pgm.gm]);
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }

    struct Key_Description {
        const char *key = nullptr;
        const char *desc = nullptr;
    };

    if (WINDOW *w = ctx.win_keydesc1.get()) {
        wclear(w);

        static const Key_Description keydesc[] = {
            { "q", "quit" },
            { "<", "prev emulator" },
            { ">", "next emulator" },
            { "[", "chips +1" },
            { "]", "chips -1" },
            { "b", "load bank" },
        };
        unsigned nkeydesc = sizeof(keydesc) / sizeof(*keydesc);

        wmove(w, 0, 0);
        for (unsigned i = 0; i < nkeydesc; ++i) {
            if (i > 0) waddstr(w, "   ");
            wattron(w, COLOR_PAIR(Colors_KeyDescription));
            waddstr(w, keydesc[i].key);
            wattroff(w, COLOR_PAIR(Colors_KeyDescription));
            waddstr(w, " ");
            waddstr(w, keydesc[i].desc);
        }
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
    return WINDOW_u(derwin(w, 1, getcols(w) - col, row, col));
}

#endif  // defined(ADLJACK_USE_CURSES)
