#include "ai_pipe.h"

#include "Backend/Joypad.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int pipe_fd = -1;
static uint8_t pipe_state = 0;
static char pipe_line[256];
static size_t pipe_line_len = 0;

// One-letter tokens, space-separated, one line per frame you want to change
// what's held: U D L R A B C S (up/down/left/right/A/B/C/start). An empty
// line releases everything.
static uint8_t ParseButtonLine(const char *line) {
    uint8_t state = 0;
    for (const char *p = line; *p; p++) {
        switch (*p) {
        case 'u': case 'U': state |= JPAD_UP;    break;
        case 'd': case 'D': state |= JPAD_DOWN;  break;
        case 'l': case 'L': state |= JPAD_LEFT;  break;
        case 'r': case 'R': state |= JPAD_RIGHT; break;
        case 'a': case 'A': state |= JPAD_A;     break;
        case 'b': case 'B': state |= JPAD_B;     break;
        case 'c': case 'C': state |= JPAD_C;     break;
        case 's': case 'S': state |= JPAD_START; break;
        case ' ': case '\t': case '\r': break;
        default:
            fprintf(stderr, "SonicDemoRecord: ignoring unknown button token '%c'\n", *p);
        }
    }
    return state;
}

void AIPipe_Open(const char *path) {
    if (mkfifo(path, 0600) != 0 && errno != EEXIST) {
        fprintf(stderr, "SonicDemoRecord: couldn't create pipe '%s': %s\n", path, strerror(errno));
        exit(1);
    }

    // O_NONBLOCK on open lets this succeed immediately even with no writer
    // connected yet; reads just return "nothing available" until one is.
    pipe_fd = open(path, O_RDONLY | O_NONBLOCK);
    if (pipe_fd < 0) {
        fprintf(stderr, "SonicDemoRecord: couldn't open pipe '%s': %s\n", path, strerror(errno));
        exit(1);
    }

    fprintf(stderr, "SonicDemoRecord: waiting for button commands on '%s' (U D L R A B C S)\n", path);
}

uint8_t AIPipe_Poll(void) {
    char c;
    while (read(pipe_fd, &c, 1) > 0) {
        if (c == '\n') {
            pipe_line[pipe_line_len] = 0;
            pipe_state = ParseButtonLine(pipe_line);
            pipe_line_len = 0;
        } else if (pipe_line_len + 1 < sizeof(pipe_line)) {
            pipe_line[pipe_line_len++] = c;
        }
    }
    return pipe_state;
}
