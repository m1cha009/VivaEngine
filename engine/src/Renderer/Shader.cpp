#include "Renderer/Shader.h"

namespace Viva {

std::unique_ptr<Shader> Shader::Create(VkDevice device, const PipelineSettings& settings)
{
    std::unique_ptr<Pipeline> pipeline = Pipeline::Create(device, settings);
    if (!pipeline)
        return nullptr;
    auto shader = std::make_unique<Shader>();
    shader->m_Pipeline = std::move(pipeline);
    return shader;
}

} // namespace Viva
