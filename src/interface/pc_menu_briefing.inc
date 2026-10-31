/* US 45410066 mission/event briefings: a left click anywhere confirms.
 *
 * Story > mission > difficulty opens the Story briefing, descriptor
 * 0x005238C0 (construct sub_002CAC30, no tick). Arcade League / Challenge >
 * group > event opens the event briefing, descriptor 0x00523870 (construct
 * sub_002D5F40, tick sub_002C9A80). Both pages hold only display items: every
 * row carries item flag 4 (text and custom draw items), so the shared row
 * click finds nothing to hit and a left click was ignored. Keyboard/pad
 * Confirm works through the dispatcher sub_0007A7B0: with no highlighted row
 * (+0x198 == 0) it jumps straight to the menu Confirm callback +0x1BC
 * (0x0007B0C7 -> 0x0007B151 -> call at 0x0007B179). The callbacks are
 * sub_002BEAA0 (Story: start screen 0x005236DC; pushed at 0x002CAC43) and
 * sub_002D5F10 (event: character carousel; stored at 0x002D601C).
 *
 * A fresh left click that is not on a tab, profile prompt or footer prompt
 * (tested first by the caller, so footer Back stays Back and the online
 * leaderboard prompt keeps its own action) is delivered like the footer
 * Select: raw pad Select plus +0xC4/+0xC8 bit 0x10. Right-click Back and the
 * wheel's text scroll are untouched. Scene 101 (front end) only; the
 * callback check excludes any other reuse of the event descriptor. */
#define MENU_STORY_BRIEFING 0x005238C0u
#define MENU_EVENT_BRIEFING 0x00523870u

static int menu_briefing_page(uint32_t menu)
{
    uint32_t descriptor,confirm;
    if((int32_t)WORD_AT(0x0079094Cu)!=101 || !guest_writable(menu,0x23c)) return 0;
    descriptor=WORD_AT(menu+4);confirm=WORD_AT(menu+0x1bc);
    return (descriptor==MENU_STORY_BRIEFING && confirm==0x002BEAA0u) ||
           (descriptor==MENU_EVENT_BRIEFING && confirm==0x002D5F10u);
}

/* A fresh left click off the tabs, profile prompts and footer prompts.
 * Returns 1 when a briefing page consumed it. */
static int menu_briefing_click(MenuMouseDispatch *s,uint32_t menu)
{
    if(!menu_briefing_page(menu)) return 0;
    menu_mouse_press(s,0x80u);
    WORD_AT(menu+0xc4)=(WORD_AT(menu+0xc4)&~0x000F000Fu)|0x100010u;
    WORD_AT(menu+0xc8)|=0x10u;
    s->actions=0; /* the later row hook must not act again */
    menu_mouse.mouse_navigation=1;
    return 1;
}
