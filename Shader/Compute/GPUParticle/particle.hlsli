#ifndef PARTICLE_HLSLI
#define PARTICLE_HLSLI

#include "Engine/Graphics/Shader/ShaderResources.h"
#include "Shader/Common/Scene.hlsli"
//=====================================================================
//  GPU Particle Initializer Shader
//  粒子の初期化処理を担当するComputeShader補助コード
//=====================================================================

cbuffer PARTICLE_SYSTEM_CONSTANTS : register(REG_CB(SLOT_CB_PARTICLE))
{
    // 発生源（エミッタ）の基本パラメータ群
    float4 emission_position;       // 粒子が発生する中心位置（ワールド座標）
    float4 emission_rotation;       // エミッタの回転
    float2 emission_offset;         // 発生範囲（半径）x:最小, y:最大
    float2 emission_size;           // サイズ設定 x:出現時, y:消滅時
    float2 emission_cone_angle;     // 放出角度範囲（ラジアン）
    float2 emission_speed;          // 初速度範囲
    float2 emission_angular_speed;  // 角速度範囲（回転演出）
    float2 lifespan;                // 寿命（秒）範囲
    float2 spawn_delay;             // 出現遅延（秒）範囲
    float2 fade_duration;           // フェード時間（in/out）
    float4 manual_color;            // 手動カラー補正（RGBA乗算）

    // シミュレーション共通値
    float time;                     // 経過時間（フレーム全体）
    float delta_time;               // 1フレームあたりの経過秒数
    float noise_scale;              // 位置ノイズや乱流演出に使うスケール
    float noise_strength;           // ノイズ影響力
    
    float gravity;                  // 重力加速度（マイナス値で下向き）
    float velocity_stretch;
    float2 uv_scroll_speed;
    
    int sprite_anim_mode;
    int render_mode;
    float2 padding1;

    uint2 sprite_sheet_grid;        // スプライトアニメの分割数（列×行）
    float2 particle_scale;          // パーティクルの大きさ
    
    float3 world_position;          //実際のエミッタ位置（ワールド座標）
    int color_mode; // カラーテンパーモード（将来拡張用）
    
    int max_particle_count;        // システム全体の粒子数
    int type;                      // ← 0=Point, 1=Quad を指定
    int emission_mode = 0;         // 0: 継続放出, 1: バースト(一時的)
    int burst_count = 100;         // バースト時に一度に放出する数

    float burst_trigger = 0.0f;     // >0でバースト発動
    float particle_offset_y = 0.0f; // Y方向の発生オフセット（未使用）
    float emission_stretch_x; // X方向のオフセットストレッチ係数
    float emission_stretch_z; // Z方向のオフセットストレッチ係数
    

}

//--------------------------------------------
// 頂点シェーダー出力構造体
//--------------------------------------------
struct VS_OUT
{
    uint vertex_id : VERTEXID;
};

//--------------------------------------------
// ジオメトリシェーダー出力構造体
//--------------------------------------------
struct GS_OUT
{
    float4 position : SV_POSITION; // スクリーン座標
    float4 color : COLOR; // 色
    float2 texcoord : TEXCOORD; // テクスチャ座標
    uint type : TEXCOORD1; // パーティクルタイプ（0=Point, 1=Quad）
};

//--------------------------------------------
// 粒子1個あたりのデータ構造
//--------------------------------------------
struct particle
{
    int state; // 状態（0:未使用, 1:生存, 2:消滅など）
    float angle; // 回転角
    int pad0[2];

    float4 color; // 色（RGBA）

    float3 position; // ワールド空間上の座標
    float mass; // 質量（風や重力計算用）

    float angular_speed; // 回転速度
    float3 velocity; // 速度ベクトル

    float lifespan; // 総寿命
    float age; // 経過時間（負ならまだ発生前）
    float2 size; // x:出現サイズ, y:消滅サイズ    スケール比率固定


    uint chip; // 使用スプライトのインデックス
    uint type; // 矩形か球体かなどのタイプ識別用（将来拡張用）
    float2 scale;   // パーティクルの大きさ       スケール比率設定
};

//--------------------------------------------
// 定数
//--------------------------------------------
#define PI 3.14159265358979
#define NUMTHREADS_X 16 // ComputeShaderスレッド数（X方向）

// パーティクルの状態定義
#define STATE_DEAD 0      // 死亡/未使用
#define STATE_ALIVE 1     // 生存中

//=====================================================================
// 疑似乱数関数（スレッドID依存）
//=====================================================================
float rand(float n)
{
    // 簡易的なハッシュ型乱数。0.0～1.0の範囲で返す。
    return frac(sin(n) * 43758.5453123);
}

// 高品質・独立乱数（Wang Hash）
uint wangHash(uint s)
{
    s = (s ^ 61) ^ (s >> 16);
    s *= 9;
    s = s ^ (s >> 4);
    s *= 0x27d4eb2d;
    s = s ^ (s >> 15);
    return s;
}

float randUint(uint s)
{
    return (wangHash(s) & 0x00FFFFFF) / 16777216.0;
}

