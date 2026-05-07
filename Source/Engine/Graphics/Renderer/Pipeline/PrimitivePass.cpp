#include "PrimitivePass.h"
#include "Engine/Graphics/Renderer/PrimitiveRenderer.h"
#include "Engine/Graphics/Renderer/ShapeRenderer.h"
#include "Engine/Graphics/Core/Camera.h"

void PrimitivePass::Execute(const RenderContext& rc){

    ZoneScopedN("Pass: Primitive");


    if (!rc.camera) return;

    if (rc.shapeRenderer) {
        rc.shapeRenderer->Render(rc.deviceContext, rc.camera->GetView(), rc.camera->GetProjection());
    }
    if (rc.primitiveRenderer) {
        rc.primitiveRenderer->Render(rc, rc.camera->GetView(), rc.camera->GetProjection());
    }
}