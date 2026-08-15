#include "Console.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Sound.h"
#include "Video.h"
#include "Backend/Joypad.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defined in Object/Sonic.c, no header declaration (matches how other
// files in this codebase forward-declare it locally -- see e.g.
// Object/Cannonball.c's own comment on this exact pattern).
int32_t KillSonic(Object *obj, Object *src);

// Defined in Backend/SDL2/System.c and Render.c respectively, same
// no-header forward-declare convention as MegaDrive.c's own
// System_Init/System_Quit. Screenshot deliberately reads the raw
// pre-overlay frame (Render.c's own CRT-blur history buffer) so it never
// contains the console itself.
void System_SetClipboardText(const char *text);
bool Render_SaveScreenshot(const char *path);

bool console_enabled = false;
static bool console_open = false;

// ---------------------------------------------------------------------
// Log ring buffer
// ---------------------------------------------------------------------

static char log_lines[CONSOLE_LOG_LINES][CONSOLE_LINE_LEN];
static int log_count = 0;  // number of lines ever written, saturates at CONSOLE_LOG_LINES for indexing purposes
static int log_head = 0;   // index log_lines[] the NEXT line will be written to

static void LogLine(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_lines[log_head], CONSOLE_LINE_LEN, fmt, ap);
    va_end(ap);
    log_head = (log_head + 1) % CONSOLE_LOG_LINES;
    if (log_count < CONSOLE_LOG_LINES)
        log_count++;
}

const char *Console_GetLogLine(int index_from_bottom) {
    if (index_from_bottom < 0 || index_from_bottom >= log_count)
        return NULL;
    int idx = (log_head - 1 - index_from_bottom + CONSOLE_LOG_LINES * 2) % CONSOLE_LOG_LINES;
    return log_lines[idx];
}

// ---------------------------------------------------------------------
// Input line + command history
// ---------------------------------------------------------------------

static char input_line[CONSOLE_LINE_LEN];
static int input_len = 0;

#define CONSOLE_HISTORY 16
static char history[CONSOLE_HISTORY][CONSOLE_LINE_LEN];
static int history_count = 0;
static int history_next = 0;   // ring buffer write position
static int history_browse = -1; // -1 = not browsing (editing a fresh line)

const char *Console_GetInputLine(void) { return input_line; }
int Console_GetCursor(void) { return input_len; }

static void HistoryPush(const char *line) {
    if (line[0] == '\0')
        return;
    snprintf(history[history_next], CONSOLE_LINE_LEN, "%s", line);
    history_next = (history_next + 1) % CONSOLE_HISTORY;
    if (history_count < CONSOLE_HISTORY)
        history_count++;
}

// ---------------------------------------------------------------------
// Variable registry -- {name, pointer, type}, linear-scanned, same idiom
// as DebugListEntry (Object/DebugList.h) and Sound.c's sound_table[].
// Deliberately a curated, fixed set (the variables this project's own
// debugging sessions actually needed), not a full symbol table -- typing
// a name outside this list just prints "unknown variable", same as an
// unknown command.
// ---------------------------------------------------------------------

typedef enum {
    Var_U8,
    Var_U16,
    Var_U32,
    Var_I16,
} VarType;

typedef struct {
    const char *name;
    void *ptr;
    VarType type;
} ConsoleVar;

static const ConsoleVar console_vars[] = {
    {"limit_top1", &limit_top1, Var_U16},
    {"limit_top2", &limit_top2, Var_U16},
    {"limit_btm1", &limit_btm1, Var_U16},
    {"limit_btm2", &limit_btm2, Var_U16},
    {"limit_left1", &limit_left1, Var_U16},
    {"limit_left2", &limit_left2, Var_U16},
    {"limit_right1", &limit_right1, Var_U16},
    {"limit_right2", &limit_right2, Var_U16},
    {"dle_routine", &dle_routine, Var_U8},
    {"scrpos_x", &scrpos_x.f.u, Var_I16},
    {"scrpos_y", &scrpos_y.f.u, Var_I16},
    {"level_id", &level_id, Var_U16},
    {"rings", &rings, Var_U16},
    {"lives", &lives, Var_U8},
    {"score", &score, Var_U32},
    {"debug_use", &debug_use, Var_U8},
    {"lock_screen", &lock_screen, Var_U8},
    // sonic_x/sonic_y aren't here -- `player` is a runtime pointer (set up
    // after this table would need to exist), not a compile-time constant,
    // so &player->pos... can't appear in a static initializer. Handled as
    // a special case in FindVar/PrintVar/SetVar below instead.
};
#define CONSOLE_VAR_COUNT (int)(sizeof(console_vars) / sizeof(console_vars[0]))

