#include "ParticlePass.h"
#include "Engine/Graphics/Renderer/ParticleRenderer.h"

void ParticlePass::Execute(const RenderContext& rc){

    ZoneScopedN("Pass: Particle");


    if (rc.particleRenderer) {
        rc.particleRenderer->Render(rc.deviceContext);
    }
}