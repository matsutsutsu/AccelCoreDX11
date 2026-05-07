#include "PostProcessPass.h"
#include "Engine/Graphics/Shader/PostProcess/PostProcessManager.h"
#include "Engine/Graphics/Core/Graphics.h"

void PostProcessPass::Execute(const RenderContext& rc){

    ZoneScopedN("Pass: PostProcess");


    if (!rc.postProcess) return;

    // HDRバッファにブルームとトーンマップをかけ、バックバッファ（画面）に出力する
    rc.postProcess->EndScene(rc.deviceContext, Graphics::Instance().GetBackBufferRTV());
}