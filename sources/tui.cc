//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_CURSES)
#include "tui.h"
#include "tui_channels.h"
#include "tui_fileselect.h"
#include "insnames.h"
#include "i18n.h"
#include "common.h"
#include <chrono>
#include <cmath>
#include <limits.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(ADLJACK_GTK3)
#   include <gtk/gtk.h>
#   include <asm/ioctls.h>
#   include <sys/select.h>
#   include <sys/ioctl.h>
#   include <termios.h>
#endif
#if defined(PDCURSES)
#include <SDL.h>
#endif
namespace stc = std::chrono;

extern std::string get_program_title();

struct TUI_windows
{
    WINDOW_u outer;
    WINDOW_u inner;
    WINDOW_u playertitle;
    WINDOW_u emutitle;
    WINDOW_u chipcount;
    WINDOW_u cpuratio;
    WINDOW_u banktitle;
    WINDOW_u chanalloc;
    WINDOW_u volumeratio;
    WINDOW_u volume[2];
    WINDOW_u instrument[16];
    WINDOW_u status;
    WINDOW_u keydesc1;
    WINDOW_u keydesc2;
    WINDOW_u keydesc3;
};

struct TUI_context
{
    bool quit = false;
    TUI_windows win;
    std::string status_text;
    bool status_display = false;
    unsigned status_timeout = 0;
    stc::steady_clock::time_point status_start;
    Player *player = nullptr;
    std::string bank_directory;
    time_t bank_mtime[player_type_count] = {};
    static constexpr unsigned perc_display_interval = 10;
    unsigned perc_display_cycle = 0;
    bool have_perc_display_program = false;
    Midi_Program_Ex perc_display_program;
    struct Channel_State {
        std::unique_ptr<char[]> data;
        unsigned size = 0;
        unsigned serial = 0;
    };
    Channel_State channel_state;
    void (*idle_proc)(void *) = nullptr;
    void *idle_data = nullptr;
};

#if defined(PDCURSES)
static void install_event_hook(TUI_context &ctx);
#endif
static void setup_colors();
static void setup_display(TUI_context &ctx);
static void update_display(TUI_context &ctx);
static void show_status(TUI_context &ctx, std::string text, unsigned timeout = 10);
static bool handle_anylevel_key(TUI_context &ctx, int key);
static bool handle_toplevel_key(TUI_context &ctx, int key);
static void handle_notifications(TUI_context &ctx);
static bool update_bank_mtime(TUI_context &ctx);

#ifdef ADLJACK_GTK3
static GtkStatusIcon *s_tray_icon = nullptr;

static void updateIconIfNeeded(int newEmu);

//void tray_icon_on_click(GtkStatusIcon *status_icon, gpointer user_data)
//{
//    //
//}

static void tray_icon_open_bank(TUI_context *ctx)
{
    handle_toplevel_key(*ctx, (int)'b');
}

static void tray_icon_quit(TUI_context *ctx)
{
    handle_anylevel_key(*ctx, (int)'q');
}

