/* PDCurses */

#include "pdcwin.h"

#include <stdlib.h>
#include <string.h>

#ifdef PDC_WIDE
# include "../common/acsuni.h"
#else
# include "../common/acs437.h"
#endif

DWORD pdc_last_blink;
static bool blinked_off = FALSE;
static bool in_italic = FALSE;

/* position hardware cursor at (y, x) */

void PDC_gotoyx(int row, int col)
{
    COORD coord;

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

    coord.X = col;
    coord.Y = row;

    SetConsoleCursorPosition(pdc_con_out, coord);
}

int _ansi_color_esc(char *esc, short f, short b, attr_t attr)
{
    char *p;
    short tmp, underline;
    bool italic;

    if (f < 16 && !pdc_color[f].mapped)
        f = pdc_curstoansi[f];

    if (b < 16 && !pdc_color[b].mapped)
        b = pdc_curstoansi[b];

    if (attr & A_REVERSE)
    {
        tmp = f;
        f = b;
        b = tmp;
    }
    attr &= SP->termattrs;
    italic = !!(attr & A_ITALIC);
    underline = !!(attr & A_UNDERLINE);

    p = esc + sprintf(esc, "\x1b[");

    if (f != pdc_oldf)
    {
        if (f < 8 && !pdc_color[f].mapped)
            p += sprintf(p, "%d", f + 30);
        else if (f < 16 && !pdc_color[f].mapped)
            p += sprintf(p, "%d", f + 82);
        else if (f < 256 && !pdc_color[f].mapped)
            p += sprintf(p, "38;5;%d", f);
        else
        {
            short red = DIVROUND(pdc_color[f].r * 255, 1000);
            short green = DIVROUND(pdc_color[f].g * 255, 1000);
            short blue = DIVROUND(pdc_color[f].b * 255, 1000);

            p += sprintf(p, "38;2;%d;%d;%d", red, green, blue);
        }

        pdc_oldf = f;
    }

    if (b != pdc_oldb)
    {
        if (strlen(esc) > 2)
            p += sprintf(p, ";");

        if (b < 8 && !pdc_color[b].mapped)
            p += sprintf(p, "%d", b + 40);
        else if (b < 16 && !pdc_color[b].mapped)
            p += sprintf(p, "%d", b + 92);
        else if (b < 256 && !pdc_color[b].mapped)
            p += sprintf(p, "48;5;%d", b);
        else
        {
            short red = DIVROUND(pdc_color[b].r * 255, 1000);
            short green = DIVROUND(pdc_color[b].g * 255, 1000);
            short blue = DIVROUND(pdc_color[b].b * 255, 1000);

            p += sprintf(p, "48;2;%d;%d;%d", red, green, blue);
        }

        pdc_oldb = b;
    }

    if (italic != in_italic)
    {
        if (strlen(esc) > 2)
            p += sprintf(p, ";");

        if (italic)
            p += sprintf(p, "3");
        else
            p += sprintf(p, "23");

        in_italic = italic;
    }

    if (underline != pdc_oldu)
    {
        if (strlen(esc) > 2)
            p += sprintf(p, ";");

        if (underline)
            p += sprintf(p, "4");
        else
            p += sprintf(p, "24");

        pdc_oldu = underline;
    }

    if (strlen(esc) > 2)
    {
        sprintf(p, "m");
        return (int)strlen(esc);
    }
    esc[0] = 0;
    return 0;
}

/* escape sequences need ENABLE_VIRTUAL_TERMINAL_PROCESSING; switch it on
   once and leave it, instead of toggling it around every write */

bool pdc_vt_enabled = FALSE;

static void _ensure_vt(void)
{
    if (!pdc_vt_enabled && !pdc_conemu)
    {
        SetConsoleMode(pdc_con_out, 0x0015);
        pdc_vt_enabled = TRUE;
    }
}

