#include "menu.h"
#include "draw.h"
#include "hid.h"
#include "fs.h"
#include "decryptor/nand.h"


u32 UnmountSd()
{
    u32 pad_state;
    
    DebugClear();
    Debug("Unmounting SD card...");
    DeinitFS();
    Debug("SD is unmounted, you may remove it now.");
    Debug("Put the SD card back in before pressing B!");
    Debug("");
    Debug("(B to return, START to reboot)");
    while (true) {
        pad_state = InputWait();
        if (((pad_state & BUTTON_B) && InitFS()) || (pad_state & BUTTON_START))
            break;
    }
    
    return pad_state;
}

void DrawMenu(MenuInfo* currMenu, u32 index, bool fullDraw, bool subMenu)
{
    bool top_screen = true;
    u32 menublock_x0 = (top_screen) ? 76 : 36;
    u32 menublock_x1 = (top_screen) ? 50 : 10;
    u32 menublock_y0 = 40;
    u32 menublock_y1 = menublock_y0 + currMenu->n_entries * 10;
    
    if (fullDraw) { // draw full menu
        ClearScreenFull(true, !top_screen);
        DrawStringF(menublock_x0, menublock_y0 - 20, top_screen, "%s", currMenu->name);
        DrawStringF(menublock_x0, menublock_y0 - 10, top_screen, "==============================");
        DrawStringF(menublock_x0, menublock_y1 +  0, top_screen, "==============================");
        DrawStringF(menublock_x0, menublock_y1 + 10, top_screen, (subMenu) ? "A: Choose  B: Return" : "A: Choose");
        DrawStringF(menublock_x0, menublock_y1 + 20, top_screen, "SELECT: Unmount SD");
        DrawStringF(menublock_x0, menublock_y1 + 30, top_screen, "START:  Reboot");
        DrawStringF(menublock_x1, SCREEN_HEIGHT - 20, top_screen, "Remaining SD storage space: %llu MiB", RemainingStorageSpace() / 1024 / 1024);
        DrawStringF(menublock_x1, SCREEN_HEIGHT - 30, top_screen, "Game directory: %s", GAME_DIR);
        if (DirOpen(WORK_DIR)) {
            DrawStringF(menublock_x1, SCREEN_HEIGHT - 40, top_screen, "Work directory: %s", WORK_DIR);
            DirClose();
        }
    }
    
    if (!top_screen)
        DrawStringF(10, 10, true, "Selected: %-*.*s", 32, 32, currMenu->entries[index].name);
        
    for (u32 i = 0; i < currMenu->n_entries; i++) { // draw menu entries / selection []
        char* name = currMenu->entries[i].name;
        DrawStringF(menublock_x0, menublock_y0 + (i*10), top_screen, (i == index) ? "[%s]" : " %s ", name);
    }
}

u32 ProcessEntry(MenuEntry* entry)
{
    u32 pad_state;
    u32 res = 0;
    
    // unlock sequence for dangerous features
    if (entry->dangerous) {
        u32 unlockSequenceEmu[] = { BUTTON_LEFT, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_UP, BUTTON_A };
        u32 unlockSequenceSys[] = { BUTTON_LEFT, BUTTON_UP, BUTTON_RIGHT, BUTTON_UP, BUTTON_A };
        u32 unlockLvlMax = 5;
        u32* unlockSequence = (entry->emunand) ? unlockSequenceEmu : unlockSequenceSys;
        u32 unlockLvl = 0;
        DebugClear();
        Debug("You selected \"%s\".", entry->name);
        Debug("This feature writes to the %s.", (entry->emunand) ? "EmuNAND" : "SysNAND");
        Debug("Doing this is potentially dangerous!");
        Debug("");
        Debug("If you wish to proceed, enter:");
        Debug((entry->emunand) ? "<Left>, <Right>, <Down>, <Up>, <A>" : "<Left>, <Up>, <Right>, <Up>, <A>");
        Debug("");
        Debug("(B to return, START to reboot)");
        while (true) {
            ShowProgress(unlockLvl, unlockLvlMax);
            if (unlockLvl == unlockLvlMax)
                break;
            pad_state = InputWait();
            if (!(pad_state & BUTTON_ANY))
                continue;
            else if (pad_state & unlockSequence[unlockLvl])
                unlockLvl++;
            else if (pad_state & (BUTTON_B | BUTTON_START))
                break;
            else if (unlockLvl == 0 || !(pad_state & unlockSequence[unlockLvl-1]))
                unlockLvl = 0;
        }
        ShowProgress(0, 0);
        if (unlockLvl < unlockLvlMax)
            return pad_state;
    }
    
    // execute this entries function
    DebugClear();
    res = (SetNand(entry->emunand) == 0) ? (*(entry->function))(entry->param) : 1;
    Debug("%s: %s!", entry->name, (res == 0) ? "succeeded" : "failed");
    Debug("");
    Debug("Press B to return, START to reboot.");
    while(!(pad_state = InputWait() & (BUTTON_B | BUTTON_START)));
    
    // returns the last known pad_state
    return pad_state;
}

