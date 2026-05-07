-- Data/Script/Test.lua

-- 変数はそのまま書く（自動的にこのエンティティ専用になる）
timer = 0
count = 0

function Update(dt, id)
    -- self.timer ではなく、そのまま timer でOK
    timer = timer + dt

    if timer > 1.0 then
        count = count + 1
        CppLog("Entity " .. id .. ": Passed " .. count .. " seconds.")
        timer = 0
    end
end