static const ConsoleVar *FindVar(const char *name) {
    // sonic_x/sonic_y: `player` is a runtime pointer (not a compile-time
    // constant), so these can't live in console_vars[]'s static
    // initializer -- resolved into a scratch entry here instead, each
    // call, since player never changes after startup but its pointee's
    // address is only known at runtime.
    static ConsoleVar dynamic_var;
    if (strcmp(name, "sonic_x") == 0) {
        dynamic_var = (ConsoleVar){"sonic_x", &player->pos.l.x.f.u, Var_I16};
        return &dynamic_var;
    }
    if (strcmp(name, "sonic_y") == 0) {
        dynamic_var = (ConsoleVar){"sonic_y", &player->pos.l.y.f.u, Var_I16};
        return &dynamic_var;
    }
    for (int i = 0; i < CONSOLE_VAR_COUNT; i++)
        if (strcmp(console_vars[i].name, name) == 0)
            return &console_vars[i];
    return NULL;
}

static void PrintVar(const ConsoleVar *v) {
    switch (v->type) {
    case Var_U8:  LogLine("%s = %u", v->name, *(uint8_t *)v->ptr); break;
    case Var_U16: LogLine("%s = %u", v->name, *(uint16_t *)v->ptr); break;
    case Var_U32: LogLine("%s = %u", v->name, *(uint32_t *)v->ptr); break;
    case Var_I16: LogLine("%s = %d", v->name, *(int16_t *)v->ptr); break;
    }
}

static void SetVar(const ConsoleVar *v, const char *value_str) {
    long value = strtol(value_str, NULL, 0); // base 0: accepts decimal or 0x-prefixed hex
    switch (v->type) {
    case Var_U8:  *(uint8_t *)v->ptr = (uint8_t)value; break;
    case Var_U16: *(uint16_t *)v->ptr = (uint16_t)value; break;
    case Var_U32: *(uint32_t *)v->ptr = (uint32_t)value; break;
    case Var_I16: *(int16_t *)v->ptr = (int16_t)value; break;
    }
    PrintVar(v);
}

// ---------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------

static int ZoneNameToId(const char *name) {
    static const struct { const char *name; int id; } zones[] = {
        {"ghz", ZoneId_GHZ}, {"lz", ZoneId_LZ}, {"mz", ZoneId_MZ},
        {"slz", ZoneId_SLZ}, {"syz", ZoneId_SYZ}, {"sbz", ZoneId_SBZ},
        {"endz", ZoneId_EndZ}, {"fz", ZoneId_SBZ}, // FZ reuses SBZ's zone slot, act 2 -- see LEVEL_ID(ZoneId_SBZ, 2) elsewhere in this codebase
    };
    for (size_t i = 0; i < sizeof(zones) / sizeof(zones[0]); i++) {
        // Case-insensitive compare -- avoids relying on strcasecmp (not
        // standard C, MSVC needs _stricmp instead), matching this
        // codebase's general preference for small self-contained helpers.
        const char *a = name, *b = zones[i].name;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb)
                goto next_zone;
            a++; b++;
        }
        if (*a == '\0' && *b == '\0')
            return zones[i].id;
    next_zone:;
    }
    return -1;
}

static const struct { const char *name; ObjectId id; } spawn_names[] = {
    {"monitor", ObjId_Monitor},
    {"ring", ObjId_Ring},
    {"crabmeat", ObjId_Crabmeat},
    {"motobug", ObjId_Motobug},
    {"buzzbomber", ObjId_BuzzBomber},
    {"spring", ObjId_Spring},
    {"chopper", ObjId_Chopper},
    {"newtron", ObjId_Newtron},
    {"jaws", ObjId_Jaws},
    {"burrobot", ObjId_Burrobot},
    {"giantring", ObjId_GiantRing},
    {"basaran", ObjId_Basaran},
    {"bomb", ObjId_Bomb},
    {"orbinaut", ObjId_Orbinaut},
    {"caterkiller", ObjId_Caterkiller},
    {"roller", ObjId_Roller},
};