void _set_ansi_color(short f, short b, attr_t attr)
{
    char esc[64];
    int n = _ansi_color_esc(esc, f, b, attr);

    if (n)
    {
        _ensure_vt();
        WriteConsoleA(pdc_con_out, esc, n, NULL, NULL);
    }
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    int j;
    short fore, back;
    bool blink, ansi;

    if (pdc_ansi && (lineno == (SP->lines - 1)) && ((x + len) == SP->cols))
    {
        len--;
        if (len)
            _new_packet(attr, lineno, x, len, srcp);
        pdc_ansi = FALSE;
        _new_packet(attr, lineno, x + len, 1, srcp + len);
        pdc_ansi = TRUE;
        return;
    }

    PDC_pair_content(PAIR_NUMBER(attr), &fore, &back);
    ansi = pdc_ansi || (fore >= 16 || back >= 16);
    blink = (SP->termattrs & A_BLINK) && (attr & A_BLINK);

    if (blink)
    {
        attr &= ~A_BLINK;
        if (blinked_off)
            attr &= ~(A_UNDERLINE | A_RIGHT | A_LEFT);
    }

    if (attr & A_BOLD)
        fore |= 8;
    if (attr & A_BLINK)
        back |= 8;

    if (ansi)
    {
#ifdef PDC_WIDE
        WCHAR buffer[512];
#else
        char buffer[512];
#endif
        for (j = 0; j < len; j++)
        {
            chtype ch = srcp[j];

            if (ch & A_ALTCHARSET && !(ch & 0xff80))
                ch = acs_map[ch & 0x7f];

            if (blink && blinked_off)
                ch = ' ';

            buffer[j] = ch & A_CHARTEXT;
        }

        PDC_gotoyx(lineno, x);
        _set_ansi_color(fore, back, attr);
#ifdef PDC_WIDE
        WriteConsoleW(pdc_con_out, buffer, len, NULL, NULL);
#else
        WriteConsoleA(pdc_con_out, buffer, len, NULL, NULL);
#endif
    }
    else
    {
        CHAR_INFO buffer[512];
        COORD bufSize, bufPos;
        SMALL_RECT sr;
        WORD mapped_attr;

        fore = pdc_curstoreal[fore];
        back = pdc_curstoreal[back];

        if (attr & A_REVERSE)
            mapped_attr = back | (fore << 4);
        else
            mapped_attr = fore | (back << 4);

        if (attr & A_UNDERLINE)
            mapped_attr |= 0x8000; /* COMMON_LVB_UNDERSCORE */
        if (attr & A_LEFT)
            mapped_attr |= 0x0800; /* COMMON_LVB_GRID_LVERTICAL */
        if (attr & A_RIGHT)
            mapped_attr |= 0x1000; /* COMMON_LVB_GRID_RVERTICAL */

        for (j = 0; j < len; j++)
        {
            chtype ch = srcp[j];

            if (ch & A_ALTCHARSET && !(ch & 0xff80))
                ch = acs_map[ch & 0x7f];

            if (blink && blinked_off)
                ch = ' ';

            buffer[j].Attributes = mapped_attr;
            buffer[j].Char.UnicodeChar = ch & A_CHARTEXT;
        }

        bufPos.X = bufPos.Y = 0;
        bufSize.X = len;
        bufSize.Y = 1;

        sr.Top = sr.Bottom = lineno;
        sr.Left = x;
        sr.Right = x + len - 1;

        WriteConsoleOutput(pdc_con_out, buffer, bufSize, bufPos, &sr);
    }
}

/* update the given physical line to look like the corresponding line in
   curscr */

/* Emit a whole span with one console write (plus one mode toggle pair
   when escape sequences are needed) instead of one or more calls per
   attribute run; each call is a round trip to the console host. */

#define PDC_BATCH_CELLS 1024