// --------------------------------------------------------
// クォータニオン(q)でベクトル(v)を回転させる関数
// --------------------------------------------------------
float3 RotateVector(float3 v, float4 q)
{
    // クォータニオン回転の標準公式: v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v)
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

//=====================================================================
// 粒子生成関数： spawn()
// 各スレッドが1つの粒子を担当し、初期パラメータを設定する
//=====================================================================
void spawn(in uint id, inout particle p)
{
    // 独立乱数を十分に確保（用途ごとに分ける）
    float r_rand = randUint(id * 1000u + 1u); // 位置（半径）用
    float a_rand = randUint(id * 1000u + 2u); // 角度用
    float speed_rand = randUint(id * 1000u + 3u); // 速度大きさ用
    float life_rand = randUint(id * 1000u + 4u); // 寿命用（ここが重要）
    float size_rand = randUint(id * 1000u + 5u); // サイズ用
    float chip_rand = randUint(id * 1000u + 6u); // スプライト用
    float ang_rand = randUint(id * 1000u + 7u); // 回転用
    float y_rand = randUint(id * 1000u + 8u); // Yオフセット用

    // -------------------------
    // 位置（均一楕円）
    // -------------------------
    float u = r_rand;
    float angle = a_rand * 2.0 * PI;
    float rMin = emission_offset.x;
    float rMax = emission_offset.y;
    float r = sqrt(lerp(rMin * rMin, rMax * rMax, u));
    float sx = emission_stretch_x;
    float sz = emission_stretch_z;
    float3 offset;
    offset.x = r * cos(angle) * sx;
    offset.z = r * sin(angle) * sz;
    offset.y = particle_offset_y * (y_rand * 2.0 - 1.0);
    p.position = world_position + emission_position.xyz + offset;

    // -------------------------
    // 方向（円錐）と速度
    // -------------------------
    // 方向の角は別乱数（ここは a_rand or another one）
    float phi = 2.0 * PI * randUint(id * 1000u + 9u); // 独立角度
    float theta = lerp(emission_cone_angle.x, emission_cone_angle.y, randUint(id * 1000u + 10u));
    float sin_theta = sin(theta);
    float cos_theta = cos(theta);
    
    // 1. まずローカル空間での速度ベクトルを作成 (Y軸基準)
    float3 localVelocity;
    localVelocity.x = sin_theta * cos(phi);
    localVelocity.y = cos_theta; // Y-Up
    localVelocity.z = sin_theta * sin(phi);

    // 速度の大きさをランダムに決定
    float speed = lerp(emission_speed.x, emission_speed.y, speed_rand);
    localVelocity *= speed;

    // ---------------------------------------------------------
    // ローカル速度をエミッターの回転に合わせて回す！
    // ---------------------------------------------------------
    p.velocity = RotateVector(localVelocity, emission_rotation);

    // -------------------------
    // 見た目・回転
    // -------------------------
    p.color = float4(1, 1, 1, 1);
    p.mass = 1.0f;
    p.angle = PI * ang_rand;
    p.angular_speed = lerp(emission_angular_speed.x, emission_angular_speed.y, randUint(id * 1000u + 11u));

    // -------------------------
    // 寿命　← ここを独立させる（重要）
    // -------------------------
    p.lifespan = lerp(lifespan.x, lifespan.y, life_rand);

    if (emission_mode == 1)
        p.age = 0.0f;
    else
        p.age = -lerp(spawn_delay.x, spawn_delay.y, randUint(id * 1000u + 12u));

    p.state = STATE_ALIVE;

    // -------------------------
    // サイズ・チップなど
    // -------------------------
    p.size.x = emission_size.x * size_rand;
    p.size.y = emission_size.y * randUint(id * 1000u + 13u);

    int count = sprite_sheet_grid.x * sprite_sheet_grid.y;
    
    // アニメーションモードによって初期チップを変える
    if (sprite_anim_mode == 1)
    {
        // Lifeモードなら、必ず「最初のコマ」からスタート
        p.chip = 0;
    }
    else
    {
        // Randomモードなら、ランダムなコマを選ぶ
        p.chip = (uint) (chip_rand * count);
    }
    
    p.chip = (uint) (chip_rand * count);
    p.type = type;
    p.scale = particle_scale;
}


//=====================================================================
// フェードイン補間関数
//=====================================================================
// age: 経過時間, duration: フェードにかかる時間, exponent: 補間カーブ
float fade_in(float duration, float age, float exponent)
{
    // smoothstep: 0→1間で滑らかに補間、powでカーブを制御
    return pow(smoothstep(0.0, 1.0, age / duration), exponent);
}

//=====================================================================
// フェードアウト補間関数
//=====================================================================
// lifespan: 全寿命, age: 現在の生存時間
float fade_out(float duration, float age, float lifespan, float exponent)
{
    // 残り寿命をもとにフェードアウト
    return pow(smoothstep(0.0, 1.0, (lifespan - age) / duration), exponent);
}


#endif