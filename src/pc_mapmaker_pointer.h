/* MapMaker editor mouse click policy (host side, no guest memory).
 *
 * While the editor screen owns the game pointer (see pc_mapmaker_pointer.inc),
 * a held left button is reported as the held A button, so the editor's own
 * select/place/drag handling applies. A button that is already held when
 * ownership starts or ends (the click that closed a list, or entered the
 * editor) is ignored until it is released. Clicks on editor menu rows hold A
 * for that press (pc_mapmaker_row_press). */
#ifndef TSFP_PC_MAPMAKER_POINTER_H
#define TSFP_PC_MAPMAKER_POINTER_H

typedef struct PcMapMakerClick { int active, blocked, row; } PcMapMakerClick;

/* A click on a row of an editor menu (pause menu, category list): the editor
 * reads A itself rather than the menu engine's confirm, so hold A for this
 * press until the button is released. */
static inline void pc_mapmaker_row_press(PcMapMakerClick *s) { s->row = 1; }

static inline int pc_mapmaker_click(PcMapMakerClick *s, int active, int left_down)
{
    active = !!active; left_down = !!left_down;
    if (active != s->active) s->blocked = left_down;
    s->active = active;
    if (!left_down) s->blocked = s->row = 0;
    /* A row press never carries onto the editor screen (list closed under it). */
    return (active && !s->blocked && left_down) || (s->row && !active && left_down);
}
/* In the editor, Start (Escape) opens the pause menu from the editor screen,
 * Back (Tab) toggles the 3D view there, and Back leaves an open menu. So
 * Escape acts as Back while a menu is open and keeps its own binding (Start)
 * on the editor screen. The choice is made when Escape goes down and kept
 * until it is released, so the press that opens the pause menu cannot then
 * close it. Returns 1 while Escape currently acts as Back. */
typedef struct PcMapMakerEscape { int was, back; } PcMapMakerEscape;
static inline int pc_mapmaker_escape_back(PcMapMakerEscape *s, int escape_down, int menu_open)
{
    escape_down = !!escape_down;
    if (escape_down && !s->was) s->back = !!menu_open;
    s->was = escape_down;
    return escape_down && s->back;
}

/* Adjusted down state for one polled key while Escape acts as Back. */
static inline int pc_mapmaker_key(int editor, int key, int back_key, int down, int escape_back)
{
    enum { ESCAPE = 0x1B };
    if (!editor || !back_key || back_key == ESCAPE) return down;
    if (key == ESCAPE) return escape_back ? 0 : down;
    if (key == back_key) return down || escape_back;
    return down;
}

/* In the editor the right trigger raises the floor, but its usual key is the
 * left mouse button, which the editor uses as A. T acts as that trigger's
 * first key there instead (the left trigger, G by default, lowers it). */
#define PC_MAPMAKER_FLOOR_UP_KEY 'T'
static inline int pc_mapmaker_trigger_key(int editor, int key, int trigger_key, int down, int t_down)
{
    if (!editor || !trigger_key || key != trigger_key) return down;
    return down || t_down;
}
#endif
