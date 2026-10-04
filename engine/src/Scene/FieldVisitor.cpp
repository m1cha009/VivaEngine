#include "Viva/FieldVisitor.h"

#include "Viva/Renderer.h"

namespace Viva {

void VisitFields(FieldVisitor& fields, MaterialSettings& settings)
{
    fields.Field("Texture", settings.Texture);
    fields.Field("Color", settings.Color);
    fields.Field("Tiling", settings.Tiling);
    fields.Field("Offset", settings.Offset);
}

} // namespace Viva