static void tray_icon_quickVolume(int volume)
{
    ::player_volume = std::min(volume_max, std::max(volume_min, volume));
    configFile.beginGroup("synth");
    configFile.setValue("volume", ::player_volume);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_quickVolume50(void *)
{
    tray_icon_quickVolume(50);
}

static void tray_icon_quickVolume100(void *)
{
    tray_icon_quickVolume(100);
}

static void tray_icon_quickVolume150(void *)
{
    tray_icon_quickVolume(150);
}

static void tray_icon_quickVolume200(void *)
{
    tray_icon_quickVolume(200);
}

static void tray_icon_quickVolume250(void *)
{
    tray_icon_quickVolume(250);
}

static void tray_icon_quickVolume300(void *)
{
    tray_icon_quickVolume(300);
}

static void tray_icon_quickVolume350(void *)
{
    tray_icon_quickVolume(350);
}

static void tray_icon_quickVolume400(void *)
{
    tray_icon_quickVolume(400);
}

static void tray_icon_quickVolume450(void *)
{
    tray_icon_quickVolume(450);
}

static void tray_icon_quickVolume500(void *)
{
    tray_icon_quickVolume(500);
}


static void tray_icon_chanAlloc(int mode)
{
    active_player().dynamic_set_channel_alloc(mode);
    configFile.beginGroup("synth");
    configFile.setValue("chanalloc", mode);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_chanAllocAuto(void *)
{
    tray_icon_chanAlloc(-1);
}

static void tray_icon_chanOffDelay(void *)
{
    tray_icon_chanAlloc(0);
}

static void tray_icon_chanSameInst(void *)
{
    tray_icon_chanAlloc(1);
}

static void tray_icon_chanAnyFree(void *)
{
    tray_icon_chanAlloc(2);
}

static void tray_icon_switchEmulator(Emulator_Id *e)
{
    auto emulator_id_pos = std::find(emulator_ids.begin(), emulator_ids.end(), *e);
    if (emulator_id_pos == emulator_ids.end()) {
        return;
    }

    int newId = std::distance(emulator_ids.begin(), emulator_id_pos);

    updateIconIfNeeded(newId);
    dynamic_switch_emulator_id(newId);
    configFile.beginGroup("synth");
    configFile.setValue("emulator", e->emulator);
    configFile.setValue("pt", (int)e->player);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data)
{
    // TUI_context *ctx = (TUI_context*)user_data;
    // GdkEventButton *event_button;

    // g_return_val_if_fail (status_icon != NULL, FALSE);
    // g_return_val_if_fail (GTK_IS_MENU (widget), FALSE);
    // g_return_val_if_fail (event != NULL, FALSE);

    // The "widget" is the menu that was supplied when
    // `g_signal_connect_swapped()` was called.
    // menu = GTK_MENU(status_icon);

    GtkWidget *menu = gtk_menu_new();
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *itemSelectBank = gtk_menu_item_new_with_label("Select bank...");
        gtk_widget_show(itemSelectBank);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemSelectBank);
        g_signal_connect_swapped(G_OBJECT(itemSelectBank), "activate",
                                 G_CALLBACK(tray_icon_open_bank), user_data);
    }
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *sep1 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep1);
    }
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *quickVolume = gtk_menu_item_new_with_label("Set volume to");
        GtkWidget *quickVolumeSubMenu = gtk_menu_new();
        auto *quickVolumeCur = gtk_menu_item_new_with_label(("Current volume: " + std::to_string(player_volume) + "%").c_str());
        auto *quickVolume50 = gtk_menu_item_new_with_label("50%");
        auto *quickVolume100 = gtk_menu_item_new_with_label("100%");
        auto *quickVolume150 = gtk_menu_item_new_with_label("150%");
        auto *quickVolume200 = gtk_menu_item_new_with_label("200%");
        auto *quickVolume250 = gtk_menu_item_new_with_label("250%");
        auto *quickVolume300 = gtk_menu_item_new_with_label("300%");
        auto *quickVolume350 = gtk_menu_item_new_with_label("350%");
        auto *quickVolume400 = gtk_menu_item_new_with_label("400%");
        auto *quickVolume450 = gtk_menu_item_new_with_label("450%");
        auto *quickVolume500 = gtk_menu_item_new_with_label("500%");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(quickVolume), quickVolumeSubMenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolumeCur);
        gtk_widget_set_sensitive(quickVolumeCur, FALSE);
        GtkWidget *volSep2 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), volSep2);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume50);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume100);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume150);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume200);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume250);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume300);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume350);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume400);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume450);
        gtk_menu_shell_append(GTK_MENU_SHELL(quickVolumeSubMenu), quickVolume500);
        g_signal_connect_swapped(G_OBJECT(quickVolume50), "activate", G_CALLBACK(tray_icon_quickVolume50), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume100), "activate", G_CALLBACK(tray_icon_quickVolume100), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume150), "activate", G_CALLBACK(tray_icon_quickVolume150), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume200), "activate", G_CALLBACK(tray_icon_quickVolume200), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume250), "activate", G_CALLBACK(tray_icon_quickVolume250), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume300), "activate", G_CALLBACK(tray_icon_quickVolume300), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume350), "activate", G_CALLBACK(tray_icon_quickVolume350), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume400), "activate", G_CALLBACK(tray_icon_quickVolume400), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume450), "activate", G_CALLBACK(tray_icon_quickVolume450), NULL);
        g_signal_connect_swapped(G_OBJECT(quickVolume500), "activate", G_CALLBACK(tray_icon_quickVolume500), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quickVolume);
    }
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *emulatorType = gtk_menu_item_new_with_label("Emulator type");
        GtkWidget *emulatorTypeSubMenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(emulatorType), emulatorTypeSubMenu);

        int ppt = -1;

        for (size_t i = 0; i < emulator_ids.size(); ++i) {
            auto &e = emulator_ids[i];
            if ((int)e.player != ppt) {
                GtkWidget *sep2 = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(emulatorTypeSubMenu), sep2);

                ppt = (int)e.player;
                GtkWidget *label;
                switch(e.player)
                {
                default:
                case Player_Type::OPL3:
                    label = gtk_menu_item_new_with_label("libADLMIDI:");
                    break;
                case Player_Type::OPN2:
                    label = gtk_menu_item_new_with_label("libOPNMIDI:");
                    break;
                }
                gtk_menu_shell_append(GTK_MENU_SHELL(emulatorTypeSubMenu), label);
                gtk_widget_set_sensitive(label, FALSE);

                GtkWidget *sep3 = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(emulatorTypeSubMenu), sep3);
            }

            auto *emuItem = gtk_check_menu_item_new_with_label(e.name.c_str());
            gtk_menu_shell_append(GTK_MENU_SHELL(emulatorTypeSubMenu), emuItem);
            g_signal_connect_swapped(G_OBJECT(emuItem), "activate",
                                     G_CALLBACK(tray_icon_switchEmulator), &e);
            if (i == active_emulator_id) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(emuItem), TRUE);
            }
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), emulatorType);
    }

    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *chanAlloc = gtk_menu_item_new_with_label("Channel allocation mode");
        GtkWidget *chanAllocSubMenu = gtk_menu_new();
        auto *chanAllocAuto = gtk_check_menu_item_new_with_label("[Auto]");
        auto *chanAllocOffDelay = gtk_check_menu_item_new_with_label("Off delay based");
        auto *chanAllocSameInst = gtk_check_menu_item_new_with_label("Re-use same instrument");
        auto *chanAllocAnyFree = gtk_check_menu_item_new_with_label("Re-use any free");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(chanAlloc), chanAllocSubMenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(chanAllocSubMenu), chanAllocAuto);
        gtk_menu_shell_append(GTK_MENU_SHELL(chanAllocSubMenu), chanAllocOffDelay);
        gtk_menu_shell_append(GTK_MENU_SHELL(chanAllocSubMenu), chanAllocSameInst);
        gtk_menu_shell_append(GTK_MENU_SHELL(chanAllocSubMenu), chanAllocAnyFree);
        g_signal_connect_swapped(G_OBJECT(chanAllocAuto), "activate", G_CALLBACK(tray_icon_chanAllocAuto), NULL);
        g_signal_connect_swapped(G_OBJECT(chanAllocOffDelay), "activate", G_CALLBACK(tray_icon_chanOffDelay), NULL);
        g_signal_connect_swapped(G_OBJECT(chanAllocSameInst), "activate", G_CALLBACK(tray_icon_chanSameInst), NULL);
        g_signal_connect_swapped(G_OBJECT(chanAllocAnyFree), "activate", G_CALLBACK(tray_icon_chanAnyFree), NULL);
        int chanAllocMode = active_player().get_channel_alloc_mode_val();
        switch(chanAllocMode)
        {
        case ADLMIDI_ChanAlloc_AUTO:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(chanAllocAuto), TRUE);
            break;
        case ADLMIDI_ChanAlloc_OffDelay:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(chanAllocOffDelay), TRUE);
            break;
        case ADLMIDI_ChanAlloc_SameInst:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(chanAllocSameInst), TRUE);
            break;
        case ADLMIDI_ChanAlloc_AnyReleased:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(chanAllocAnyFree), TRUE);
            break;
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), chanAlloc);
    }
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *sep2 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep2);
    }
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *itemQuit = gtk_menu_item_new_with_label("Quit");
        gtk_widget_show(itemQuit);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemQuit);
        g_signal_connect_swapped(G_OBJECT(itemQuit), "activate",
                                 G_CALLBACK(tray_icon_quit), user_data);
    }
    // ------------------------------------------------------------------------------------------
    gtk_widget_show_all(menu);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, activate_time);
}

