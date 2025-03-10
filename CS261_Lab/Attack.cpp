//---------------------------------------------------------
// file:	Attack.cpp
// author:	Matthew Picioccio
// email:	matthew.picioccio@digipen.edu
//
// brief:	Represents an instant area-of-effect "attack" on the other player.
//
// Copyright © 2021 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "pch.h"
#include "Attack.h"
#include "LabMath.h"

const float kAttackRadius = 100.0f;


void Attack::Set(const float attack_x, const float attack_y, const float target_x, const float target_y, const SyncRatio& attack_sync)
{
    x_ = attack_x;
    y_ = attack_y;
    target_x_ = target_x;
    target_y_ = target_y;
    is_target_hit_ = LabMath::IsWithinDistance(attack_x, attack_y, target_x, target_y, kAttackRadius);
    sync_ratio_ = attack_sync;
    alpha_ = 255;
}


void Attack::Draw(const bool draw_attack, const bool draw_target)
{
    if (!IsVisible())
    {
        return;
    }

    if (draw_attack)
    {
        CP_Settings_NoStroke();
        CP_Settings_Fill(CP_Color_Create(attack_color_.r, attack_color_.g, attack_color_.b, alpha_));
        CP_Graphics_DrawCircle(x_, y_, kAttackRadius * 2.0f);
    }

    if (draw_target)
    {
        CP_Settings_NoStroke();
        CP_Settings_Fill(CP_Color_Create(target_color_.r, target_color_.g, target_color_.b, 255));
        CP_Graphics_DrawCircle(target_x_, target_y_, target_draw_size_);
    }

    alpha_ -= 2;
}