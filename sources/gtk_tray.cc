//          Copyright Novichkov Vitaliy Dmitriyevich 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <atomic>
#include "tui.h"
#include "tui_channels.h"
#include "i18n.h"
#include "common.h"
#include "player.h"

#include <gtk/gtk.h>
#include <asm/ioctls.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>

static GtkStatusIcon *s_tray_icon = nullptr;
static GMutex mutex_interface;
static GtkStatusIcon *create_tray_icon();
static void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data);
static std::atomic<bool> s_running(false);


void adl_gtk_init(int *argc, char ***argv)
{
    gtk_init(argc, argv);
    s_running = true;
}

void adl_gtk_quit()
{
    s_running = false;
}

gpointer adl_gtk_gui_runner(gpointer data)
{
    while(s_running) {
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
        // timeout(1);
    }

    return NULL;
}

void adl_gtk_init_icon(TUI_contextP ctx)
{
    g_mutex_lock(&mutex_interface);
    s_tray_icon = create_tray_icon();

    if (active_player().type() == Player_Type::OPL3) {
        gtk_status_icon_set_from_icon_name(s_tray_icon, "adljack");
    } else {
        gtk_status_icon_set_from_icon_name(s_tray_icon, "opnjack");
    }

    g_signal_connect(G_OBJECT(s_tray_icon), "popup-menu", G_CALLBACK(tray_icon_on_menu), ctx);
    g_mutex_unlock(&mutex_interface);

    g_thread_new("thread", adl_gtk_gui_runner, NULL);
}