static bool _batch_line(int lineno, int x, int len, const chtype *srcp)
{
    static CHAR_INFO ci[PDC_BATCH_CELLS];
    static WCHAR wbuf[PDC_BATCH_CELLS + 64 * 64];
    attr_t attr, run_attr = 0;
    short fore = 0, back = 0;
    bool blink = FALSE, ansi = pdc_ansi, have = FALSE;
    WORD mapped_attr = 0;
    int j, n = 0, runs = 0;

    if (len <= 0 || len > PDC_BATCH_CELLS)
        return FALSE;
    if (pdc_ansi && (lineno == (SP->lines - 1)) && ((x + len) == SP->cols))
        return FALSE;

    for (j = 0; j < len; j++)
    {
        attr = srcp[j] & (A_ATTRIBUTES ^ A_ALTCHARSET);
        if (!have || attr != run_attr)
        {
            PDC_pair_content(PAIR_NUMBER(attr), &fore, &back);
            if (fore >= 16 || back >= 16)
                ansi = TRUE;
            run_attr = attr;
            have = TRUE;
            runs++;
        }
    }
    if (runs > 64)
        return FALSE;

    have = FALSE;
    if (ansi)
    {
        char esc[64];
        int k, m = sprintf(esc, "\x1b[%d;%dH", lineno + 1, x + 1);
        for (k = 0; k < m; k++)
            wbuf[n++] = (WCHAR)esc[k];
    }

    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];
        attr = ch & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (!have || attr != run_attr)
        {
            attr_t a = attr;
            PDC_pair_content(PAIR_NUMBER(a), &fore, &back);
            blink = (SP->termattrs & A_BLINK) && (a & A_BLINK);
            if (blink)
            {
                a &= ~A_BLINK;
                if (blinked_off)
                    a &= ~(A_UNDERLINE | A_RIGHT | A_LEFT);
            }
            if (a & A_BOLD)
                fore |= 8;
            if (a & A_BLINK)
                back |= 8;

            if (ansi)
            {
                char esc[64];
                int k, m = _ansi_color_esc(esc, fore, back, a);
                for (k = 0; k < m; k++)
                    wbuf[n++] = (WCHAR)esc[k];
            }
            else
            {
                short f = pdc_curstoreal[fore], b = pdc_curstoreal[back];
                mapped_attr = (a & A_REVERSE) ? (b | (f << 4)) : (f | (b << 4));
                if (a & A_UNDERLINE)
                    mapped_attr |= 0x8000;
                if (a & A_LEFT)
                    mapped_attr |= 0x0800;
                if (a & A_RIGHT)
                    mapped_attr |= 0x1000;
            }
            run_attr = attr;
            have = TRUE;
        }

        if (ch & A_ALTCHARSET && !(ch & 0xff80))
            ch = acs_map[ch & 0x7f];
        if (blink && blinked_off)
            ch = ' ';

        if (ansi)
            wbuf[n++] = (WCHAR)(ch & A_CHARTEXT);
        else
        {
            ci[j].Attributes = mapped_attr;
            ci[j].Char.UnicodeChar = (WCHAR)(ch & A_CHARTEXT);
        }
    }

    if (ansi)
    {
        _ensure_vt();
        WriteConsoleW(pdc_con_out, wbuf, n, NULL, NULL);
    }
    else
    {
        COORD bufSize, bufPos;
        SMALL_RECT sr;

        bufPos.X = bufPos.Y = 0;
        bufSize.X = len;
        bufSize.Y = 1;
        sr.Top = sr.Bottom = lineno;
        sr.Left = x;
        sr.Right = x + len - 1;
        WriteConsoleOutput(pdc_con_out, ci, bufSize, bufPos, &sr);
    }
    return TRUE;
}

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

#ifdef PDC_WIDE
    if (_batch_line(lineno, x, len, srcp))
        return;
#endif

    old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

void PDC_blink_text(void)
{
    int i, j, k;

    if (!(SP->termattrs & A_BLINK))
        blinked_off = FALSE;
    else
        blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line(i, j, k - j, srcp + j);
                j = k;
            }
    }

    PDC_gotoyx(SP->cursrow, SP->curscol);
    pdc_last_blink = GetTickCount();
}
