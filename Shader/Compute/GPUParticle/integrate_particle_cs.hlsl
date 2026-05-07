#include "Shader/Compute/GPUParticle/particle.hlsli"

void UpdateParticle(inout particle p); // これがプロトタイプ宣言

// GPU上で書き込み可能な粒子バッファ
RWStructuredBuffer<particle> particle_buffer : register(REG_UAV(SLOT_UAV_PARTICLE_BUF));

// サンプラー・テクスチャ
#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
//SamplerState sampler_states[3] : register(REG_SMP_LINEAR);
SamplerState sampler_linear : register(REG_SMP(SLOT_SMP_LINEAR));
Texture2D color_temper_chart : register(REG_SRV(SLOT_SRV_MAT_TEX1)); // パーティクル色の補正用
Texture3D<float4> noise_3d : register(REG_SRV(SLOT_SRV_MAT_TEX2)); // float4に変更推奨

// ====================================================
// Compute Shader のメイン関数（粒子の更新処理）
// NUMTHREADS_X スレッド並列で粒子を更新
// ====================================================
[numthreads(NUMTHREADS_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x; // このスレッドが担当する粒子のインデックス

	// 現在の粒子データを取得
    particle p = particle_buffer[id];
	
    p.type = type; // 将来拡張用にタイプをセット（現状は固定）

	// --------------------------------------
    // バーストモードの処理
    // --------------------------------------
    if (emission_mode == 1) // バーストモード
    {
        // バーストトリガーが発動中かつこのパーティクルがバースト範囲内
        if (burst_trigger > 0.0f && id < burst_count)
        {
            // 死んでいる or 寿命が尽きた粒子だけを再生成
            if (p.state == STATE_DEAD || p.age > p.lifespan)
            {
                spawn(id, p);
                p.state = STATE_ALIVE;
            }
        }
        
        // 生きている粒子の処理
        if (p.state == STATE_ALIVE)
        {
            // 年齢を進める
            p.age += delta_time;
            
            // 寿命が尽きたら死亡状態にする（リスポーンしない）
            if (p.age > p.lifespan)
            {
                p.state = STATE_DEAD;
                p.color.a = 0.0f; // 完全に透明化
            }
            else
            {
                // 生きている粒子の挙動更新（後述の更新処理）
                UpdateParticle(p);
            }
        }
    }
    else // 継続放出モード（従来の処理）
    {
        // --------------------------------------
        // 粒子の年齢を進める
        // --------------------------------------
        p.age += delta_time;
        
        // --------------------------------------
        // 寿命が尽きた場合、再生成(respawn)する
        // --------------------------------------
        bool respawn = true;
        if (p.age > p.lifespan && respawn)
        {
            spawn(id, p); // spawn関数で初期化
            p.state = STATE_ALIVE;
        }
        
        // 生きている粒子の挙動更新
        if (p.age > 0)
        {
            UpdateParticle(p);
        }
    }
	// 更新後の粒子データをバッファに書き戻す
    particle_buffer[id] = p;
}





// ====================================================
// パーティクル更新処理を関数化
// ====================================================
void UpdateParticle(inout particle p)
{
    float3 force = 0;
    
    // 重力加算
    force += p.mass * float3(0.0, gravity, 0.0);
    
    // 乱流（Turbulence）の適用
    // 位置に基づきノイズをサンプリング (時間経過でスクロールさせて動きをつける)
    float3 uvw = p.position * noise_scale + float3(0, time * 0.1, 0);
    float3 noise = noise_3d.SampleLevel(sampler_linear, uvw, 0).xyz;

    // 0.0~1.0 を -1.0~1.0 にリマップして方向ベクトルにする
    float3 noiseDir = noise * 2.0 - 1.0;

    // 速度に加算 (massの影響を受けるようにしても良い)
    p.velocity += noiseDir * noise_strength * delta_time;

    // 抵抗（Drag）を入れるとより自然になります（お好みで）
    // p.velocity *= 0.98;

#if 0
    // 風などの外力を加えたい場合のサンプル(未使用)
    float3 wind_dir = float3(1, 0, 0);
    float wind_strength = 1;
    p.velocity += normalize(wind_dir) * wind_strength * delta_time;
#endif

#if 0  
    // ベクトル場(電荷の影響など)のサンプル(未使用)
    float3 electric_charge_position = float3(sin(time), 1.0, cos(time));
    float3 r = electric_charge_position - p.position;
    float w = 1.0;
    float l = max(1e-4, length(r));
    force += w * normalize(r) / (l * l * l);
#endif

    // 力から加速度を計算して速度を更新
    p.velocity += force / p.mass * delta_time;

#if 1
    // 最大速度制限(高速になりすぎないようにクランプ)
    float speed = length(p.velocity);
    const float max_speed = 10.0;
    p.velocity = min(max_speed, speed) * normalize(p.velocity);
#endif	

    // 位置を更新
    p.position += p.velocity * delta_time;
    
    // =============================================================
    // ★ここに実装！ (簡易フロアコリジョン)
    // =============================================================
    // 位置更新の後、Y座標が地面より下になっていないかチェックする
    /*
    float ground_height = 0.0;
    if (p.position.y < ground_height)
    {
        // 1. 位置を地面の上に戻す (めり込み防止)
        p.position.y = ground_height;

        // 2. 速度のY成分を反転させて跳ね返らせる
        //    (0.5 などの係数を掛けると、エネルギーを失って跳ねなくなる)
        p.velocity.y = -p.velocity.y * 0.5;

        // 3. XZ方向（横滑り）にも摩擦をかけて止まるようにする
        p.velocity.xz *= 0.9;
    }
    */
    // =============================================================
    
    // --------------------------------------
    // 色の更新
    // --------------------------------------
    // テクスチャ参照で色調整
    if (p.type == 0)
    {
        // マニュアル指定色を使用
        p.color.rgb = manual_color.rgb;
    }
    else
    {
        // カラーテンパーモード　1: 色温度チャート参照　0 : 手動カラー
        if(color_mode == 1)
        {       
            p.color = color_temper_chart.SampleLevel(sampler_linear, float2(0.5, p.age / p.lifespan), 0);
            p.color = pow(p.color, 2.3); // 色補正(ガンマ調整)
        }
        else
        {
            // マニュアル指定色を使用
            p.color.rgb = manual_color.rgb;
        }
    
    }

#if 1
    // フェードイン/フェードアウト
    float alpha = fade_in(fade_duration.x, p.age, 1) * fade_out(fade_duration.y, p.age, p.lifespan, 1);
    p.color.a = pow(alpha, 1);
#endif		

    // 回転角度の更新
    p.angle += p.angular_speed * delta_time;
    
    // =============================================================
    // スプライトシートアニメーション
    // =============================================================
    if (sprite_anim_mode == 1) // Life Cycle Mode
    {
        // 全コマ数
        uint total_frames = sprite_sheet_grid.x * sprite_sheet_grid.y;
        
        if (total_frames > 1)
        {
            // 寿命に対する現在の進行度 (0.0 ～ 1.0)
            float t = saturate(p.age / p.lifespan);
            
            // 進行度に合わせてコマを選択
            // 例: 0.0->0コマ目, 0.5->真ん中, 1.0->最後のコマ
            p.chip = (uint) (t * (float) total_frames);
            
            // 念のため範囲外アクセス防止
            if (p.chip >= total_frames)
                p.chip = total_frames - 1;
        }
    }
    

}