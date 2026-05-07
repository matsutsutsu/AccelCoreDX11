//#pragma once
//#include "ECS/System/CCL_System.h"
//#include "BoidsSwarmComponent.h"
//
//// 1. UpdateSystem (CSを回す)
//class BoidsUpdateSystem : public CCL::ECS::IfSystem<BoidsUpdateSystem, CCL::ECS::Write<BoidsSwarmComponent>> {
//public:
//    BoidsUpdateSystem() : IfSystem("BoidsUpdateSystem") {}
//    void Initialize(ID3D11Device* device);
//    void Update(float dt) override;
//private:
//    Microsoft::WRL::ComPtr<ID3D11ComputeShader> _cs;
//};
//
//// 2. RenderSystem (バケツに入れる)
//class BoidsRenderSystem : public CCL::ECS::IfSystem<BoidsRenderSystem, CCL::ECS::Read<BoidsSwarmComponent>> {
//public:
//    BoidsRenderSystem() : IfSystem("BoidsRenderSystem") {}
//    void Update(float dt) override;
//};