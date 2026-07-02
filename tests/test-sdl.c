#include <SDL3/SDL.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window *window = SDL_CreateWindow(
        "SDL3 Window",
        800,
        600,
        SDL_WINDOW_RESIZABLE
    );

	

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

	SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

    // Main loop
    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = 0;
                    break;
            }
        }
      	// Background
        SDL_SetRenderDrawColor(renderer, 30, 250, 30, 255);
        SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);

        // Sleep a little to reduce CPU usage
        SDL_Delay(16);
    }

    // Cleanup
	SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