u32 ProcessMenu(MenuInfo* info, u32 n_entries_main)
{
    MenuInfo mainMenu;
    MenuInfo* currMenu = &mainMenu;
    MenuInfo* prevMenu[MENU_MAX_DEPTH];
    u32 prevIndex[MENU_MAX_DEPTH];
    u32 index = 0;
    u32 menuLvl = 0;
    
    // build main menu structure from submenus
    memset(&mainMenu, 0x00, sizeof(MenuInfo));
    for (u32 i = 0; i < n_entries_main && i < MENU_MAX_ENTRIES; i++) {
        mainMenu.entries[i].name = info[i].name;
        mainMenu.entries[i].function = NULL;
        mainMenu.entries[i].param = i;
        mainMenu.entries[i].dangerous = 0;
        mainMenu.entries[i].emunand = 0;
    }
    #ifndef BUILD_NAME
    mainMenu.name = "Decrypt9 Main Menu";
    #else
    mainMenu.name = BUILD_NAME;
    #endif
    mainMenu.n_entries = (n_entries_main > MENU_MAX_ENTRIES) ? MENU_MAX_ENTRIES : n_entries_main;
    DrawMenu(&mainMenu, 0, true, false);
    
    // main processing loop
    while (true) {
        bool full_draw = true;
        u32 pad_state = InputWait();
        if ((pad_state & BUTTON_A) && (currMenu->entries[index].function == NULL)) {
            if (menuLvl < MENU_MAX_DEPTH) {
                prevMenu[menuLvl] = currMenu;
                prevIndex[menuLvl] = index;
                menuLvl++;
            }
            currMenu = info + currMenu->entries[index].param;
            index = 0;
        } else if (pad_state & BUTTON_A) {
            pad_state = ProcessEntry(currMenu->entries + index);
        } else if ((pad_state & BUTTON_B) && (menuLvl > 0)) {
            menuLvl--;
            currMenu = prevMenu[menuLvl];
            index = prevIndex[menuLvl];
        } else if (pad_state & BUTTON_DOWN) {
            index = (index == currMenu->n_entries - 1) ? 0 : index + 1;
            full_draw = false;
        } else if (pad_state & BUTTON_UP) {
            index = (index == 0) ? currMenu->n_entries - 1 : index - 1;
            full_draw = false;
        } else if ((pad_state & BUTTON_R1) && (menuLvl == 1)) {
            if (++currMenu - info >= n_entries_main) currMenu = info;
            index = 0;
        } else if ((pad_state & BUTTON_L1) && (menuLvl == 1)) {
            if (--currMenu < info) currMenu = info + n_entries_main - 1;
            index = 0;
        } else if (pad_state & BUTTON_SELECT) {
            pad_state = UnmountSd();
        } else if (pad_state & BUTTON_X) {
            Screenshot(NULL);
        } else {
            full_draw = false;
        }
        if (pad_state & BUTTON_START) {
            break;
        }
        DrawMenu(currMenu, index, full_draw, menuLvl > 0);
    }
    
    return 0;
}
