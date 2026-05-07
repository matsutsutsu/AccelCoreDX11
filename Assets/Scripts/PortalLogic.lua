-- C++の TriggerSystem から呼ばれる
function OnTriggerEnter()
    
    -- 1. 現在のシーン名を取得
    local currentName = Scene.GetName()
    local nextName = ""

    -- A. ショップにいる場合 -> 「ゲーム」へ
    if currentName == "Shop" then
        nextName = "Game"

         CppLog("Now Shop Stage go to game stage!")

        -- レベルはそのまま

    -- B. ゲームにいる場合 -> 「ショップ」へ戻る（クリア）
    elseif currentName == "Game" then
        nextName = "Shop"
        
        local currentLevel = Player.GetLevel()

        if currentLevel >= 5 then
            -- 最終ステージクリア
            CppLog("Congratulations! You have cleared the final stage!")
            Player.ResetLevel()
        else
            -- ステージクリアなのでレベルアップ
            currentLevel = currentLevel + 1
            Player.SetLevel(currentLevel)
            CppLog("Stage Cleared! Next Level: " .. currentLevel)
        end
    
    -- C. その他 -> ショップへ
    else
        nextName = "Shop"
    end

    -- プレイヤーデータ保存
    Game.Save() 

    -- シーン遷移
    if nextName ~= "" then
         CppLog("Scene Loading now")
        Scene.Load(nextName)
    end
end

-- 空の関数を追加して警告を消す
function Update(dt, id)
    -- 何もしない
end