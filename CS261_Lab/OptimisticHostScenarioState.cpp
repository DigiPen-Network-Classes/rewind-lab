//---------------------------------------------------------
// file:	OptimisticHostScenarioState.cpp
// author:	Matthew Picioccio
// email:	matthew.picioccio@digipen.edu
//
// brief:	The optimistic scenario for the host, in which the local system is the authority.
//
// Copyright © 2021 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "pch.h"
#include "OptimisticHostScenarioState.h"
#include "PacketSerializer.h"
#include "DoubleOrbitControl.h"
#include "LabMath.h"

const int kNetworkBufferSize = 1024; // maximum size of our packet
const float kDrawLocalHit_Secs = 2.0f; // number of seconds to draw the local player as hit
const float kAttackTextSize = 30.0f; // The size of the attack text.
const CP_Color kAttackTextColor = CP_Color_Create(255, 255, 255, 255); // The color of the attack text.
const int kLocalStateHistorySize = 100; // the amount of state records to keep


OptimisticHostScenarioState::OptimisticHostScenarioState(const SOCKET socket)
	: NetworkedScenarioState(socket, true),
	local_control_(200.0f, 250.0f, 100.0f, 1.5f),
	remote_control_(200.0f, 150.0f, 100.0f, 2.0f),
	is_remote_paused_(false),
	is_client_attack_queued_(false),
	local_hit_timer_secs_(0.0f),
	local_frame_(0),
	remote_frame_(0),
	send_timer_secs_(0.0f), // always start with a packet
	target_time_between_send_(0.0f),
	packet_(kNetworkBufferSize)
{
	local_player_.color = CP_Color_Create(255, 0, 0, 255);
	remote_player_.color = CP_Color_Create(0, 0, 255, 255);
}


OptimisticHostScenarioState::~OptimisticHostScenarioState() = default;


void OptimisticHostScenarioState::Update()
{
	NetworkedScenarioState::Update();

	if (CP_Input_KeyTriggered(CP_KEY::KEY_W))
	{
		target_time_between_send_ += 0.1f;
		if (target_time_between_send_ > 0.5f)
		{
			target_time_between_send_ = 0.0f;
		}
	}

	const auto system_dt = 1.0f / 30.0f; // CP_System_GetDt();
	const bool is_local_paused = CP_Input_KeyDown(KEY_SPACE);
	// always send a packet when the server pauses...
	if (CP_Input_KeyTriggered(KEY_SPACE))
	{
		send_timer_secs_ = 0.0f; 
	}

	local_control_.Update(!is_local_paused ? system_dt : 0.0f);
	remote_control_.Update(!is_remote_paused_ ? system_dt : 0.0f);

	local_player_.SetPosition(local_control_.GetCurrentX(), local_control_.GetCurrentY());
	remote_player_.SetPosition(remote_control_.GetCurrentX(), remote_control_.GetCurrentY());

	packet_.Reset();
	const auto res = recv(socket_, packet_.GetRoot(), packet_.GetRemainingSpace(), 0);
	if (res > 0)
	{
		u_long received_frame;
		PacketSerializer::ReadValue<u_long>(packet_, received_frame);
		// only use data if it's newer than the last frame we received
		if (received_frame > remote_frame_)
		{
			remote_frame_ = received_frame;
			// the host only receives control updates, while the client receives all positions
			PacketSerializer::ReadValue<bool>(packet_, is_remote_paused_);

			bool is_client_attacking;
			PacketSerializer::ReadValue<bool>(packet_, is_client_attacking);
			if (is_client_attacking)
			{
				float attack_x, attack_y;
				u_long base_attack_frame, target_attack_frame;
				float attack_t;
				PacketSerializer::ReadValue<float>(packet_, attack_x);
				PacketSerializer::ReadValue<float>(packet_, attack_y);
				PacketSerializer::ReadValue<u_long>(packet_, base_attack_frame);
				PacketSerializer::ReadValue<u_long>(packet_, target_attack_frame);
				PacketSerializer::ReadValue<float>(packet_, attack_t);
				// calculate historical position of the local (target) player, using remote sync information and stored frames
				const auto base_attack_record_iter = std::find_if(local_state_history_.begin(), local_state_history_.end(), [=](ControlStateRecord record) { return record.frame == base_attack_frame; });
				const auto target_attack_record_iter = std::find_if(local_state_history_.begin(), local_state_history_.end(), [=](ControlStateRecord record) { return record.frame == target_attack_frame; });
				if (base_attack_record_iter == local_state_history_.end())
				{
					std::cout << "Base attack frame " << base_attack_frame << "not found" << std::endl;
				}
				else if (target_attack_record_iter == local_state_history_.end())
				{
					std::cout << "Base attack frame " << base_attack_frame << " found, BUT target attack frame " << target_attack_frame << " not found" << std::endl;
				}
				else
				{
					//GOAL: calculate historical position of the local (target) player, using remote sync information and stored frames
					float target_x, target_y;

					// just use whatever the host has now
					//WRONG: current values won't match what the client thought they were hitting!
					target_x = local_control_.GetCurrentX();
					target_y = local_control_.GetCurrentY();

					// TODO ADD LAB CODE HERE!

					client_attack_.Set(attack_x, attack_y, target_x, target_y, { base_attack_frame, target_attack_frame, attack_t });
					is_client_attack_queued_ = true;
					local_hit_timer_secs_ = client_attack_.IsTargetHit() ? kDrawLocalHit_Secs : 0.0f;
				}
			}
		}
	}

	send_timer_secs_ -= system_dt;
	if (send_timer_secs_ < 0.0f)
	{
		packet_.Reset();
		PacketSerializer::WriteValue<u_long>(packet_, ++local_frame_);
		// CONVENTION: host writes its own values first
		PacketSerializer::WriteValue<float>(packet_, local_control_.GetCurrentX());
		PacketSerializer::WriteValue<float>(packet_, local_control_.GetCurrentY());
		PacketSerializer::WriteValue<float>(packet_, local_control_.GetCurrentVelocityX());
		PacketSerializer::WriteValue<float>(packet_, local_control_.GetCurrentVelocityY());
		PacketSerializer::WriteValue<float>(packet_, remote_control_.GetCurrentX());
		PacketSerializer::WriteValue<float>(packet_, remote_control_.GetCurrentY());
		PacketSerializer::WriteValue<float>(packet_, remote_control_.GetCurrentVelocityX());
		PacketSerializer::WriteValue<float>(packet_, remote_control_.GetCurrentVelocityY());
		PacketSerializer::WriteValue<bool>(packet_, is_client_attack_queued_);
		if (is_client_attack_queued_)
		{
			PacketSerializer::WriteValue<float>(packet_, client_attack_.GetAttackX());
			PacketSerializer::WriteValue<float>(packet_, client_attack_.GetAttackY());
			PacketSerializer::WriteValue<float>(packet_, client_attack_.GetTargetX());
			PacketSerializer::WriteValue<float>(packet_, client_attack_.GetTargetY());
			is_client_attack_queued_ = false;
		}
		send(socket_, packet_.GetRoot(), packet_.GetUsedSpace(), 0);
		send_timer_secs_ = target_time_between_send_;

		local_state_history_.push_back({ local_frame_, local_control_.GetState(), {local_control_.GetCurrentX(), local_control_.GetCurrentY(), target_time_between_send_} });
		while (local_state_history_.size() > kLocalStateHistorySize)
		{
			local_state_history_.pop_front();
		}
	}
}


