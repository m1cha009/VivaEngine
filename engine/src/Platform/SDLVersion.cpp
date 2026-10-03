#include "Viva/Version.h"

#include <SDL3/SDL_version.h>

namespace Viva {

Version GetSDLVersion()
{
    // SDL packs its version into a single int, and the SDL_VERSIONNUM_* macros unpack it.
    // SDL_GetVersion() reports the SDL library we linked, while the SDL_VERSION macro reports
    // the headers we compiled against. With static linking they're always the same. With a
    // DLL, a different SDL3.dll could be dropped next to the .exe and they'd differ.
    const int version = SDL_GetVersion();

    // static_cast is C++'s explicit cast, like (uint)x in C#. It's required here because brace
    // initialization refuses to silently convert an int into a uint32_t.
    return {
        static_cast<uint32_t>(SDL_VERSIONNUM_MAJOR(version)),
        static_cast<uint32_t>(SDL_VERSIONNUM_MINOR(version)),
        static_cast<uint32_t>(SDL_VERSIONNUM_MICRO(version)),
    };
}

} // namespace Viva
