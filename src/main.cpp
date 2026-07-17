#include "Application.hpp"

#include <SDL3/SDL_main.h>

#include <cstdlib>

int main(const int argumentCount, char** arguments) {
    const char* scenePath = argumentCount > 1
        ? arguments[1]
        : "assets/levels/test_scene.glb";

    Application application;
    if (!application.initialize(scenePath)) {
        return EXIT_FAILURE;
    }

    application.run();
    return EXIT_SUCCESS;
}
