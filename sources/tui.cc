//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_CURSES)
#include "tui.h"
#include "tui_fileselect.h"
#include "insnames.h"
#include "common.h"
#include <chrono>
#include <cmath>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
namespace stc = std::chrono;

extern std::string get_program_title();

struct TUI_context
{
    WINDOW_u win_inner;
    WINDOW_u win_playertitle;
    WINDOW_u win_emutitle;
    WINDOW_u win_chipcount;
    WINDOW_u win_cpuratio;
    WINDOW_u win_banktitle;
    WINDOW_u win_volumeratio;
    WINDOW_u win_volume[2];
    WINDOW_u win_instrument[16];
    WINDOW_u win_status;
    WINDOW_u win_keydesc1;
    WINDOW_u win_keydesc2;
    std::string status_text;
    bool status_display = false;
    unsigned status_timeout = 0;
    stc::steady_clock::time_point status_start;
};

static void setup_colors();
static void setup_display(TUI_context &ctx);
static void update_display(TUI_context &ctx);
static void show_status(TUI_context &ctx, const std::string &text, unsigned timeout = 10);

void curses_interface_exec()
{
    initscr();
    if (has_colors())
        setup_colors();
    raw();
    keypad(stdscr, true);
    noecho();
    const unsigned timeout_ms = 50;
    timeout(timeout_ms);
    curs_set(0);

#if !defined(PDCURSES)
    set_escdelay(25);
#endif

#ifdef PDCURSES
    PDC_set_title(get_program_title().c_str());
#endif

    std::string bank_directory;
    {
        char pathbuf[PATH_MAX + 1];
        if (char *path = getcwd(pathbuf, sizeof(pathbuf)))
            bank_directory.assign(path);
    }

    time_t player_bank_mtime[player_type_count] = { 0 };

    auto update_bank_mtime =
        [&]() -> bool {
            struct stat st;
            const char *path = active_bank_file().c_str();
            Player_Type pt = active_player().type();
            time_t old_mtime = player_bank_mtime[(unsigned)pt];
            time_t new_mtime = (path[0] && !stat(path, &st)) ? st.st_mtime : 0;
            player_bank_mtime[(unsigned)pt] = new_mtime;
            return new_mtime && new_mtime != old_mtime;
        };

    {
        TUI_context ctx;
        setup_display(ctx);

        show_status(ctx, "Ready!");
        update_bank_mtime();

        unsigned bank_check_interval = 1;
        stc::steady_clock::time_point bank_check_last = stc::steady_clock::now();

        bool quit = false;
        while (!quit && !interface_interrupted()) {
#if 0
#if defined(PDCURSES)
            bool resized = is_termresized();
#else
            bool resized = is_term_resized(LINES, COLS);
#endif
            if (resized) {
#if defined(PDCURSES)
                resize_term(LINES, COLS);
#else
                resizeterm(LINES, COLS);
#endif
                setup_display(ctx);
                endwin();
                refresh();
            }
#endif

            stc::steady_clock::time_point now = stc::steady_clock::now();
            if (now - bank_check_last > stc::seconds(bank_check_interval)) {
                if (update_bank_mtime()) {
                    if (active_player().dynamic_load_bank(active_bank_file().c_str()))
                        show_status(ctx, "Bank has changed on disk. Reload!");
                    else
                        show_status(ctx, "Bank has changed on disk. Reloading failed.");
                }
                bank_check_last = now;
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
                if (active_emulator_id > 0)
                    dynamic_switch_emulator_id(active_emulator_id - 1);
                break;
            }
            case '>': {
                if (active_emulator_id + 1 < emulator_ids.size())
                    dynamic_switch_emulator_id(active_emulator_id + 1);
                break;
            }
            case '[': {
                Player &player = active_player();
                unsigned nchips = player.chip_count();
                if (nchips > 1)
                    player.dynamic_set_chip_count(nchips - 1);
                break;
            }
            case ']': {
                Player &player = active_player();
                unsigned nchips = player.chip_count();
                player.dynamic_set_chip_count(nchips + 1);
                break;
            }
            case '/': {
                ::player_volume = std::max(0, player_volume - 1);
                break;
            }
            case '*': {
                ::player_volume = std::min(500, player_volume + 1);
                break;
            }
            case 'b':
            case 'B': {
                WINDOW_u w(newwin(getrows(stdscr), getcols(stdscr), 0, 0));
                if (w) {
                    File_Selection_Options fopts;
                    fopts.title = "Load bank";
                    fopts.directory = bank_directory;
                    File_Selector fs(w.get(), fopts);
                    File_Selection_Code code = File_Selection_Code::Continue;
                    fs.update();
                    for (int ch = getch(); !quit && !interface_interrupted() &&
                             code == File_Selection_Code::Continue; ch = getch()) {
                        switch (ch) {
                        case 'q':
                        case 'Q':
                        case 3:   // console break
                            quit = true;
                            break;
                        default:
                            code = fs.key(ch);
                            fs.update();
                            break;
                        }
                    }
                    if (code == File_Selection_Code::Ok) {
                        Player &player = active_player();
                        if (player.dynamic_load_bank(fopts.filepath.c_str())) {
                            show_status(ctx, "Bank loaded!");
                            active_bank_file() = fopts.filepath;
                            update_bank_mtime();
                        }
                        else
                            show_status(ctx, "Error loading the bank file.");
                        bank_directory = fopts.directory;
                    }
                    clear();
                }
                break;
            }
            case 'p':
            case 'P': {
                Player &player = active_player();
                player.dynamic_panic();
                break;
            }
#if 0
            case KEY_RESIZE:
                clear();
                break;
#endif
            }
        }
    }

    endwin();

    if (interface_interrupted())
        fprintf(stderr, "Interrupted.\n");
}

