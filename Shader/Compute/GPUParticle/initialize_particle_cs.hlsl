#include "Shader/Compute/GPUParticle/particle.hlsli"

// GPU上で書き込み可能な粒子バッファ
RWStructuredBuffer<particle> particle_buffer : register(REG_UAV(SLOT_UAV_PARTICLE_BUF));


// ====================================================
// Compute Shader のメイン関数（粒子初期化専用）
// すべての粒子をspawn関数で初期化する
// ====================================================
[numthreads(NUMTHREADS_X, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint id = dtid.x;

	// 現在の粒子データを取得
    particle p = particle_buffer[id];
	
	// --------------------------------------
	// 粒子を初期化
	// --------------------------------------
    spawn(id, p);
	
	// --------------------------------------
    // バーストモードの場合は死亡状態にする
    // （トリガーが来るまで待機）
    // --------------------------------------
    if (emission_mode == 1)
    {
        // バーストモード時は死亡状態で待機
        p.state = STATE_DEAD;
        p.color.a = 0.0f; // 透明化
        p.age = p.lifespan + 1.0f; // 寿命切れ状態にする
    }
    else
    {
        // 継続放出モード時は生存状態
        p.state = STATE_ALIVE;
        
        // --------------------------------------------------------------------------
        // 「最初から常に出ている」状態を作る (Pre-warm)
        // --------------------------------------------------------------------------
        
       // 0.0(先頭) ～ 1.0(最後尾) の割合
        // id 0番が「生まれたて」、id 最後尾が「もうすぐ死ぬ」ように分布させる
        float start_ratio = (float) id / (float) max_particle_count;
        
        // 【重要】ageを「マイナス(待機)」ではなく「プラス(経過済み)」にする
        // これにより、0秒時点ですでに「寿命の途中まで生きている」ことにします。
        p.age = start_ratio * p.lifespan;

        // 【重要】年齢を経過させるなら、その分「位置」も進めておく必要があります。
        // そうしないと、発生源で止まったまま消えかけるパーティクルになってしまいます。
        // 位置 = 現在位置 + (初速度 * 経過時間)
        p.position += p.velocity * p.age;

        // 重力の影響も簡易的に加算 (位置 += 1/2 * g * t^2)
        // ※正確でなくても「それっぽく散らばっている」ことが重要です
        float3 gVec = float3(0, gravity, 0);
        p.position += 0.5f * gVec * p.age * p.age;

        // 速度自体も重力で変化させておく (速度 += g * t)
        p.velocity += gVec * p.age;
    }
	
	// 更新後の粒子データを書き戻す
    particle_buffer[id] = p;
}
