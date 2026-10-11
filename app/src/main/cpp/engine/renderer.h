#pragma once
#include <string>

// Creates (or re-creates after a context loss) all GL resources.
// The world itself is generated and meshed only once.
void rendererInit();
void rendererResize(int width, int height);
void rendererFrame();
void rendererToggleWireframe();
std::string rendererStats();