static GtkStatusIcon *create_tray_icon()
{
    GtkStatusIcon *tray_icon;

    tray_icon = gtk_status_icon_new();
    // g_signal_connect(G_OBJECT(tray_icon), "activate", G_CALLBACK(tray_icon_on_click), NULL);
    gtk_status_icon_set_from_icon_name(tray_icon, "adljack");
    gtk_status_icon_set_tooltip_text(tray_icon, "ADLJack is running");
    gtk_status_icon_set_visible(tray_icon, TRUE);

    return tray_icon;
}

static void updateIconIfNeeded(int newEmu)
{
    Emulator_Id old_id = emulator_ids[active_emulator_id];
    Emulator_Id new_id  = emulator_ids[newEmu];

    if (old_id.player != new_id.player) {
        if (new_id.player == Player_Type::OPL3) {
            gtk_status_icon_set_from_icon_name(s_tray_icon, "adljack");
        } else {
            gtk_status_icon_set_from_icon_name(s_tray_icon, "opnjack");
        }
        gtk_main_iteration_do(false);
    }
}
#endif

void curses_interface_exec(void (*idle_proc)(void *), void *idle_data)
{
    Screen screen;
    screen.init();

    TUI_context ctx;
    ctx.idle_proc = idle_proc;
    ctx.idle_data = idle_data;
#if defined(PDCURSES)
    install_event_hook(ctx);
#endif

    if (has_colors())
        setup_colors();
    raw();
    keypad(stdscr, true);
    noecho();
#ifdef ADLJACK_GTK3
    const unsigned timeout_ms = 1;
#else
    const unsigned timeout_ms = 50;
#endif
    timeout(timeout_ms);
    curs_set(0);
#if !defined(PDCURSES)
    set_escdelay(25);
#endif

#if defined(PDCURSES)
    PDC_set_title(get_program_title().c_str());
#endif

    {
        char pathbuf[PATH_MAX + 1];
        if (char *path = getcwd(pathbuf, sizeof(pathbuf)))
            ctx.bank_directory.assign(path);
    }

    configFile.beginGroup("tui");
    ctx.bank_directory = configFile.value("bank_directory", ctx.bank_directory).toString();
    configFile.endGroup();

#ifdef ADLJACK_GTK3
    s_tray_icon = create_tray_icon();

    if (active_player().type() == Player_Type::OPL3) {
        gtk_status_icon_set_from_icon_name(s_tray_icon, "adljack");
    } else {
        gtk_status_icon_set_from_icon_name(s_tray_icon, "opnjack");
    }

    g_signal_connect(G_OBJECT(s_tray_icon), "popup-menu", G_CALLBACK(tray_icon_on_menu),  &ctx);
#endif

    setup_display(ctx);
    show_status(ctx, _("Ready!"));

    unsigned bank_check_interval = 1;
    stc::steady_clock::time_point bank_check_last = stc::steady_clock::now();

    while (!ctx.quit && !interface_interrupted()) {
        if (idle_proc)
            idle_proc(idle_data);

        handle_notifications(ctx);

        Player *player = have_active_player() ? &active_player() : nullptr;
        if (ctx.player != player) {
            ctx.player = player;
            update_bank_mtime(ctx);
        }

        stc::steady_clock::time_point now = stc::steady_clock::now();
        if (now - bank_check_last > stc::seconds(bank_check_interval)) {
            if (update_bank_mtime(ctx)) {
                if (ctx.player->dynamic_load_bank(active_bank_file().c_str()))
                    show_status(ctx, _("Bank has changed on disk. Reload!"));
                else
                    show_status(ctx, _("Bank has changed on disk. Reloading failed."));
            }
            bank_check_last = now;
        }

        update_display(ctx);

#ifdef ADLJACK_GTK3
        gtk_main_iteration_do(false);
#endif
        int key = getch();
        if (!handle_anylevel_key(ctx, key))
            handle_toplevel_key(ctx, key);
        doupdate();
    }
    ctx.win = TUI_windows();
    screen.end();

#ifdef ADLJACK_GTK3
    gtk_status_icon_set_visible(s_tray_icon, FALSE);
#endif

    if (interface_interrupted())
        fprintf(stderr, "Interrupted.\n");
}

