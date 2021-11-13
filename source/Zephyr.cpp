#include <cstdlib>
#include "Renderer.hpp"
#include "Logger.hpp"

int main(int argc, char *argv[])
{
    Logger::init();

    LOG_INFO("Number of arguments passed on launch: {}", argc);
    for (int index{}; index != argc; ++index)
        LOG_INFO("Argument {}: {}", index, argv[index]);

    Renderer renderer;

    if (!renderer.initialise())
    {
        LOG_CRITICAL("Failed to initalise Renderer");
        return EXIT_FAILURE;
    }

    // Continuous loop until the main window is marked for closing
    renderer.drawLoop();

    return EXIT_SUCCESS;
}