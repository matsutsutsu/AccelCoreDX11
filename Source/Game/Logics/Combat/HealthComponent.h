#pragma once

enum class TeamID {
    Player,
    Enemy,
    Neutral
};

// 【純粋データ】HPと無敵状態の保持のみを行う
struct HealthComponent {
    float  maxHealth             = 100.0f;
    float  currentHealth         = 100.0f;
    float  invincibilityTimer    = 0.0f;
    float  invincibilityDuration = 0.5f;
    TeamID team                  = TeamID::Enemy;

};