static void CmdWarp(char *args) {
    char zone_name[16] = {0};
    int act = 0;
    if (sscanf(args, "%15s %d", zone_name, &act) < 1) {
        LogLine("usage: warp <zone> <act>");
        return;
    }
    int zone = ZoneNameToId(zone_name);
    if (zone < 0) {
        LogLine("unknown zone '%s' (try ghz/lz/mz/slz/syz/sbz/fz/endz)", zone_name);
        return;
    }
    level_id = LEVEL_ID(zone, act);
    restart = true;
    LogLine("warping to zone %d act %d", zone, act);
}

static void CmdSpawn(char *args) {
    char name[24] = {0};
    if (sscanf(args, "%23s", name) < 1) {
        LogLine("usage: spawn <name> (try 'help' for the list)");
        return;
    }
    for (size_t i = 0; i < sizeof(spawn_names) / sizeof(spawn_names[0]); i++) {
        if (strcmp(spawn_names[i].name, name) != 0)
            continue;
        Object *spawned = FindFreeObj();
        if (spawned == NULL) {
            LogLine("no free object slot");
            return;
        }
        spawned->pos.l.x.f.u = player->pos.l.x.f.u;
        spawned->pos.l.y.f.u = player->pos.l.y.f.u - 32;
        spawned->scratch.u8[0] = 0;
        spawned->type = spawn_names[i].id;
        LogLine("spawned %s at player", name);
        return;
    }
    LogLine("unknown object '%s' (try 'help' for the list)", name);
}

static void CmdGive(char *args) {
    char what[16] = {0};
    int amount = 0;
    if (sscanf(args, "%15s %d", what, &amount) < 2 || strcmp(what, "rings") != 0) {
        LogLine("usage: give rings <n>");
        return;
    }
    rings = (uint16_t)(rings + amount);
    LogLine("rings = %u", rings);
}

static void CmdKill(char *args) {
    (void)args;
    KillSonic(player, player);
    LogLine("killed sonic");
}

static void CmdNoclip(char *args) {
    (void)args;
    // Reuses the game's own existing free-fly debug mode (Object/Sonic.c's
    // DebugMode) rather than building a second, separate collision-bypass
    // system from scratch.
    debug_use = 1;
    LogLine("debug fly mode on (same as the game's own debug mode)");
}

static void CmdClipboard(char *args) {
    (void)args;
    // Oldest-to-newest, one line per log entry -- log_count/GetLogLine
    // index from the bottom (most recent first), so walk it backwards.
    char buf[CONSOLE_LOG_LINES * (CONSOLE_LINE_LEN + 1) + 1];
    buf[0] = '\0';
    for (int i = log_count - 1; i >= 0; i--) {
        strncat(buf, Console_GetLogLine(i), sizeof(buf) - strlen(buf) - 2);
        strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    }
    System_SetClipboardText(buf);
    LogLine("copied %d lines to clipboard", log_count);
}

static void CmdScreencap(char *args) {
    (void)args;
    static int counter = 0;
    char path[64];
    snprintf(path, sizeof(path), "screencap_%04d.ppm", counter++);
    if (Render_SaveScreenshot(path))
        LogLine("saved %s", path);
    else
        LogLine("screenshot failed");
}

static void CmdHelp(char *args);

static const struct { const char *name; void (*fn)(char *args); } commands[] = {
    {"warp", CmdWarp},
    {"spawn", CmdSpawn},
    {"give", CmdGive},
    {"kill", CmdKill},
    {"noclip", CmdNoclip},
    {"clipboard", CmdClipboard},
    {"screencap", CmdScreencap},
    {"help", CmdHelp},
};

static void CmdHelp(char *args) {
    (void)args;
    LogLine("commands: warp <zone> <act>, spawn <name>, give rings <n>,");
    LogLine("          kill, noclip, clipboard, screencap, help");
    LogLine("spawn names: monitor ring crabmeat motobug buzzbomber spring");
    LogLine("  chopper newtron jaws burrobot giantring basaran bomb");
    LogLine("  orbinaut caterkiller roller");
    LogLine("variables (type name to read, 'name = value' to set):");
    LogLine("  sonic_x sonic_y");
    for (int i = 0; i < CONSOLE_VAR_COUNT; i += 3) {
        char line[CONSOLE_LINE_LEN] = "  ";
        for (int j = i; j < i + 3 && j < CONSOLE_VAR_COUNT; j++) {
            strncat(line, console_vars[j].name, CONSOLE_LINE_LEN - strlen(line) - 2);
            strncat(line, " ", CONSOLE_LINE_LEN - strlen(line) - 1);
        }
        LogLine("%s", line);
    }
}

