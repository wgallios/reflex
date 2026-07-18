#include "Application.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>

int main(const int argumentCount, char** arguments) {
    const char* scenePath = argumentCount > 1
        ? arguments[1]
        : "assets/campaign/level01.json";

    Application application;
    if (!application.initialize(scenePath)) {
        return EXIT_FAILURE;
    }

    application.run();
    return EXIT_SUCCESS;
}