#if defined(PDCURSES)
static int event_hook(void *userdata, SDL_Event *event)
{
    TUI_context &ctx = *(TUI_context *)userdata;

    if (event->type == SDL_QUIT) {
        // prevents pdcurses from invoking exit()
        ctx.quit = true;
        return 0;
    }

    return 1;
}

static void install_event_hook(TUI_context &ctx)
{
    SDL_SetEventFilter(&event_hook, &ctx);
}
#endif

static void setup_colors()
{
    start_color();

//#if defined(PDCURSES)
    if (can_change_color()) {
        /* set Tango colors */
        init_color_rgb24(COLOR_BLACK, 0x2e3436);
        init_color_rgb24(COLOR_RED, 0xcc0000);
        init_color_rgb24(COLOR_GREEN, 0x4e9a06);
        init_color_rgb24(COLOR_YELLOW, 0xc4a000);
        init_color_rgb24(COLOR_BLUE, 0x3465a4);
        init_color_rgb24(COLOR_MAGENTA, 0x75507b);
        init_color_rgb24(COLOR_CYAN, 0x06989a);
        init_color_rgb24(COLOR_WHITE, 0xd3d7cf);
        init_color_rgb24(8|COLOR_BLACK, 0x555753);
        init_color_rgb24(8|COLOR_RED, 0xef2929);
        init_color_rgb24(8|COLOR_GREEN, 0x8ae234);
        init_color_rgb24(8|COLOR_YELLOW, 0xf57900);
        init_color_rgb24(8|COLOR_BLUE, 0x729fcf);
        init_color_rgb24(8|COLOR_MAGENTA, 0xad7fa8);
        init_color_rgb24(8|COLOR_CYAN, 0x34e2e2);
        init_color_rgb24(8|COLOR_WHITE, 0xeeeeec);
    }
//#endif

    init_pair(Colors_Background, COLOR_WHITE, COLOR_BLACK);
    init_pair(Colors_Highlight, COLOR_YELLOW, COLOR_BLACK);
    init_pair(Colors_Select, COLOR_BLACK, COLOR_WHITE);
    init_pair(Colors_Frame, COLOR_BLUE, COLOR_BLACK);
    init_pair(Colors_ActiveVolume, COLOR_GREEN, COLOR_BLACK);
    init_pair(Colors_KeyDescription, COLOR_BLACK, COLOR_WHITE);
    init_pair(Colors_Instrument, COLOR_BLUE, COLOR_BLACK);
    init_pair(Colors_ProgramNumber, COLOR_MAGENTA, COLOR_BLACK);

    const unsigned rgb_colors[16] = {
        // (loop :for i :from 0 :below 16 :collect
        //   (print-hex-rgb (hsv-to-rgb (hsv (* (/ i 16) 360) 1.0 0.8))))
        0xbf0000, 0xbf4800, 0xbf8f00, 0xa7bf00, 0x60bf00, 0x18bf00, 0x00bf30, 0x00bf78, 0x00bfbf,
        0x0078bf, 0x0030bf, 0x1800bf, 0x6000bf, 0xa700bf, 0xbf008f, 0xbf0048,
    };

    for (unsigned i = 0; i < 16; ++i) {
        init_color_rgb24(16 + i, rgb_colors[i]);
        init_pair(Colors_MidiCh1 + i, COLOR_WHITE, 16 + i);
    }
}

static void setup_display(TUI_context &ctx)
{
    ctx.win = TUI_windows();

    bkgd(COLOR_PAIR(Colors_Background));

    WINDOW *outer = derwin_s(stdscr, LINES, COLS, 0, 0);
    if (!outer) return;
    ctx.win.outer.reset(outer);

    WINDOW *inner = derwin_s(outer, LINES - 2, COLS - 2, 1, 1);
    if (!inner) return;
    ctx.win.inner.reset(inner);

    unsigned cols = getcols(inner);
    unsigned rows = getrows(inner);

    int row = 0;

    ctx.win.playertitle.reset(linewin(inner, row++, 0));
    ctx.win.emutitle.reset(linewin(inner, row++, 0));
    ctx.win.chipcount.reset(linewin(inner, row++, 0));
    ctx.win.cpuratio.reset(linewin(inner, row++, 0));
    ctx.win.banktitle.reset(linewin(inner, row++, 0));
    ctx.win.chanalloc.reset(linewin(inner, row++, 0));
    ctx.win.volumeratio.reset(linewin(inner, row++, 0));

    for (unsigned channel = 0; channel < 2; ++channel)
        ctx.win.volume[channel].reset(linewin(inner, row++, 0));
    ++row;

    for (unsigned midichannel = 0; midichannel < 16; ++midichannel) {
        unsigned width = cols / 2;
        unsigned row2 = row + midichannel % 8;
        unsigned col = (midichannel < 8) ? 0 : width;
        ctx.win.instrument[midichannel].reset(derwin_s(inner, 1, (int)width - 1, row2, col));
    }
    row += 8;

    ctx.win.status.reset(derwin_s(inner, 1, cols, rows - 5, 0));
    ctx.win.keydesc1.reset(derwin_s(inner, 1, cols, rows - 3, 0));
    ctx.win.keydesc2.reset(derwin_s(inner, 1, cols, rows - 2, 0));
    ctx.win.keydesc3.reset(derwin_s(inner, 1, cols, rows - 1, 0));
}