// ---------------------------------------------------------------------
// Command-line execution
// ---------------------------------------------------------------------

static void ExecuteLine(char *line) {
    // Trim leading whitespace
    while (*line == ' ')
        line++;
    if (*line == '\0')
        return;

    LogLine("> %s", line);
    HistoryPush(line);

    // "name = value" (variable set) -- must check this before command
    // dispatch, since variable names and command names share no
    // namespace but '=' unambiguously marks this shape.
    char *eq = strchr(line, '=');
    if (eq != NULL) {
        char name[32] = {0};
        size_t name_len = (size_t)(eq - line);
        while (name_len > 0 && line[name_len - 1] == ' ')
            name_len--;
        if (name_len >= sizeof(name))
            name_len = sizeof(name) - 1;
        memcpy(name, line, name_len);
        name[name_len] = '\0';

        const ConsoleVar *v = FindVar(name);
        if (v != NULL) {
            char *value_str = eq + 1;
            while (*value_str == ' ')
                value_str++;
            SetVar(v, value_str);
            return;
        }
        // Falls through to command/bare-name handling below if the LHS
        // isn't a known variable -- e.g. so "warp=foo" (unlikely, but not
        // worth special-casing out) still gets a sensible error.
    }

    // Split into command word + rest-of-line args.
    char cmd[24] = {0};
    int consumed = 0;
    sscanf(line, "%23s%n", cmd, &consumed);
    char *args = line + consumed;
    while (*args == ' ')
        args++;

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(commands[i].name, cmd) == 0) {
            commands[i].fn(args);
            return;
        }
    }

    // Bare variable name (no '=') -- print its value.
    const ConsoleVar *v = FindVar(cmd);
    if (v != NULL) {
        PrintVar(v);
        return;
    }

    LogLine("unknown command or variable '%s' (try 'help')", cmd);
}

// ---------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------

bool Console_IsOpen(void) { return console_open; }

void Console_Toggle(void) {
    if (!console_enabled)
        return;
    console_open = !console_open;
    Joypad_SetTextInputMode(console_open);
    if (console_open)
        LogLine("-- console open (type 'help') --");
}

void Console_HandleText(const char *text) {
    for (const char *c = text; *c != '\0' && input_len < CONSOLE_LINE_LEN - 1; c++)
        input_line[input_len++] = *c;
    input_line[input_len] = '\0';
}

void Console_HandleKey(ConsoleKey key) {
    switch (key) {
    case ConsoleKey_Backspace:
        if (input_len > 0)
            input_line[--input_len] = '\0';
        break;
    case ConsoleKey_Enter:
        ExecuteLine(input_line);
        input_len = 0;
        input_line[0] = '\0';
        history_browse = -1;
        break;
    case ConsoleKey_Up:
        if (history_count == 0)
            break;
        if (history_browse == -1)
            history_browse = 0;
        else if (history_browse < history_count - 1)
            history_browse++;
        {
            int idx = (history_next - 1 - history_browse + CONSOLE_HISTORY * 2) % CONSOLE_HISTORY;
            snprintf(input_line, CONSOLE_LINE_LEN, "%s", history[idx]);
            input_len = (int)strlen(input_line);
        }
        break;
    case ConsoleKey_Down:
        if (history_browse == -1)
            break;
        history_browse--;
        if (history_browse == -1) {
            input_line[0] = '\0';
            input_len = 0;
        } else {
            int idx = (history_next - 1 - history_browse + CONSOLE_HISTORY * 2) % CONSOLE_HISTORY;
            snprintf(input_line, CONSOLE_LINE_LEN, "%s", history[idx]);
            input_len = (int)strlen(input_line);
        }
        break;
    }
}

// Mirrors GM_Level.c's own PauseGame(): a blocking do/while that keeps
// calling WaitForVBla() (rendering + Input_HandleEvents + Audio_Update,
// same chain any normal frame goes through) until closed, with
// Sound_Pause/Sound_Resume bracketing it so music genuinely freezes too --
// "opening acts like Ctrl+C, closing acts like continue" for audio as
// well as gameplay, matching your own framing of this feature.
void ConsoleUpdate(void) {
    if (!console_enabled || !console_open)
        return;
    Sound_Pause();
    do {
        vbla_routine = 0x08;
        WaitForVBla();
    } while (console_open);
    Sound_Resume();
}
