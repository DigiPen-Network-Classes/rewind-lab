//---------------------------------------------------------
// file:	Player.cpp
// author:	Matthew Picioccio
// email:	matthew.picioccio@digipen.edu
//
// brief:	Representation of a player in the game
//
// Copyright © 2021 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "pch.h"
#include "Player.h"

const int kPlayerTrailLength = 15;


void Player::SetPosition(float x, float y)
{
	const float kSkipEpsilon = 0.01f;
	float x_diff = fabsf(x - current_x);
	float y_diff = fabsf(y - current_y);
	if ((x_diff < kSkipEpsilon) && (y_diff < kSkipEpsilon))
	{
		return;
	}

	if (previous_x.size() > kPlayerTrailLength)
	{
		previous_x.pop_back();
		previous_y.pop_back();
	}
	previous_x.push_front(current_x);
	previous_y.push_front(current_y);
	current_x = x;
	current_y = y;
}


/// <summary>
/// Draw the player object at its current location.
/// </summary>
void Player::Draw() const
{
	CP_Settings_NoStroke();

	auto alpha = 255;
	auto draw_color = CP_Color_Create(color.r, color.g, color.b, alpha);
	auto draw_size = size;
	CP_Settings_Fill(draw_color);
	CP_Graphics_DrawCircle(current_x, current_y, draw_size);
	for (size_t i = 0; i < previous_x.size(); ++i)
	{
		alpha -= 255 / kPlayerTrailLength;
		draw_color = CP_Color_Create(color.r, color.g, color.b, alpha);
		CP_Settings_Fill(draw_color);
		CP_Graphics_DrawCircle(previous_x[i], previous_y[i], draw_size - i);
	}
}