static void setup_colors()
{
    start_color();

#if defined(PDCURSES) && !defined(PDC_RGB)
#    error PDCurses color definitions are not defined in the appropriate order.
#endif

    if (can_change_color()) {
#ifdef PDCURSES
        constexpr uint32_t tango_colors[16] = {
            0x2e3436, 0xcc0000, 0x4e9a06, 0xc4a000,
            0x3465a4, 0x75507b, 0x06989a, 0xd3d7cf,
            0x555753, 0xef2929, 0x8ae234, 0xf57900,
            0x729fcf, 0xad7fa8, 0x34e2e2, 0xeeeeec,
        };
        for (unsigned i = 0; i < 16; ++i) {
            unsigned r = (tango_colors[i] >> 16) & 0xff;
            unsigned g = (tango_colors[i] >> 8) & 0xff;
            unsigned b = (tango_colors[i] >> 0) & 0xff;
            init_color(i, r * 1000 / 0xff, g * 1000 / 0xff, b * 1000 / 0xff);
        }
#endif
        init_pair(Colors_Background, COLOR_WHITE, COLOR_BLACK);
        init_pair(Colors_Highlight, COLOR_YELLOW, COLOR_BLACK);
        init_pair(Colors_Select, COLOR_BLACK, COLOR_WHITE);
        init_pair(Colors_Frame, COLOR_BLUE, COLOR_BLACK);
        init_pair(Colors_ActiveVolume, COLOR_GREEN, COLOR_BLACK);
        init_pair(Colors_KeyDescription, COLOR_BLACK, COLOR_WHITE);
    }
}

