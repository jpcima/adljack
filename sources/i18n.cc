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

#if defined(PDCURSES)

static const unsigned char low_ucs4_to_cp437[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 173, 155, 156, 0, 157, 0, 21, 0, 0, 166, 174, 170, 0, 0, 0, 248, 241, 253, 0, 0, 230, 20, 250, 0, 0, 167, 175, 172, 171, 0, 168, 0, 0, 0, 0, 142, 143, 146, 128, 0, 144, 0, 0, 0, 0, 0, 0, 0, 165, 0, 0, 0, 0, 153, 0, 0, 0, 0, 0, 154, 0, 0, 0, 133, 160, 131, 0, 132, 134, 145, 135, 138, 130, 136,
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
    std::string ext;
    ext.reserve(ucs4.size());
    for (char32_t ch : ucs4)
        ext.push_back(char_to_cp437(ch));

    return ext;
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
#endif  // defined(PDCURSES)

#endif  // defined(ADLJACK_I18N)
