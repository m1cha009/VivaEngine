#include "Viva/Component.h"

#include "Viva/GameObject.h"

namespace Viva {

// Defined here rather than in Component.h: it needs GameObject's full definition, and
// GameObject.h already includes Component.h (the other way round would be a circle).
Transform& Component::GetTransform() const
{
    return m_GameObject->GetTransform();
}

} // namespace Viva
