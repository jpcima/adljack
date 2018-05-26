//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "winmm_dialog.h"
#if defined(_WIN32)
#include "win_resource.h"
#include "i18n.h"
#include "i18n_util.h"

static INT_PTR CALLBACK winmm_dlgproc(HWND hdlg, unsigned msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        Encoder<Encoding::Local8, Encoding::UTF8> cvt;
        SetWindowTextA(hdlg, cvt.from_string(_("ADLrt for Windows")).c_str());
        SetDlgItemTextA(hdlg, IDC_LABEL, cvt.from_string(_("Select a MIDI input device from the list:")).c_str());
        SetDlgItemTextA(hdlg, IDOK, cvt.from_string(_("OK")).c_str());
        SetDlgItemTextA(hdlg, IDCANCEL, cvt.from_string(_("Cancel")).c_str());

        Audio_Context &ctx = *(Audio_Context *)lp;
        RtMidiIn &midi_client = *ctx.midi_client;
        unsigned nports = midi_client.getPortCount();
        HWND hchoice = GetDlgItem(hdlg, IDC_CHOICE);
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
        if (ctx.have_virtualmidi) {
            int index = SendMessageA(hchoice, CB_ADDSTRING, 0, (LPARAM)_("New virtual MIDI port"));
            SendMessageA(hchoice, CB_SETITEMDATA, index, -2);
        }
#endif
        for (unsigned i = 0; i < nports; ++i) {
            std::string name = midi_client.getPortName(i);
            int index = SendMessageA(hchoice, CB_ADDSTRING, 0, (LPARAM)name.c_str());
            SendMessageA(hchoice, CB_SETITEMDATA, index, i);
        }
        SendMessage(hchoice, CB_SETCURSEL, 0, 0);
        SetFocus(hchoice);
        return 0;
    }
    case WM_COMMAND: {
        switch (wp) {
            case IDOK: {
                HWND hchoice = GetDlgItem(hdlg, IDC_CHOICE);
                int data = -1;
                int index = SendMessage(hchoice, CB_GETCURSEL, 0, 0);
                if (index >= 0)
                    data = SendMessage(hchoice, CB_GETITEMDATA, index, 0);
                EndDialog(hdlg, data);
                break;
            }
            case IDCANCEL:
                EndDialog(hdlg, -1);
                break;
        }
    }
    }
    return FALSE;
}

int dlg_select_midi_port(Audio_Context &ctx)
{
    return DialogBoxParam(
        nullptr, MAKEINTRESOURCE(IDD_DIALOG1), nullptr, &winmm_dlgproc, (LPARAM)&ctx);
}

#endif  // defined(_WIN32)