static void setup_display(TUI_context &ctx)
{
    ctx = TUI_context();

    bkgd(COLOR_PAIR(Colors_Background));

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
    ctx.win_banktitle = linewin(inner, row++, 0);
    ctx.win_volumeratio = linewin(inner, row++, 0);

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

    ctx.win_status.reset(derwin(inner, 1, cols, rows - 4, 0));
    ctx.win_keydesc1.reset(derwin(inner, 1, cols, rows - 2, 0));
    ctx.win_keydesc2.reset(derwin(inner, 1, cols, rows - 1, 0));
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
    Player &player = active_player();
    Player_Type ptype = player.type();

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
        mvwprintw(w, 0, 10, "%s %s", player.name(ptype), player.version(ptype));
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_emutitle.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "Emulator");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwaddstr(w, 0, 10, player.emulator_name());
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_chipcount.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "Chips");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwprintw(w, 0, 10, "%u", player.chip_count());
        wattroff(w, COLOR_PAIR(Colors_Highlight));
        waddstr(w, " * ");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        waddstr(w, player.chip_name(ptype));
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_cpuratio.get()) {
        wclear(w);
        mvwaddstr(w, 0, 0, "CPU");

        WINDOW_u barw(derwin(w, 1, 25, 0, 10));
        if (barw)
            print_bar(barw.get(), cpuratio, '*', '-', COLOR_PAIR(Colors_Highlight));
    }
    if (WINDOW *w = ctx.win_banktitle.get()) {
        wclear(w);
        std::string title;
        const std::string &path = active_bank_file();
        if (path.empty())
            title = "(default)";
        else
        {
#if !defined(_WIN32)
            size_t pos = path.rfind('/');
#else
            size_t pos = path.find_last_of("/\\");
#endif
            title = (pos != path.npos) ? path.substr(pos + 1) : path;
        }
        mvwaddstr(w, 0, 0, "Bank");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwaddstr(w, 0, 10, title.c_str());
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }

    double channel_volumes[2] = {lvcurrent[0], lvcurrent[1]};
    const char *channel_names[2] = {"Left", "Right"};

    // enables logarithmic view for perceptual volume, otherwise linear.
    //  (better use linear to watch output for clipping)
    const bool logarithmic = false;

    if (WINDOW *w = ctx.win_volumeratio.get()) {
        mvwaddstr(w, 0, 0, "Volume");
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwprintw(w, 0, 10, "%d%%\n", ::player_volume);
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }

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
        if (midi_channel_note_count[midichannel] > 0) {
            wattron(w, A_BOLD|COLOR_PAIR(Colors_ActiveVolume));
            mvwaddch(w, 0, 11, '*');
            wattroff(w, A_BOLD|COLOR_PAIR(Colors_ActiveVolume));
        }
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwaddstr(w, 0, 12, midi_instrument_name[pgm.gm]);
        wattroff(w, COLOR_PAIR(Colors_Highlight));
    }

    if (WINDOW *w = ctx.win_status.get()) {
        if (!ctx.status_text.empty()) {
            if (!ctx.status_display) {
                wclear(w);
                wattron(w, COLOR_PAIR(Colors_Select));
                waddstr(w, ctx.status_text.c_str());
                wattroff(w, COLOR_PAIR(Colors_Select));
                ctx.status_start = stc::steady_clock::now();
                ctx.status_display = true;
            }
            else if (stc::steady_clock::now() - ctx.status_start > stc::seconds(ctx.status_timeout)) {
                wclear(w);
                ctx.status_text.clear();
                ctx.status_display = false;
            }
        }
    }

    struct Key_Description {
        Key_Description(const char *key, const char *desc)
            : key(key), desc(desc) {}
        const char *key = nullptr;
        const char *desc = nullptr;
    };

    const unsigned key_spacing = 16;

    if (WINDOW *w = ctx.win_keydesc1.get()) {
        wclear(w);

        static const Key_Description keydesc[] = {
            { "<", "prev emulator" },
            { ">", "next emulator" },
            { "[", "chips -1" },
            { "]", "chips +1" },
            { "b", "load bank" },
        };
        unsigned nkeydesc = sizeof(keydesc) / sizeof(*keydesc);

        for (unsigned i = 0; i < nkeydesc; ++i) {
            wmove(w, 0, i * key_spacing);
            wattron(w, COLOR_PAIR(Colors_KeyDescription));
            waddstr(w, keydesc[i].key);
            wattroff(w, COLOR_PAIR(Colors_KeyDescription));
            waddstr(w, " ");
            waddstr(w, keydesc[i].desc);
        }
    }

    if (WINDOW *w = ctx.win_keydesc2.get()) {
        wclear(w);

        static const Key_Description keydesc[] = {
            { "/", "volume -1" },
            { "*", "volume +1" },
            { "p", "panic" },
            { "q", "quit" },
        };
        unsigned nkeydesc = sizeof(keydesc) / sizeof(*keydesc);

        for (unsigned i = 0; i < nkeydesc; ++i) {
            wmove(w, 0, i * key_spacing);
            wattron(w, COLOR_PAIR(Colors_KeyDescription));
            waddstr(w, keydesc[i].key);
            wattroff(w, COLOR_PAIR(Colors_KeyDescription));
            waddstr(w, " ");
            waddstr(w, keydesc[i].desc);
        }
    }
}

static void show_status(TUI_context &ctx, const std::string &text, unsigned timeout)
{
    ctx.status_text = text;
    ctx.status_display = false;
    ctx.status_timeout = timeout;
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
