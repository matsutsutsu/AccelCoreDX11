#include "ComponentRegistry.h"

// シングルトン記述
ComponentRegistry &ComponentRegistry::Instance()
{
    static ComponentRegistry s;
    return s;
}