#include "Shader/Compute/GPUParticle/particle.hlsli"

//----------------------------------------------
// サンプラーの種類を定義
//----------------------------------------------
// POINT        : 最近傍サンプリング（1ピクセルをそのまま）
// LINEAR       : 線形補間（滑らか）
// ANISOTROPIC  : 異方性フィルタリング（角度に応じて補間）
#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2

// サンプラー配列（Texture sampling の設定）
//SamplerState sampler_states[3] : register(REG_SMP_LINEAR);
SamplerState sampler_linear : register(REG_SMP(SLOT_SMP_LINEAR));

// パーティクルのテクスチャ（例：煙・火花などの画像）
Texture2D color_map : register(REG_SRV(SLOT_SRV_MAT_TEX0));

//----------------------------------------------
// ピクセルごとに実行されるシェーダー
//----------------------------------------------
float4 main(GS_OUT pin) : SV_TARGET
{
	// -----------------------------
	// ジオメトリシェーダーから受け取った基本色
	// （例えばパーティクルの発光色など）
	// -----------------------------
	float4 color = pin.color;
    
    // pin.type は 0 = Point, 1 = Quad (GSでセットされている)
    // **Point の場合**：テクスチャは使わず color を返す（完全に自前色）
    if (pin.type == 0)
    {
        return color;
    }

    // ---------------------------------------------------------
    // UVスクロール
    // ---------------------------------------------------------
    float2 uv = pin.texcoord;
    
    // 時間経過(time) × 速度(speed) を足してズラす
    uv += uv_scroll_speed * time;    
    
#if 1
	//------------------------------------------
    // 【テクスチャの明度を掛ける処理】
    // color_map（煙などのテクスチャ）をサンプリングして、
    // RGBの明るさを計算（グレースケール化）して
    // 元の color に掛ける。
    //
    // 明度が低い部分は透明になる！
    // → 白黒画像を明るさマスクとして使う感じ。
    // → 火花や煙などの「透明部分」を制御する用途。
    //------------------------------------------
    float4 texColor = color_map.Sample(sampler_linear, uv);
    
    // -----------------------------------------------------------
    // ★修正: 描画モードによる色の計算分岐
    // -----------------------------------------------------------
    if (render_mode == 0) // Additive (加算)
    {
        // 従来通り：テクスチャの明るさをアルファとして扱う
        // (真っ黒な背景の画像でも、黒を透明として扱える)
        float brightness = dot(texColor.rgb, float3(0.299, 0.587, 0.114));
        color.rgb *= texColor.rgb;
        color.a *= brightness;
    }
    else // Transparency (半透明)
    {
        // 新方式：テクスチャのRGBをそのまま使い、アルファで切り抜く
        // (黒い煙などの表現が可能)
        // ※画像自体が透過PNGである必要があります
        color.rgb *= texColor.rgb;
        color.a *= texColor.a;
    }

	// 輝度計算：人間の目の感度に基づいた重み付き平均
    float brightness = dot(texColor.rgb, float3(0.299, 0.587, 0.114));

	// 元の色に明るさを掛けて暗く／明るくする
    color *= brightness;
#else
	//------------------------------------------
    // 【標準のテクスチャ乗算】
    // 上の「明度だけ使う」代わりに、
    // テクスチャのフルカラーを掛ける。
    // （これを使うと普通のテクスチャ合成になる）
    //------------------------------------------
	color *= color_map.Sample(sampler_states[LINEAR], pin.texcoord);
#endif
	
	
#if 0
	//------------------------------------------
    // 【簡易ライティング処理】（現在は無効）
    // ピクセルの位置から法線ベクトルを計算して
    // 光の方向との角度で明るさを決める。
    //
    // → 中心が光って、端が暗くなるような
    //    立体的なパーティクル表現を作れる。
    //------------------------------------------
	// テクスチャ座標 (0～1) を -1～+1 に変換して「スクリーン上の位置」を計算
	float2 p = float2((pin.texcoord.x - 0.5) * 2, (0.5 - pin.texcoord.y) * 2);

	// 球面上の法線を近似（パーティクルを半球状に照らす）
	float3 n = normalize(float3(p.x, p.y, -sqrt(1 - (p.x * p.x + p.y * p.y)))); // view space

	// 光源方向をビュー空間へ変換
	float3 l = normalize(mul(-light_direction, view)).xyz;
#if 1
	// フェードアウト（中心が明るく、端が暗くなる）
	color.rgb *= max(0.0, 0.5 * dot(n, l) + 0.5);
#else
	// シンプルな拡散光（光の当たり具合だけで明暗）
	color.rgb *= max(0.0, dot(n, l));
#endif
#endif
	
#if 0
	//------------------------------------------
    // 【中心フェード】（現在は無効）
    // パーティクルの中心付近は透明にしないで、
    // 端だけを少しぼかして消す。
    // → 円形パーティクルを自然にフェードさせたいときに使う。
    //------------------------------------------
	float dist = length(pin.texcoord - 0.5); // テクスチャ中央からの距離
	color.a *= smoothstep(0.5, 0.8, 1.0 - dist);
#endif

	return color;
}
