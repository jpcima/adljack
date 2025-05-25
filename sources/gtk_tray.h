//          Copyright Novichkov Vitaliy Dmitriyevich 2025.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

typedef struct TUI_context* TUI_contextP;

void adl_gtk_init(int *argc, char ***argv);
void adl_gtk_init_icon(TUI_contextP ctx);
void adl_gtk_quit();
void adk_gtk_hide_icon();
void adk_gtk_sync_icon();
void adl_gtk_do_events();

void adl_gtk_updateIconIfNeeded(int newEmu);

void adl_gtk_bank_select_dialogue(TUI_contextP ctx);
