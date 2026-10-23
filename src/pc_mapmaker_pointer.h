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
#endif
