#include "Shader/Renderer/Primitive/PrimitiveRenderer.hlsli"
#include "Shader/Common/Lighting.hlsli"

float4 main(VS_OUT pin) : SV_TARGET
{
    // 1. 法線の正規化
    // ※頂点からピクセルへ渡される間に補間(ブレンド)され、長さが1からズレるため再度正規化します。
    float3 N = normalize(pin.worldNormal);
    
    // 2. 光の方向の正規化
    // Scene.hlsli の lightDirection は「光が進む方向」なので、逆向き(-lightDirection)にして計算します。
    float3 L = normalize(-gDirLight.direction);
    
    // 3. ランバート反射（面の向きと光の向きの内積）
    // dot(N,L) は、光が正面なら 1.0、真横なら 0.0、裏側ならマイナスになります。
    // saturate() を被せることで、0.0 ～ 1.0 の範囲に強制的に収めます（裏側が真っ黒になる）。
    float NdotL = saturate(dot(N, L));
    
    // 4. 環境光（Ambient）の設定
    // 光の当たらない裏側が「完全な真っ黒」になって視認性が落ちるのを防ぐため、最低限の明るさを足します。
    float ambient = 0.3f;
    
    // ★追加: ECSで設定した「色」と「強さ(intensity)」をしっかり掛け合わせる
    float3 lightFinalColor = gDirLight.color * gDirLight.intensity;
    
    // 5. 最終カラーの合成
    // 元の色 × (光の当たり具合 + 環境光) × 光の色
    float3 finalColor = pin.color.rgb * (NdotL + ambient) * lightFinalColor;
    
    // 6. アルファ値(透明度)は元の色をそのまま使う（ワイヤーフレーム等で必要なため）
    return float4(finalColor, pin.color.a);
}