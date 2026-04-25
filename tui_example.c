#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>

void simple_tui_example() {
    // Initialize notcurses
    struct notcurses *nc = notcurses_init(NULL, stdout);
    if (!nc) {
        fprintf(stderr, "Failed to initialize notcurses\n");
        return;
    }

    // Create a new plane (think of it as a canvas)
    struct ncplane *n = notcurses_stdplane(nc);
    if (!n) {
        notcurses_stop(nc);
        fprintf(stderr, "Failed to get standard plane\n");
        return;
    }

    // Draw some text
    ncplane_putstr(n, "Hello, notcurses!");

    // Render the changes
    notcurses_render(nc);

    // Wait for a key press
    while (getchar() != 'q') {
        // You can add more rendering logic here
    }

    // Clean up
    notcurses_stop(nc);
}

int main() {
    simple_tui_example();
    return 0;
}