void adk_gtk_hide_icon()
{
    s_running = false;
    g_mutex_lock(&mutex_interface);
    gtk_status_icon_set_visible(s_tray_icon, FALSE);
    g_mutex_unlock(&mutex_interface);
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

void adl_gtk_updateIconIfNeeded(int newEmu)
{
    g_mutex_lock(&mutex_interface);

    const Emulator_Id old_id = emulator_ids[active_emulator_id];
    const Emulator_Id new_id  = emulator_ids[newEmu];

    if (old_id.player != new_id.player) {
        if (new_id.player == Player_Type::OPL3) {
            gtk_status_icon_set_from_icon_name(s_tray_icon, "adljack");
        } else {
            gtk_status_icon_set_from_icon_name(s_tray_icon, "opnjack");
        }
    }

    g_mutex_unlock(&mutex_interface);
}

void adl_gtk_bank_select_dialogue(TUI_contextP ctx)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileFilter *filter;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;
    Player *player = handle_ctx_get_player(ctx);

    if (!player)
        return;

    g_mutex_lock(&mutex_interface);

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

    gtk_file_chooser_set_current_folder(chooser, handle_ctx_bank_directory(ctx).c_str());
    gtk_file_chooser_set_filename(chooser, active_bank_file().c_str());

    gtk_widget_show_all(dialog);

    res = gtk_dialog_run(GTK_DIALOG(dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        char *dirname;

        filename = gtk_file_chooser_get_filename(chooser);
        dirname = gtk_file_chooser_get_current_folder(chooser);

        if (player->dynamic_load_bank(filename)) {
            if (player->type() == Player_Type::OPL3) {
                ::player_opl_embedded_bank_id = -1;
            }
            show_status_p(ctx, _("Bank loaded!"));
            active_bank_file() = filename;
            update_bank_mtime_p(ctx);
        }
        else
            show_status_p(ctx, _("Error loading the bank file."));

        handle_ctx_bank_directory(ctx) = dirname;
        g_free(filename);
        g_free(dirname);

        configFile.beginGroup("tui");
        configFile.setValue("bank_directory", handle_ctx_bank_directory(ctx));
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

    g_mutex_unlock(&mutex_interface);
}

//void tray_icon_on_click(GtkStatusIcon *status_icon, gpointer user_data)
//{
//    //
//}

static void tray_icon_open_bank(TUI_context *ctx)
{
    adl_gtk_bank_select_dialogue(ctx);
    // handle_toplevel_key_p(ctx, (int)'b');
}

static void tray_icon_set_opl_embedded_bank(intptr_t bank_id)
{
    ::player_opl_embedded_bank_id = bank_id;
    active_player().dynamic_set_embedded_bank(active_bank_file().c_str(), ::player_opl_embedded_bank_id);
    configFile.beginGroup("synth");
    configFile.setValue("opl-embedded-bank", ::player_opl_embedded_bank_id);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_quit(TUI_context *ctx)
{
    handle_anylevel_key_p(ctx, (int)'q');
}

static void tray_icon_quickVolume(intptr_t volume)
{
    ::player_volume = std::min(volume_max, std::max(volume_min, (int)volume));
    configFile.beginGroup("synth");
    configFile.setValue("volume", ::player_volume);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_chanAlloc(intptr_t mode)
{
    active_player().dynamic_set_channel_alloc((int)mode);
    configFile.beginGroup("synth");
    configFile.setValue("chanalloc", mode);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_chipsNum(intptr_t chips)
{
    active_player().dynamic_set_chip_count((unsigned)chips);
    configFile.beginGroup("synth");
    configFile.setValue("nchip", (unsigned)chips);
    configFile.endGroup();
    configFile.writeIniFile();
}

static void tray_icon_switchEmulator(Emulator_Id *e)
{
    auto emulator_id_pos = std::find(emulator_ids.begin(), emulator_ids.end(), *e);
    if (emulator_id_pos == emulator_ids.end()) {
        return;
    }

    int newId = std::distance(emulator_ids.begin(), emulator_id_pos);

    adl_gtk_updateIconIfNeeded(newId);
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
        GtkWidget *itemSelectBank = gtk_menu_item_new_with_label("Select bank file...");
        gtk_widget_show(itemSelectBank);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), itemSelectBank);
        g_signal_connect_swapped(G_OBJECT(itemSelectBank), "activate",
                                 G_CALLBACK(tray_icon_open_bank), user_data);
    }
    // ------------------------------------------------------------------------------------------
    if(active_emulator_id >= 0 && active_emulator_id < emulator_ids.size() && emulator_ids[active_emulator_id].player == Player_Type::OPL3)
    {
        GtkWidget *embeddedBank = gtk_menu_item_new_with_label("Use embedded OPL bank");
        GtkWidget *embeddedBankSubMenu = gtk_menu_new();

        auto *bankUseCustom = gtk_check_menu_item_new_with_label(active_bank_file().empty() ? "<Default bank>" : ("Custom bank: " + active_bank_file()).c_str());
        gtk_menu_shell_append(GTK_MENU_SHELL(embeddedBankSubMenu), bankUseCustom);

        if (active_opl_embedded_bank() == -1) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(bankUseCustom), TRUE);
        }

        g_signal_connect_swapped(G_OBJECT(bankUseCustom), "activate", G_CALLBACK(tray_icon_set_opl_embedded_bank), (void*)(intptr_t)-1);

        GtkWidget *volSep2 = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(embeddedBankSubMenu), volSep2);

        for(int i = 0; i < adl_getBanksCount(); ++i)
        {
            const char *bankName = adl_getBankNames()[i];
            auto *bankNameItem = gtk_check_menu_item_new_with_label(bankName);
            gtk_menu_shell_append(GTK_MENU_SHELL(embeddedBankSubMenu), bankNameItem);
            if (active_opl_embedded_bank() == i) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(bankNameItem), TRUE);
            }
            g_signal_connect_swapped(G_OBJECT(bankNameItem), "activate", G_CALLBACK(tray_icon_set_opl_embedded_bank), (void*)(intptr_t)i);
        }

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(embeddedBank), embeddedBankSubMenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), embeddedBank);
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
        g_signal_connect_swapped(G_OBJECT(quickVolume50), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)50);
        g_signal_connect_swapped(G_OBJECT(quickVolume100), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)100);
        g_signal_connect_swapped(G_OBJECT(quickVolume150), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)150);
        g_signal_connect_swapped(G_OBJECT(quickVolume200), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)200);
        g_signal_connect_swapped(G_OBJECT(quickVolume250), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)250);
        g_signal_connect_swapped(G_OBJECT(quickVolume300), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)300);
        g_signal_connect_swapped(G_OBJECT(quickVolume350), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)350);
        g_signal_connect_swapped(G_OBJECT(quickVolume400), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)400);
        g_signal_connect_swapped(G_OBJECT(quickVolume450), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)450);
        g_signal_connect_swapped(G_OBJECT(quickVolume500), "activate", G_CALLBACK(tray_icon_quickVolume), (void*)(intptr_t)500);
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
            if (i == active_emulator_id) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(emuItem), TRUE);
            }

            g_signal_connect_swapped(G_OBJECT(emuItem), "activate",
                                     G_CALLBACK(tray_icon_switchEmulator), &e);
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

        g_signal_connect_swapped(G_OBJECT(chanAllocAuto), "activate", G_CALLBACK(tray_icon_chanAlloc), (void*)(intptr_t)-1);
        g_signal_connect_swapped(G_OBJECT(chanAllocOffDelay), "activate", G_CALLBACK(tray_icon_chanAlloc), (void*)(intptr_t)0);
        g_signal_connect_swapped(G_OBJECT(chanAllocSameInst), "activate", G_CALLBACK(tray_icon_chanAlloc), (void*)(intptr_t)1);
        g_signal_connect_swapped(G_OBJECT(chanAllocAnyFree), "activate", G_CALLBACK(tray_icon_chanAlloc), (void*)(intptr_t)2);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), chanAlloc);
    }
    // ------------------------------------------------------------------------------------------
    {
        GtkWidget *numChips = gtk_menu_item_new_with_label("Number of chips");
        GtkWidget *numChipsSubMenu = gtk_menu_new();
        static const char *const numbers[] = {
            "..",
            "1 chip", "2 chips", "3 chips", "4 chips", "5 chips", "6 chips",
            "7 chips", "8 chips", "9 chips", "10 chips", "11 chips", "12 chips"
        };

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(numChips), numChipsSubMenu);

        for(int i = 1; i <= 12; ++i)
        {
            auto *chipsItem = gtk_check_menu_item_new_with_label(numbers[i]);
            gtk_menu_shell_append(GTK_MENU_SHELL(numChipsSubMenu), chipsItem);
            if(active_player().chip_count() == i) {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(chipsItem), TRUE);
            }
            g_signal_connect_swapped(G_OBJECT(chipsItem), "activate", G_CALLBACK(tray_icon_chipsNum), (void*)(intptr_t)i);
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), numChips);
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

