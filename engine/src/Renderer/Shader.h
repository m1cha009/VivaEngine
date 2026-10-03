#pragma once

#include "Renderer/GpuResource.h"
#include "Renderer/Pipeline.h"

#include <memory>

namespace Viva {

// A shader as games see it: a vertex and a fragment shader, built into a pipeline with the
// renderer's standard settings (vertex format, descriptor sets, push constants, depth testing).
// In Unity terms, a Shader asset with one pass. Every shader shares the same pipeline layout, so
// switching between them leaves the bound descriptor sets in place.
class Shader : public GpuResource {
public:
    // Returns nullptr (after logging why) if the compiled shader files can't be loaded.
    static std::unique_ptr<Shader> Create(VkDevice device, const PipelineSettings& settings);

    const Pipeline& GetPipeline() const { return *m_Pipeline; }

private:
    std::unique_ptr<Pipeline> m_Pipeline;
};

} // namespace Viva
