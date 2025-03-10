//---------------------------------------------------------
// file:	OptimisticClientScenarioState.cpp
// author:	Matthew Picioccio
// email:	matthew.picioccio@digipen.edu
//
// brief:	The optimistic scenario for the client, in which the remote is the authority.
//
// Copyright © 2021 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "pch.h"
#include "OptimisticClientScenarioState.h"
#include "PacketSerializer.h"

const int kNetworkBufferSize = 1024; // maximum size of our packet
const float kTimeBetweenClientSend_Secs = 0.1f; // acceptable latency on client control updates: 100ms (plus wire, etc.)
const float kDrawRemoteHit_Secs = 2.0f; // number of seconds to draw the remote player as hit
const float kAttackTextSize = 30.0f; // The size of the attack text.
const CP_Color kAttackAgreeTextColor = CP_Color_Create(255, 255, 255, 255); // The color of the attack text when local and remote agree
const CP_Color kAttackDisagreeTextColor = CP_Color_Create(255, 0, 255, 255); // The color of the attack text when local and remote disagree.


OptimisticClientScenarioState::OptimisticClientScenarioState(const SOCKET socket)
	: NetworkedScenarioState(socket, false),
	active_control_(OptimisticClientScenarioState::Active_Control::Simple),
	is_drawing_controls_(false),
	is_attack_queued_(false),
	remote_hit_timer_secs_(0.0f),
	local_frame_(0),
	remote_frame_(0),
	send_timer_secs_(0.0f), // always start with a packet
	time_since_last_recv_(0.0f),
	packet_(kNetworkBufferSize)
{
	local_player_.color = CP_Color_Create(255, 0, 0, 255);
	remote_player_.color = CP_Color_Create(0, 0, 255, 255);
	local_attack_.SetAttackColor(CP_Color_Create(0, 200, 0, 0));
	local_attack_.SetTargetColor(CP_Color_Create(255, 0, 255, 0));
	local_attack_.SetTargetSize(30.0f);
	remote_confirmed_attack_.SetAttackColor(CP_Color_Create(0, 0, 0, 0));
	remote_confirmed_attack_.SetTargetColor(CP_Color_Create(255, 255, 255, 0));
	remote_confirmed_attack_.SetTargetSize(25.0f);
}


OptimisticClientScenarioState::~OptimisticClientScenarioState() = default;


void OptimisticClientScenarioState::Update()
{
	NetworkedScenarioState::Update();

	const bool is_local_paused = CP_Input_KeyDown(KEY_SPACE);

	if (CP_Input_KeyTriggered(CP_KEY::KEY_D))
	{
		is_drawing_controls_ = !is_drawing_controls_;
	}

	if (CP_Input_KeyTriggered(CP_KEY::KEY_A))
	{
		switch (active_control_)
		{
		case Active_Control::Simple:
			active_control_ = Active_Control::Dead_Reckoning;
			break;
		case Active_Control::Dead_Reckoning:
			active_control_ = Active_Control::Snapshot;
			break;
		case Active_Control::Snapshot:
			active_control_ = Active_Control::Simple;
			break;
		}
	}

	const auto system_dt = 1.0f / 30.0f; // CP_System_GetDt();
	simple_local_control_.Update(system_dt);
	simple_remote_control_.Update(system_dt);
	snapshot_local_control_.Update(system_dt);
	snapshot_remote_control_.Update(system_dt);
	dr_local_control_.Update(system_dt);
	dr_remote_control_.Update(system_dt);

	auto local_x = 0.0f, local_y = 0.0f;
	auto remote_x = 0.0f, remote_y = 0.0f;
	SyncRatio current_sync{};
	switch (active_control_)
	{
	case Active_Control::Dead_Reckoning:
		local_x = dr_local_control_.GetCurrentX();
		local_y = dr_local_control_.GetCurrentY();
		remote_x = dr_remote_control_.GetCurrentX();
		remote_y = dr_remote_control_.GetCurrentY();
		current_sync = dr_local_control_.GetSyncRatio();
		break;
	case Active_Control::Snapshot:
		local_x = snapshot_local_control_.GetCurrentX();
		local_y = snapshot_local_control_.GetCurrentY();
		remote_x = snapshot_remote_control_.GetCurrentX();
		remote_y = snapshot_remote_control_.GetCurrentY();
		current_sync = snapshot_local_control_.GetSyncRatio();
		break;
	case Active_Control::Simple:
		local_x = simple_local_control_.GetCurrentX();
		local_y = simple_local_control_.GetCurrentY();
		remote_x = simple_remote_control_.GetCurrentX();
		remote_y = simple_remote_control_.GetCurrentY();
		current_sync = simple_local_control_.GetSyncRatio();
		break;
	}
	local_player_.SetPosition(local_x, local_y);
	remote_player_.SetPosition(remote_x, remote_y);

	if (CP_Input_KeyTriggered(CP_KEY::KEY_F))
	{
		is_attack_queued_ = true;
		local_attack_.Set(local_x, local_y, remote_x, remote_y, current_sync);
	}

	time_since_last_recv_ += system_dt;
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
			float host_x, host_y, host_velocity_x, host_velocity_y;
			float non_host_x, non_host_y, non_host_velocity_x, non_host_velocity_y;
			// CONVENTION: host writes its own values first
			PacketSerializer::ReadValue<float>(packet_, host_x);
			PacketSerializer::ReadValue<float>(packet_, host_y);
			PacketSerializer::ReadValue<float>(packet_, host_velocity_x);
			PacketSerializer::ReadValue<float>(packet_, host_velocity_y);
			PacketSerializer::ReadValue<float>(packet_, non_host_x);
			PacketSerializer::ReadValue<float>(packet_, non_host_y);
			PacketSerializer::ReadValue<float>(packet_, non_host_velocity_x);
			PacketSerializer::ReadValue<float>(packet_, non_host_velocity_y);
			// if there is a confirmed client attack, then process it
			auto is_client_attacking = false;
			PacketSerializer::ReadValue<bool>(packet_, is_client_attacking);
			if (is_client_attacking)
			{
				float client_attack_x, client_attack_y;
				float target_x, target_y;
				PacketSerializer::ReadValue<float>(packet_, client_attack_x);
				PacketSerializer::ReadValue<float>(packet_, client_attack_y);
				PacketSerializer::ReadValue<float>(packet_, target_x);
				PacketSerializer::ReadValue<float>(packet_, target_y);
				remote_confirmed_attack_.Set(client_attack_x, client_attack_y, target_x, target_y, SyncRatio());
				remote_hit_timer_secs_ = remote_confirmed_attack_.IsTargetHit() ? kDrawRemoteHit_Secs : 0.0f;
			}
			// store the data in all of the controls
			simple_local_control_.SetLastKnown(non_host_x, non_host_y, remote_frame_);
			simple_remote_control_.SetLastKnown(host_x, host_y, remote_frame_);
			dr_local_control_.SetLastKnown(non_host_x, non_host_y, non_host_velocity_x, non_host_velocity_y, time_since_last_recv_, remote_frame_);
			dr_remote_control_.SetLastKnown(host_x, host_y, host_velocity_x, host_velocity_y, time_since_last_recv_, remote_frame_);
			snapshot_local_control_.AddSnapshot({ non_host_x, non_host_y, time_since_last_recv_}, remote_frame_);
			snapshot_remote_control_.AddSnapshot({ host_x, host_y, time_since_last_recv_}, remote_frame_);
		}
		time_since_last_recv_ = 0.0f;
	}

	send_timer_secs_ -= system_dt;
	if (send_timer_secs_ < 0.0f)
	{
		packet_.Reset();
		PacketSerializer::WriteValue<u_long>(packet_, ++local_frame_);
		PacketSerializer::WriteValue<bool>(packet_, is_local_paused);
		PacketSerializer::WriteValue<bool>(packet_, is_attack_queued_);
		if (is_attack_queued_)
		{
			const auto attack_sync = local_attack_.GetSyncRatio(); // use the stored sync, not the current one!
			PacketSerializer::WriteValue<float>(packet_, local_attack_.GetAttackX());
			PacketSerializer::WriteValue<float>(packet_, local_attack_.GetAttackY());
			PacketSerializer::WriteValue<u_long>(packet_, attack_sync.base_frame);
			PacketSerializer::WriteValue<u_long>(packet_, attack_sync.target_frame);
			PacketSerializer::WriteValue<float>(packet_, attack_sync.t);
			is_attack_queued_ = false;
		}
		send(socket_, packet_.GetRoot(), packet_.GetUsedSpace(), 0);
		send_timer_secs_ = kTimeBetweenClientSend_Secs;
	}
}


