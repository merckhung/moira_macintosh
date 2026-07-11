#ifndef MACINTOSH_SRC_PLATFORM_X11_PLATFORM_CREATION_H_
#define MACINTOSH_SRC_PLATFORM_X11_PLATFORM_CREATION_H_

#include <memory>

class Platform;
class MacSystem;

std::unique_ptr<Platform> CreateX11Platform(MacSystem* system);

#endif  // MACINTOSH_SRC_PLATFORM_X11_PLATFORM_CREATION_H_
