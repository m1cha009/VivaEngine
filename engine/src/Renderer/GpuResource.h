#pragma once

namespace Viva {

// The base class of every resource games hold (Mesh, Texture, Shader, Material).
//
// When a game lets go of one, the GPU may still be drawing with it in a frame that's in flight,
// so the renderer can't destroy it yet. It parks it in that frame's list instead, as a
// std::unique_ptr<GpuResource>, and destroys it once the frame's fence says the GPU is done.
// One list holds every kind of resource because they share this base; the virtual destructor
// makes destroying through a GpuResource pointer run the real class's destructor (like calling
// Dispose() through an IDisposable reference in C#).
class GpuResource {
public:
    virtual ~GpuResource() = default;

    GpuResource(const GpuResource&) = delete;
    GpuResource& operator=(const GpuResource&) = delete;

protected:
    GpuResource() = default; // only derived classes are created
};

} // namespace Viva
