#pragma once

struct TimerComponent
{
	// 経過時間
	float currentTime = 0.0f;

	// ★追加: 寿命 (これが0より大きければ自動削除の対象にする)
    float lifeTime = 0.0f;

	// ★追加: 期限切れフラグ (重複処理防止用)
    bool isExpired = false;

	// 何秒間に実行すべきか（int 版）
	bool IsTimeStep(int num)
	{
		if (static_cast<int>(currentTime) % num != 0)
		{
			isStep = false;
			return false;
		}

		if (isStep) return false;

		isStep = true;
		return true;
	}
	// 何秒間に実行すべきか（float 版）
	bool IsTimeStep(float interval, float dt)
	{
		// fmod で周期判定（誤差対策として dt 未満なら true）
		if (fmod(currentTime, interval) >= dt)
		{
			isStep = false;
			return false;
		}

		if (isStep) return false;

		isStep = true;
		return true;
	}
	// int 以外を禁止にする
	template<typename T>
	bool IsTimeStep(T) = delete;

	// 時間が経過したかチェック
	bool IsTimeUp(float duration) const { return currentTime >= duration; }
private:
	bool isStep = false;
};