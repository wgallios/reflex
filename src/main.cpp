#include "Application.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>

int main(int, char**) {
    Application application;
    if (!application.initialize()) {
        return EXIT_FAILURE;
    }

    application.run();
    return EXIT_SUCCESS;
}