void OptimisticClientScenarioState::Draw()
{
	ScenarioState::Draw();

	local_attack_.Draw(true, true);
	remote_confirmed_attack_.Draw(false, true);

	if (remote_hit_timer_secs_ > 0.0f)
	{
		remote_player_.color = CP_Color_Create(0, 255, 255, 255);
		remote_hit_timer_secs_ -= CP_System_GetDt();
	}
	else
	{
		remote_player_.color = CP_Color_Create(0, 0, 255, 255);
	}

	local_player_.Draw();
	remote_player_.Draw();

	// draw the debug visualizations of our replication controls
	if (is_drawing_controls_)
	{
		switch (active_control_)
		{
		case Active_Control::Dead_Reckoning:
			dr_local_control_.Draw();
			dr_remote_control_.Draw();
			break;
		case Active_Control::Snapshot:
			snapshot_local_control_.Draw();
			snapshot_remote_control_.Draw();
			break;
		case Active_Control::Simple:
			break;
		}
	}

	if (local_attack_.IsVisible())
	{
		std::string attack_status = local_attack_.IsTargetHit() ? "Local Attack: HIT" : "Local Attack: MISS";
		CP_Settings_Fill(kAttackAgreeTextColor);
		if (remote_confirmed_attack_.IsVisible())
		{
			attack_status += remote_confirmed_attack_.IsTargetHit() ? ", Remote Confirm: HIT" : ", Remote Confirm: MISS";
			if (local_attack_.IsTargetHit() != remote_confirmed_attack_.IsTargetHit())
			{
				CP_Settings_Fill(kAttackDisagreeTextColor);
			}
		}
		CP_Settings_TextSize(kAttackTextSize);
		CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_LEFT, CP_TEXT_ALIGN_V_TOP);
		CP_Font_DrawText(attack_status.c_str(), 0.0f, 715.0f);
	}
}


std::string OptimisticClientScenarioState::GetDescription() const
{
	std::string description("Optimistic Scenario, Client, Local: ");
	description += std::to_string(local_frame_);
	description += ", Remote: ";
	description += std::to_string(remote_frame_);
	switch (active_control_)
	{
	case Active_Control::Simple:
		description += ", Simple";
		break;
	case Active_Control::Dead_Reckoning:
		description += ", Dead Reckoning";
		break;
	case Active_Control::Snapshot:
		description += ", Snapshot";
		break;
	}
	if (is_drawing_controls_)
	{
		description += ", Drawing";
	}
	return description;
}


std::string OptimisticClientScenarioState::GetInstructions() const
{
	return "Hold SPACE to halt local (red) player, F to attack, A to toggle control, D to toggle drawing";
}


bool OptimisticClientScenarioState::HandleSocketError(const char* error_text)
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