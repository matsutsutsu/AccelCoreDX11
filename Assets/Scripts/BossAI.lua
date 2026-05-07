function Update(dt, id)
    local boss = Entity.GetBoss(id)
    if boss == nil then
        -- CppLog("Boss Component is NIL for ID: " .. id)
        return
    end

 
    local health = Entity.GetHealth(id)
    if health == nil then
        -- CppLog("Health Component is NIL for ID: " .. id)
        return
    end

    -- ★デバッグ: 現在の状態を表示
    -- CppLog("Boss State: " .. boss.state .. " (Idle=" .. BossState.Idle .. ")")
    
    -- 1. 行動中なら何もしない（C++にお任せ）
    if boss.state ~= BossState.Idle then
        CppLog("Boss is busy, skipping AI logic")
        return
    end

    CppLog("Boss is Idle, running AI logic...")
    
    -- 2. ここからAIの思考ロジック
    CppLog("Health: " .. health.current .. "/" .. health.max)
    CppLog("Distance to player: " .. boss.dist)
    CppLog("Phase: " .. boss.hpPhase)
    
    -- HPが半分を切ったらフェーズ移行
    if health.current < health.max * 0.5 and boss.hpPhase == 1 then
        boss.hpPhase = 2
        CppLog("Boss Enraged Mode!")
        boss:SetState(BossState.Summon)
        return
    end

    -- プレイヤーが近い (6m以内) -> 薙ぎ払い
    if boss.dist < 6.0 then
        CppLog("Player is close, using SweepAttack")
        boss:SetState(BossState.SweepAttack)
        return
    end

    -- プレイヤーが遠い (15m以上) -> 突進
    if boss.dist > 15.0 then
        CppLog("Player is far, using Charge")
        boss:SetState(BossState.Charge)
        return
    end

    -- それ以外 -> 歩いて近づく
    if math.random() < 0.01 then
        CppLog("Moving towards player")
        boss:SetState(BossState.Move)
    end
end