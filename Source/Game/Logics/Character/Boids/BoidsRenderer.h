//#pragma once
//#include <d3d11.h>
//#include <wrl/client.h>
//#include <vector>
//#include "BoidsSwarmComponent.h"
//
//class BoidsRenderer {
//public:
//    void Initialize(ID3D11Device* device);
//    void BeginFrame();
//    void Draw(BoidsSwarmComponent* swarm); // バケツに入れる
//    void Render(ID3D11DeviceContext* dc);  // 一気に描画する
//
//private:
//    std::vector<BoidsSwarmComponent*> _drawCommands;
//
//    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
//    Microsoft::WRL::ComPtr<ID3D11PixelShader>  ps;
//};