void OptimisticHostScenarioState::Draw()
{
	ScenarioState::Draw();

	client_attack_.Draw(true, true);

	if (local_hit_timer_secs_ > 0.0f)
	{
		local_player_.color = CP_Color_Create(255, 255, 0, 255);
		local_hit_timer_secs_ -= CP_System_GetDt();
	}
	else
	{
		local_player_.color = CP_Color_Create(255, 0, 0, 255);
	}

	local_player_.Draw();
	remote_player_.Draw();

	local_control_.Draw();
	remote_control_.Draw();

	if (client_attack_.IsVisible())
	{
		const std::string attack_status = client_attack_.IsTargetHit() ? "Client Attack: HIT" : "Client Attack: MISS";
		CP_Settings_TextSize(kAttackTextSize);
		CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_TOP);
		CP_Settings_Fill(kAttackTextColor);
		CP_Font_DrawText(attack_status.c_str(), 0.0f, 715.0f);
	}
}


std::string OptimisticHostScenarioState::GetDescription() const
{
	std::string description("Optimistic Scenario, Host, Local: ");
	description += std::to_string(local_frame_);
	description += ", Remote: ";
	description += std::to_string(remote_frame_);
	description += ", Send Target: ";
	description += std::to_string(static_cast<int>(target_time_between_send_ * 1000));
	description += "ms";
	return description;
}


std::string OptimisticHostScenarioState::GetInstructions() const
{
	return "Hold SPACE to halt the local (red) player, W to increase Send Target";
}


bool OptimisticHostScenarioState::HandleSocketError(const char* error_text)
{
	const auto wsa_error = WSAGetLastError();

	// ignore WSAEWOULDBLOCK
	if (wsa_error == WSAEWOULDBLOCK)
	{
		return false;
	}

	// log unexpected errors and return to the default game mode
	std::cerr << "Optimistic WinSock Error: " << error_text << wsa_error << std::endl;

	// close the socket and clear it
	// -- this should trigger a GameStateManager reset in the next Update
	closesocket(socket_);
	socket_ = INVALID_SOCKET;

	return true;
}