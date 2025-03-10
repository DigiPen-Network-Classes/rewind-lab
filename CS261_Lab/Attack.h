//---------------------------------------------------------
// file:	Attack.h
// author:	Matthew Picioccio
// email:	matthew.picioccio@digipen.edu
//
// brief:	Represents an instant area-of-effect "attack" on the other player.
//
// Copyright © 2021 DigiPen, All rights reserved.
//---------------------------------------------------------
#pragma once
#include "cprocessing.h"
#include "SyncRatio.h"


/// <summary>
/// Represents an instant area-of-effect "attack" on the other player.
/// </summary>
struct Attack
{
public:

	void Set(float attack_x, float attack_y, float target_x, float target_y, const SyncRatio& attack_sync);

	inline void SetAttackColor(CP_Color color) { attack_color_ = color; }
	inline void SetTargetColor(CP_Color color) { target_color_ = color; }
	inline void SetTargetSize(float target_size) { target_draw_size_ = target_size; }
	void Draw(bool draw_attack, bool draw_target);

	inline float GetAttackX() const { return x_; }
	inline float GetAttackY() const { return y_; }
	inline float GetTargetX() const { return target_x_; }
	inline float GetTargetY() const { return target_y_; }
	inline bool IsTargetHit() const { return is_target_hit_; }
	inline bool IsVisible() const { return alpha_ > 0; }
	inline SyncRatio GetSyncRatio() const { return sync_ratio_; }

private:
	float x_ = 0.0f, y_ = 0.0f;
	float target_x_ = 0.0f, target_y_ = 0.0f;
	int alpha_ = 0; // start invisible
	bool is_target_hit_ = false;
	SyncRatio sync_ratio_{};

	CP_Color attack_color_{ 255, 255, 255 };
	float target_draw_size_ = 25.0f;
	CP_Color target_color_{ 255, 0, 255 };
};