/* US 45410066 MapMaker map browser (main menu > MapMaker > Load map).
 *
 * The browser is an ordinary menu, descriptor 0x0052EA88 (construct
 * sub_0032BEA0, tick sub_0032BDD0). sub_0032BEA0 adds one type-7 row per file
 * of the enumerated list [0x00778D38] (count at +8, 0x6C-byte entries at
 * +0xC filled by sub_00190720); each row's id (+0x14) is its list index.
 * The tick loads the highlighted row (+0x198, index +0x18C) when the active
 * menu's input word +0xC4 has the Confirm bit 0x10 (sub_0032BC90 ->
 * sub_0032BA90), and skips a row flagged +0x28 & 1.
 *
 * The dispatcher sub_0007A7B0 calls that tick (menu+0xC, 0x0007AD76) BEFORE
 * the row layout hook sub_00073750 (0x0007ADC5), where the shared row click
 * sets its Confirm edge, so an ordinary row click only highlighted the map
 * and E/A was needed to load it. Here, in the input hook (sub_00073870, before
 * the tick), a fresh left click on a row highlights it (sub_00076EF0), and a
 * click on the row already highlighted is Confirm, delivered like the footer
 * prompts: raw pad A plus +0xC4/+0xC8 bit 0x10. Hover and the wheel keep the
 * shared row behaviour. Scene 101 (front end) only; the editor's own copy of
 * the list (scene 102) keeps the MapMaker pointer's row press. */
#define MENU_MAP_BROWSER 0x0052EA88u
#define MENU_MAP_FILES 0x00778D38u

static uint32_t menu_map_browser_row(uint32_t menu,float x,float y)
{
    uint32_t files,count,item;
    if((int32_t)WORD_AT(0x0079094Cu)!=101 || !guest_writable(menu,0x23c) ||
       WORD_AT(menu+4)!=MENU_MAP_BROWSER) return 0;
    files=WORD_AT(MENU_MAP_FILES);
    if(!guest_writable(files,0xc)) return 0;
    count=WORD_AT(files+8);
    if(!(x>=FLOAT_AT(menu+0xe4) && x<FLOAT_AT(menu+0xec) &&
         y>=FLOAT_AT(menu+0xe8) && y<FLOAT_AT(menu+0xf0))) return 0;
    item=WORD_AT(menu+0x178);
    for(unsigned n=0;item && n<256 && guest_writable(item,0x10c);n++,item=WORD_AT(item+4)) {
        uint32_t type=WORD_AT(item);
        if((type==4 || type==7 || type==8) && !(WORD_AT(item+0x28)&5u) &&
           WORD_AT(item+0x14)<count &&
           x>=FLOAT_AT(item+0x78) && x<FLOAT_AT(item+0x80) &&
           y>=FLOAT_AT(item+0x7c) && y<FLOAT_AT(item+0x84)) return item;
    }
    return 0;
}

/* A fresh left click at UI (x, y). Returns 1 when the browser consumed it. */
static int menu_map_browser_click(MenuMouseDispatch *s,uint32_t menu,float x,float y)
{
    uint32_t item=menu_map_browser_row(menu,x,y);
    if(!item) return 0;
    if(WORD_AT(menu+0x198)==item) {
        menu_mouse_press(s,0x80u);
        WORD_AT(menu+0xc4)=(WORD_AT(menu+0xc4)&~0x000F000Fu)|0x100010u;
        WORD_AT(menu+0xc8)|=0x10u;
    } else guest_call(sub_00076EF0,2,menu,item);
    s->actions=0; /* the later row hook must not click it again */
    menu_mouse.mouse_navigation=1;
    return 1;
}
