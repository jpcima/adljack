//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define ADLJACK_NO_PDC_I18N_MACROS
#include "i18n.h"
#include <locale>
#include <codecvt>
#include <unordered_map>
#include <stdio.h>
#include <string.h>

#if defined(ADLJACK_I18N)

#if defined(PDCURSES) && !defined(PDC_WIDE)

static const unsigned char low_ucs4_to_cp437[256] = {
    0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xad, 0x9b, 0x9c, 0, 0x9d, 0, 0x15, 0, 0, 0xa6, 0xae, 0xaa, 0, 0, 0, 0xf8, 0xf1, 0xfd, 0, 0, 0xe6, 0x14, 0xfa, 0, 0, 0xa7, 0xaf, 0xac, 0xab, 0, 0xa8, 0, 0, 0, 0, 0x8e, 0x8f, 0x92, 0x80, 0, 0x90, 0, 0, 0, 0, 0, 0, 0, 0xa5, 0, 0, 0, 0, 0x99, 0, 0, 0, 0, 0, 0x9a, 0, 0, 0, 0x85, 0xa0, 0x83, 0, 0x84, 0x86, 0x91, 0x87, 0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b, 0, 0xa4, 0x95, 0xa2, 0x93, 0, 0x94, 0xf6, 0, 0x97, 0xa3, 0x96, 0x81, 0, 0, 0x98,
};
static const std::unordered_map<char32_t, unsigned char> high_ucs4_to_cp437 = {
    {9786, 1}, {9787, 2}, {9829, 3}, {9830, 4}, {9827, 5}, {9824, 6}, {8226, 7}, {9688, 8}, {9675, 9}, {9689, 10}, {9794, 11}, {9792, 12}, {9834, 13}, {9835, 14}, {9788, 15}, {9654, 16}, {9664, 17}, {8597, 18}, {8252, 19}, {9644, 22}, {8616, 23}, {8593, 24}, {8595, 25}, {8594, 26}, {8592, 27}, {8735, 28}, {8596, 29}, {9650, 30}, {9660, 31}, {8962, 127}, {8359, 158}, {402, 159}, {8976, 169}, {9617, 176}, {9618, 177}, {9619, 178}, {9474, 179}, {9508, 180}, {9569, 181}, {9570, 182}, {9558, 183}, {9557, 184}, {9571, 185}, {9553, 186}, {9559, 187}, {9565, 188}, {9564, 189}, {9563, 190}, {9488, 191}, {9492, 192}, {9524, 193}, {9516, 194}, {9500, 195}, {9472, 196}, {9532, 197}, {9566, 198}, {9567, 199}, {9562, 200}, {9556, 201}, {9577, 202}, {9574, 203}, {9568, 204}, {9552, 205}, {9580, 206}, {9575, 207}, {9576, 208}, {9572, 209}, {9573, 210}, {9561, 211}, {9560, 212}, {9554, 213}, {9555, 214}, {9579, 215}, {9578, 216}, {9496, 217}, {9484, 218}, {9608, 219}, {9604, 220}, {9612, 221}, {9616, 222}, {9600, 223}, {945, 224}, {946, 225}, {915, 226}, {960, 227}, {931, 228}, {963, 229}, {964, 231}, {934, 232}, {920, 233}, {937, 234}, {948, 235}, {8734, 236}, {966, 237}, {949, 238}, {8745, 239}, {8801, 240}, {8805, 242}, {8804, 243}, {8992, 244}, {8993, 245}, {8776, 247}, {8729, 249}, {8730, 251},
};

static char char_to_cp437(char32_t ucs4)
{
    char cp437;
    if (ucs4 < 256)
        cp437 = low_ucs4_to_cp437[ucs4];
    else {
        auto it = high_ucs4_to_cp437.find(ucs4);
        cp437 = (it != high_ucs4_to_cp437.end()) ? (char)it->second : '\0';
    }
    return cp437 ? cp437 : ' ';
}

static std::string string_to_cp437(
    const char *input, size_t input_size = (size_t)-1)
{
    if (input_size == (size_t)-1)
        input_size = strlen(input);

    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
    std::u32string ucs4 = cvt.from_bytes(input, input + input_size);
    std::string cp437;
    cp437.reserve(2 * ucs4.size());

    for (char32_t ch : ucs4) {
        switch (ch) {
        default:
            cp437.push_back(char_to_cp437(ch)); break;
        case U'œ':  // ligature not in cp437
            cp437.append("oe"); break;
        }
    }

    return cp437;
}

int pdc_ext_mvprintw(int y, int x, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = move(y, x);
    if (ret == OK)
        ret = pdc_ext_vw_printw(stdscr, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = wmove(w, y, x);
    if (ret == OK)
        ret = pdc_ext_vw_printw(w, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_printw(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = pdc_ext_vw_printw(stdscr, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_vw_printw(WINDOW *w, const char *fmt, va_list ap)
{
    char buf[256];
    if (vsnprintf(buf, 256, fmt, ap) == -1)
        return ERR;
    buf[sizeof(buf) - 1] = '\0';
    std::string lat1 = string_to_cp437(buf, strlen(buf));
    return waddstr(w, lat1.c_str());
}

int pdc_ext_vwprintw(WINDOW *w, const char *fmt, va_list ap)
{
    return pdc_ext_vw_printw(w, fmt, ap);
}

int pdc_ext_wprintw(WINDOW *w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = pdc_ext_vw_printw(w, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_mvaddch(int y, int x, char32_t ch)
{
    return mvaddch(y, x, char_to_cp437(ch));
}

int pdc_ext_mvwaddch(WINDOW *w, int y, int x, char32_t ch)
{
    return mvwaddch(w, y, x, char_to_cp437(ch));
}

int pdc_ext_waddch(WINDOW *w, char32_t ch)
{
    return waddch(w, char_to_cp437(ch));
}

int pdc_ext_addch(char32_t ch)
{
    return addch(char_to_cp437(ch));
}

int pdc_ext_addstr(const char *str)
{
    return addstr(string_to_cp437(str).c_str());
}

int pdc_ext_addnstr(const char *str, int n)
{
    return addstr(string_to_cp437(str, n).c_str());
}

int pdc_ext_waddstr(WINDOW *w, const char *str)
{
    return waddstr(w, string_to_cp437(str).c_str());
}

int pdc_ext_waddnstr(WINDOW *w, const char *str, int n)
{
    return waddstr(w, string_to_cp437(str, n).c_str());
}

int pdc_ext_mvaddstr(int y, int x, const char *str)
{
    return mvaddstr(y, x, string_to_cp437(str).c_str());
}

int pdc_ext_mvaddnstr(int y, int x, const char *str, int n)
{
    return mvaddstr(y, x, string_to_cp437(str, n).c_str());
}

int pdc_ext_mvwaddstr(WINDOW *w, int y, int x, const char *str)
{
    return mvwaddstr(w, y, x, string_to_cp437(str).c_str());
}

int pdc_ext_mvwaddnstr(WINDOW *w, int y, int x, const char *str, int n)
{
    return mvwaddstr(w, y, x, string_to_cp437(str, n).c_str());
}
#endif  // defined(PDCURSES) && !defined(PDC_WIDE)

#endif  // defined(ADLJACK_I18N)
