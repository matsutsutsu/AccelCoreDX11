#include "Shader/Compute/GPUParticle/particle.hlsli"
// パーティクル構造体や定数バッファの共通定義を読み込み
// → vertex_shader と同じ定義を共有することで GPU 側と一致させる

//	1粒ごとに：
//	 ├─
//	StructuredBufferからパーティクル情報を取得
//	 │ position, angle, color, size, chip...
//	 ├─
//	生存チェック（ age<= 0 || age >=
//	lifespan） ならスキップ
//	 ├─
//	size補間（ spawn→
//	despawn）
//	 ├─ 4 頂点（
//	左下・ 左上・
//	右下・ 右上）
//	を生成
//	 │    ├─ 回転と拡大
//	 │    ├─
//	UV座標設定（ スプライトシート対応）
//	 │    └─
//	color設定
//	 ├─ output.Append()
//	で描画用ストリームへ送出
//	 └─ RestartStrip() 
//	で区切る（1 枚の四角形完成）



//======================================================================
// GPU上のパーティクル配列（StructuredBuffer）
//======================================================================
// Compute Shader が毎フレーム更新した "particle" 配列をここで読み取る。
// 各インデックス = 1粒。
StructuredBuffer<particle> particle_buffer : register(REG_SRV(SLOT_SRV_PARTICLE_BUF));


//======================================================================
// Geometry Shader の宣言
//======================================================================
// 最大4頂点を出力する → 四角形スプライト1枚分
// "point" トポロジー（1粒 = 1点）を入力に取り、
// "TriangleStream" で4頂点のポリゴンを出力する。
[maxvertexcount(4)] // 「1つの点を受け取って、最大4つの頂点（四角形）を作るぞ」という宣言
void main(point VS_OUT input[1], inout TriangleStream<GS_OUT> output)
{
    // 四角形スプライトのローカル座標（-1～+1）
    const float2 corners[] =
    {
        float2(-1.0, -1.0), // 左下
        float2(-1.0, +1.0), // 左上
        float2(+1.0, -1.0), // 右下
        float2(+1.0, +1.0), // 右上
    };

    // UV座標
    const float2 texcoords[] =
    {
        float2(0.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0),
    };

    // パーティクル情報の取得
    particle p = particle_buffer[input[0].vertex_id];

    // 寿命チェック
    if (p.age <= 0.0 || p.age >= p.lifespan)
        return;

    // アスペクト比補正
    const float aspect_ratio = 1280.0 / 720.0;

    // --- サイズ計算 ---
    // ここで計算した size を全モードで使用します
    float age_ratio = saturate(p.age / p.lifespan);
    float size = lerp(p.size.x, p.size.y, age_ratio);

    // =============================================================
    // ★統合: 分岐を削除し、すべてのタイプで以下の計算を行います
    // =============================================================
    
    // --- 速度ストレッチ計算 ---
    float2 stretch_scale = float2(1.0, 1.0);
    float rotation_angle = p.angle;

    if (velocity_stretch > 0.0)
    {
        float3 view_velocity = mul(float4(p.velocity, 0.0), viewProjection).xyz;
        float speed = length(view_velocity.xy);

        if (speed > 0.001)
        {
            rotation_angle = atan2(view_velocity.y, view_velocity.x) - PI / 2.0;
            stretch_scale.y = 1.0 + speed * velocity_stretch;
            stretch_scale.x = max(0.1, 1.0 - speed * velocity_stretch * 0.1);
        }
    }

    // --- 頂点生成ループ ---
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        GS_OUT element;

        // 中心座標
        element.position = mul(float4(p.position, 1), viewProjection);

        // ローカル座標の計算 (回転・ストレッチ適用)
        float2 raw_corner = corners[i];
        
        // 1. ストレッチ
        raw_corner *= stretch_scale;

        // 2. 回転
        float c = cos(rotation_angle);
        float s = sin(rotation_angle);
        
        float2 rotated_corner;
        rotated_corner.x = raw_corner.x * c - raw_corner.y * s;
        rotated_corner.y = raw_corner.x * s + raw_corner.y * c;

        // 3. サイズ適用 (計算済みの size を使用)
        element.position.xy += rotated_corner * float2(size, size * aspect_ratio);

        // 色
        element.color = p.color;

        // スプライトシート計算 (Type 0でも計算しておいて損はありません)
        if (sprite_sheet_grid.x * sprite_sheet_grid.y > 1)
        {
            float2 uv = texcoords[i] / sprite_sheet_grid;
            float2 grid_size = 1.0 / sprite_sheet_grid;
            uint x = p.chip % sprite_sheet_grid.x;
            uint y = p.chip / sprite_sheet_grid.x;
            element.texcoord = uv + grid_size * uint2(x, y);
        }
        else
        {
            element.texcoord = texcoords[i];
        }

        // タイプを渡す (PSでテクスチャを使うかどうかの判断に使用)
        element.type = p.type;

        output.Append(element);
    }

    output.RestartStrip();
}