static int print_bar(
    WINDOW *w, int y, int x, int size,
    double vol, char ch_on, char ch_off, int attr_on)
{
    if (size < 2)
        return ERR;
    if (wmove(w, y, x) == ERR)
        return ERR;
    waddch(w, '[');
    for (int i = 0; i < size - 2; ++i) {
        double ref = (double)i / (size - 2);
        bool gt = vol > ref;
        if (gt) wattron(w, attr_on);
        waddch(w, gt ? ch_on : ch_off);
        if (gt) wattroff(w, attr_on);
    }
    waddch(w, ']');
    return OK;
}

static void update_display(TUI_context &ctx)
{
    Player *player = ctx.player;

    if (WINDOW *w = ctx.win.outer.get()) {
        std::string title = get_program_title();
        size_t titlesize = title.size();

        wattron(w, A_BOLD|COLOR_PAIR(Colors_Frame));
        wborder(w, ' ', ' ', '-', '-', '-', '-', '-', '-');
        wattroff(w, A_BOLD|COLOR_PAIR(Colors_Frame));

        unsigned cols = getcols(stdscr);
        if (cols >= titlesize + 2) {
            unsigned x = (cols - (titlesize + 2)) / 2;
            wattron(w, A_BOLD|COLOR_PAIR(Colors_Frame));
            mvwaddch(w, 0, x, '(');
            mvwaddch(w, 0, x + titlesize + 1, ')');
            wattroff(w, A_BOLD|COLOR_PAIR(Colors_Frame));
            mvwaddstr(w, 0, x + 1, title.c_str());
        }
        wnoutrefresh(w);
    }

    if (WINDOW *w = ctx.win.playertitle.get()) {
        mvwaddstr(w, 0, 0, _("Player"));
        if (player) {
            wattron(w, COLOR_PAIR(Colors_Highlight));
            mvwprintw(w, 0, 15, "%s %s", player->name(), player->version());
            wattroff(w, COLOR_PAIR(Colors_Highlight));
        }
        wclrtoeol(w);
        wnoutrefresh(w);
    }
    if (WINDOW *w = ctx.win.emutitle.get()) {
        mvwaddstr(w, 0, 0, _("Emulator"));
        if (player) {
            wattron(w, COLOR_PAIR(Colors_Highlight));
            mvwaddstr(w, 0, 15, player->emulator_name());
            wattroff(w, COLOR_PAIR(Colors_Highlight));
        }
        wclrtoeol(w);
        wnoutrefresh(w);
    }
    if (WINDOW *w = ctx.win.chipcount.get()) {
        mvwaddstr(w, 0, 0, _("Chips"));
        if (player) {
            wattron(w, COLOR_PAIR(Colors_Highlight));
            mvwprintw(w, 0, 15, "%u", player->chip_count());
            wattroff(w, COLOR_PAIR(Colors_Highlight));
            waddstr(w, " * ");
            wattron(w, COLOR_PAIR(Colors_Highlight));
            waddstr(w, player->chip_name());
            wattroff(w, COLOR_PAIR(Colors_Highlight));
        }
        wclrtoeol(w);
        wnoutrefresh(w);
    }
    if (WINDOW *w = ctx.win.cpuratio.get()) {
        mvwaddstr(w, 0, 0, _("CPU"));
        print_bar(w, 0, 15, 15, cpuratio, '*', '-', COLOR_PAIR(Colors_Highlight));
        wclrtoeol(w);
        wnoutrefresh(w);
    }
    if (WINDOW *w = ctx.win.banktitle.get()) {
        mvwaddstr(w, 0, 0, _("Bank"));
        if (player) {
            std::string title;
            const std::string &path = active_bank_file();
            if (path.empty())
                title = _("(default)");
            else
            {
#if !defined(_WIN32)
                size_t pos = path.rfind('/');
#else
                size_t pos = path.find_last_of("/\\");
#endif
                title = (pos != path.npos) ? path.substr(pos + 1) : path;
            }
            wattron(w, COLOR_PAIR(Colors_Highlight));
            mvwaddstr(w, 0, 15, title.c_str());
            wattroff(w, COLOR_PAIR(Colors_Highlight));
        }
        wclrtoeol(w);
        wnoutrefresh(w);
    }
    if (WINDOW *w = ctx.win.chanalloc.get()) {
        mvwaddstr(w, 0, 0, _("Alloc mode"));
        if (player) {
            wattron(w, COLOR_PAIR(Colors_Highlight));
            mvwaddstr(w, 0, 15, player->get_channel_alloc_mode_name());
            wattroff(w, COLOR_PAIR(Colors_Highlight));
        }
        wclrtoeol(w);
        wnoutrefresh(w);
    }

    double channel_volumes[2] = {lvcurrent[0], lvcurrent[1]};
    const char *channel_names[2] = {_("Left"), _("Right")};

    // enables logarithmic view for perceptual volume, otherwise linear.
    //  (better use linear to watch output for clipping)
    const bool logarithmic = false;

    if (WINDOW *w = ctx.win.volumeratio.get()) {
        mvwaddstr(w, 0, 0, _("Volume"));
        wattron(w, COLOR_PAIR(Colors_Highlight));
        mvwprintw(w, 0, 15, "%3d%%\n", ::player_volume);
        wattroff(w, COLOR_PAIR(Colors_Highlight));
        wclrtoeol(w);
        wrefresh(w);
    }

    for (unsigned channel = 0; channel < 2; ++channel) {
        WINDOW *w = ctx.win.volume[channel].get();
        if (!w) continue;

        double vol = channel_volumes[channel];
        if (logarithmic && vol > 0) {
            double db = 20 * log10(vol);
            const double dbmin = -60.0;
            vol = (db - dbmin) / (0 - dbmin);
        }

        mvwaddstr(w, 0, 0, channel_names[channel]);
        print_bar(w, 0, 7, getcols(w) - 7, vol, '*', '-', A_BOLD|COLOR_PAIR(Colors_ActiveVolume));
        wclrtoeol(w);
        wnoutrefresh(w);
    }

    for (unsigned midichannel = 0; midichannel < 16; ++midichannel) {
        WINDOW *w = ctx.win.instrument[midichannel].get();
        if (!w) continue;
        const Program &pgm = channel_map[midichannel];
        mvwprintw(w, 0, 0, "%2u: [", midichannel + 1);
        wattron(w, A_BOLD|COLOR_PAIR(Colors_ProgramNumber));
        wprintw(w, "%3u", pgm.gm);
        wattroff(w, A_BOLD|COLOR_PAIR(Colors_ProgramNumber));
        waddstr(w, "]");

        bool playing = midi_channel_note_count[midichannel] > 0;
        wattron(w, A_BOLD|COLOR_PAIR(Colors_ActiveVolume));
        mvwaddch(w, 0, 11, playing ? '*' : ' ');
        wattroff(w, A_BOLD|COLOR_PAIR(Colors_ActiveVolume));

        const char *name = nullptr;
        Midi_Spec spec = Midi_Spec::GM;

        const int ms_attr[5] = {
            A_BOLD|COLOR_PAIR(Colors_Instrument),
            A_BOLD|COLOR_PAIR(Colors_InstrumentEx),
            A_BOLD|COLOR_PAIR(Colors_InstrumentEx),
            A_BOLD|COLOR_PAIR(Colors_InstrumentEx),
            A_BOLD|COLOR_PAIR(Colors_InstrumentEx),
        };

        if (midichannel == 9) {
            // percussion display, with update rate limit
            if (++ctx.perc_display_cycle == ctx.perc_display_interval) {
                ctx.perc_display_cycle = 0;
                if (unsigned pgm = ::midi_channel_last_note_p1[midichannel]) {
                    --pgm;
                    ctx.have_perc_display_program = true;
                    ctx.perc_display_program = midi_db.perc(pgm);
                }
            }
            if (!ctx.have_perc_display_program)
                name = _("Percussion");
            else {
                spec = ctx.perc_display_program.spec;
                name = ctx.perc_display_program.name;
            }
        }
        else {
            // melodic display
            if (pgm.bank_msb | pgm.bank_lsb) {
                if (const Midi_Program_Ex *ex = midi_db.find_ex(
                        pgm.bank_msb, pgm.bank_lsb, pgm.gm)) {
                    spec = ex->spec;
                    name = ex->name;
                }
            }
            if (!name)
                name = midi_db.inst(pgm.gm);
        }

        int attr = ms_attr[(unsigned)spec];
        wattron(w, attr);
        mvwaddstr(w, 0, 12, name);
        wattroff(w, attr);
        if (spec != Midi_Spec::GM)
            wprintw(w, " [%s]", midi_spec_name(spec));
        wclrtoeol(w);
        wrefresh(w);
    }

    if (WINDOW *w = ctx.win.status.get()) {
        if (!ctx.status_text.empty()) {
            if (!ctx.status_display) {
                wattron(w, COLOR_PAIR(Colors_Select));
                mvwaddstr(w, 0, 0, ctx.status_text.c_str());
                wattroff(w, COLOR_PAIR(Colors_Select));
                ctx.status_start = stc::steady_clock::now();
                ctx.status_display = true;
                wclrtoeol(w);
                wnoutrefresh(w);
            }
            else if (stc::steady_clock::now() - ctx.status_start > stc::seconds(ctx.status_timeout)) {
                ctx.status_text.clear();
                ctx.status_display = false;
                werase(w);
                wnoutrefresh(w);
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

    if (WINDOW *w = ctx.win.keydesc1.get()) {
        static const Key_Description keydesc[] = {
            { "<", _("prev emulator") },
            { ">", _("next emulator") },
            { "[", _("chips -1") },
            { "]", _("chips +1") },
            { "b", _("load bank") },
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

        wclrtoeol(w);
        wnoutrefresh(w);
    }

    if (WINDOW *w = ctx.win.keydesc2.get()) {
        static const Key_Description keydesc[] = {
            { "/", _("volume -1") },
            { "*", _("volume +1") },
            { "p", _("panic") },
            { "c", _("channels") },
            { "q", _("quit") },
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

        wclrtoeol(w);
        wnoutrefresh(w);
    }

    if (WINDOW *w = ctx.win.keydesc3.get()) {
        static const Key_Description keydesc[] = {
            { "a", _("next chanalloc") },
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

        wclrtoeol(w);
        wnoutrefresh(w);
    }
}

static void show_status(TUI_context &ctx, std::string text, unsigned timeout)
{
    ctx.status_text = std::move(text);
    ctx.status_display = false;
    ctx.status_timeout = timeout;
}

static bool handle_anylevel_key(TUI_context &ctx, int key)
{
    switch (key) {
    default:
        return false;
    case 'q': case 'Q':
    case 3:   // console break
        ctx.quit = true;
        return true;
    case KEY_RESIZE:
        resize_term(0, 0);
        erase();
        setup_display(ctx);
        return true;
    }
}

static bool handle_toplevel_key(TUI_context &ctx, int key)
{
    Player *player = ctx.player;
    if (!player)
        return false;

    switch (key) {
    default:
        return false;
    case '<': {
        if (active_emulator_id > 0)
        {
#ifdef ADLJACK_GTK3
            updateIconIfNeeded(active_emulator_id - 1);
#endif
            dynamic_switch_emulator_id(active_emulator_id - 1);
            configFile.beginGroup("synth");
            configFile.setValue("emulator", player->emulator());
            configFile.setValue("pt", active_player_index());
            configFile.endGroup();
            configFile.writeIniFile();
        }
        return true;
    }
    case '>': {
        if (active_emulator_id + 1 < emulator_ids.size()) {
#ifdef ADLJACK_GTK3
            updateIconIfNeeded(active_emulator_id + 1);
#endif
            dynamic_switch_emulator_id(active_emulator_id + 1);
            configFile.beginGroup("synth");
            configFile.setValue("emulator", player->emulator());
            configFile.setValue("pt", active_player_index());
            configFile.endGroup();
            configFile.writeIniFile();
        }
        return true;
    }
    case '[': {
        unsigned nchips = player->chip_count();
        if (nchips > 1) {
            player->dynamic_set_chip_count(nchips - 1);
            configFile.beginGroup("synth");
            configFile.setValue("nchip", player->chip_count());
            configFile.endGroup();
            configFile.writeIniFile();
        }
        return true;
    }
    case ']': {
        unsigned nchips = player->chip_count();
        player->dynamic_set_chip_count(nchips + 1);
        configFile.beginGroup("synth");
        configFile.setValue("nchip", player->chip_count());
        configFile.endGroup();
        configFile.writeIniFile();
        return true;
    }
    case '/': {
        ::player_volume = std::max(volume_min, ::player_volume - 1);
        configFile.beginGroup("synth");
        configFile.setValue("volume", ::player_volume);
        configFile.endGroup();
        configFile.writeIniFile();
        return true;
    }
    case '*': {
        ::player_volume = std::min(volume_max, ::player_volume + 1);
        ::configFile.beginGroup("synth");
        ::configFile.setValue("volume", ::player_volume);
        ::configFile.endGroup();
        ::configFile.writeIniFile();
        return true;
    }
    case 'b':
    case 'B': {
#ifdef ADLJACK_GTK3
        GtkWidget *dialog;
        GtkFileChooser *chooser;
        GtkFileFilter *filter;
        GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
        gint res;

        gtk_main_iteration_do(false);

        dialog = gtk_file_chooser_dialog_new("Load bank file",
                                             NULL,
                                             action,
                                             _("_Cancel"),
                                             GTK_RESPONSE_CANCEL,
                                             _("_OPEN"),
                                             GTK_RESPONSE_ACCEPT,
                                             NULL);
        chooser = GTK_FILE_CHOOSER(dialog);

        filter = gtk_file_filter_new();
        if (active_player().type() == Player_Type::OPL3) {
            gtk_file_filter_set_name(filter, "WOPL bank files");
            gtk_file_filter_add_pattern(filter, "*.wopl");
        } else {
            gtk_file_filter_set_name(filter, "WOPN bank files");
            gtk_file_filter_add_pattern(filter, "*.wopn");
        }
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

        gtk_file_chooser_set_current_folder(chooser, ctx.bank_directory.c_str());
        gtk_file_chooser_set_filename(chooser, active_bank_file().c_str());

        gtk_widget_show_all(dialog);

        res = gtk_dialog_run(GTK_DIALOG(dialog));

        if (res == GTK_RESPONSE_ACCEPT) {
            char *filename;
            char *dirname;

            filename = gtk_file_chooser_get_filename(chooser);
            dirname = gtk_file_chooser_get_current_folder(chooser);

            if (player->dynamic_load_bank(filename)) {
                show_status(ctx, _("Bank loaded!"));
                active_bank_file() = filename;
                update_bank_mtime(ctx);
            }
            else
                show_status(ctx, _("Error loading the bank file."));

            ctx.bank_directory = dirname;
            g_free(filename);
            g_free(dirname);

            configFile.beginGroup("tui");
            configFile.setValue("bank_directory", ctx.bank_directory);
            configFile.endGroup();
            configFile.beginGroup("synth");
            for (unsigned i = 0; i < player_type_count; ++i) {
                std::string bankname_field = "bankfile-" + std::to_string(i);
                configFile.setValue(bankname_field.c_str(), player_bank_file[i]);
            }
            configFile.endGroup();
            configFile.writeIniFile();
        }

        gtk_widget_destroy(dialog);
#else
        erase();

        File_Selection_Options fopts;
        fopts.title = _("Load bank");
        fopts.directory = ctx.bank_directory;
        File_Selector fs(fopts);
        WINDOW_u w(derwin(stdscr, LINES, COLS, 0, 0));
        fs.setup_display(w.get());
        File_Selection_Code code = File_Selection_Code::Continue;
        fs.update();

        void (*idle_proc)(void *) = ctx.idle_proc;
        void *idle_data = ctx.idle_data;

        for (key = getch(); !ctx.quit && !interface_interrupted() &&
                 code == File_Selection_Code::Continue; key = getch()) {
            if (idle_proc)
                idle_proc(idle_data);

            handle_notifications(ctx);

            if (handle_anylevel_key(ctx, key)) {
                if (key == KEY_RESIZE) {
                    w.reset(derwin(stdscr, LINES, COLS, 0, 0));
                    fs.setup_display(w.get());
                }
            }
            else {
                code = fs.key(key);
                fs.update();
            }
            doupdate();
        }

        if (code == File_Selection_Code::Ok) {
            if (player->dynamic_load_bank(fopts.filepath.c_str())) {
                show_status(ctx, _("Bank loaded!"));
                active_bank_file() = fopts.filepath;
                update_bank_mtime(ctx);
            }
            else
                show_status(ctx, _("Error loading the bank file."));
            ctx.bank_directory = fopts.directory;
        }

        configFile.beginGroup("tui");
        configFile.setValue("bank_directory", ctx.bank_directory);
        for (unsigned i = 0; i < player_type_count; ++i) {
            std::string bankname_field = "bankfile-" + std::to_string(i);
            configFile.setValue(bankname_field.c_str(), player_bank_file[i]);
        }
        configFile.endGroup();
        configFile.writeIniFile();

        erase();
#endif
        return true;
    }
    case 'p':
    case 'P': {
        player->dynamic_panic();
        return true;
    }

    case 'c':
    case 'C': {
        erase();

        WINDOW_u w(derwin(stdscr, LINES, COLS, 0, 0));
        Channel_Monitor cm;
        cm.setup_display(w.get());
        cm.setup_player(player);

        TUI_context::Channel_State &state = ctx.channel_state;
        cm.update(state.data.get(), state.size, state.serial);

        void (*idle_proc)(void *) = ctx.idle_proc;
        void *idle_data = ctx.idle_data;

        int code = 1;
        for (key = getch(); !ctx.quit && !interface_interrupted() &&
                 code > 0; key = getch()) {
            if (idle_proc)
                idle_proc(idle_data);

            handle_notifications(ctx);

            if (handle_anylevel_key(ctx, key)) {
                if (key == KEY_RESIZE) {
                    w.reset(derwin(stdscr, LINES, COLS, 0, 0));
                    cm.setup_display(w.get());
                }
            }
            else {
                code = cm.key(key);
                cm.update(state.data.get(), state.size, state.serial);
            }
            doupdate();
#ifdef ADLJACK_GTK3
            gtk_main_iteration_do(false);
#endif
        }

        erase();
        return true;
    }

    case 'a':
    case 'A': {
        int mode = player->get_channel_alloc_mode();
        mode++;
        if (mode >= ADLMIDI_ChanAlloc_Count)
            mode = -1;
        player->dynamic_set_channel_alloc(mode);
        configFile.beginGroup("synth");
        configFile.setValue("chanalloc", mode);
        configFile.endGroup();
        configFile.writeIniFile();
        return true;
    }
    }
}

static void handle_notifications(TUI_context &ctx)
{
    Ring_Buffer *fifo = ::fifo_notify.get();
    if (!fifo)
        return;

    Notify_Header hdr;
    while (fifo->peek(hdr) && fifo->size_used() >= sizeof(hdr) + hdr.size) {
        fifo->discard(sizeof(hdr));
        switch (hdr.type) {
        default:
            assert(false);
            break;
        case Notify_TextInsert: {
            std::unique_ptr<char[]> buf(new char[hdr.size]);
            fifo->get(buf.get(), hdr.size);
            show_status(ctx, std::string(buf.get(), hdr.size));
            break;
        }
        case Notify_Channels: {
            assert((hdr.size & 1) == 0);
            TUI_context::Channel_State &state = ctx.channel_state;
            if (state.size < hdr.size)
                state.data.reset(new char[hdr.size]);
            fifo->get(state.data.get(), hdr.size);
            state.size = hdr.size;
            ++state.serial;
            break;
        }
        }
    }
}

static bool update_bank_mtime(TUI_context &ctx)
{
    Player *player = ctx.player;
    if (!player)
        return false;
    struct stat st;
    const char *path = active_bank_file().c_str();
    Player_Type pt = player->type();
    time_t old_mtime = ctx.bank_mtime[(unsigned)pt];
    time_t new_mtime = (path[0] && !stat(path, &st)) ? st.st_mtime : 0;
    ctx.bank_mtime[(unsigned)pt] = new_mtime;
    return new_mtime && new_mtime != old_mtime;
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

WINDOW *subwin_s(WINDOW *orig, int lines, int cols, int y, int x)
{
    if (lines <= 0 || cols <= 0)
        return nullptr;
    return subwin(orig, lines, cols, y, x);
}

WINDOW *derwin_s(WINDOW *orig, int lines, int cols, int y, int x)
{
    if (lines <= 0 || cols <= 0)
        return nullptr;
    return derwin(orig, lines, cols, y, x);
}

WINDOW *linewin(WINDOW *w, int row, int col)
{
    return derwin_s(w, 1, getcols(w) - col, row, col);
}

int init_color_rgb24(short id, uint32_t value)
{
    unsigned r = (value >> 16) & 0xff;
    unsigned g = (value >> 8) & 0xff;
    unsigned b = value & 0xff;
    return init_color(id, r * 1000 / 0xff, g * 1000 / 0xff, b * 1000 / 0xff);
}

#endif  // defined(ADLJACK_USE_CURSES)
