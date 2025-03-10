//---------------------------------------------------------
// file:	OptimisticClientScenarioState.h
// author:	Matthew Picioccio
// email:	matthew.picioccio@digipen.edu
//
// brief:	The optimistic scenario for the client, in which the remote is the authority.
//
// Copyright © 2021 DigiPen, All rights reserved.
//---------------------------------------------------------
#pragma once
#include "NetworkedScenarioState.h"
#include "Player.h"
#include "SimpleSyncControl.h"
#include "SnapshotControl.h"
#include "DeadReckoningControl.h"
#include "Packet.h"
#include "Attack.h"


/// <summary>
/// The optimistic scenario for the client, in which the remote is the authority.
/// </summary>
class OptimisticClientScenarioState final :
    public NetworkedScenarioState
{
public:
    OptimisticClientScenarioState(const SOCKET socket);
    ~OptimisticClientScenarioState() override;

    OptimisticClientScenarioState(const OptimisticClientScenarioState&) = delete;
    OptimisticClientScenarioState(OptimisticClientScenarioState&&) = delete;
    OptimisticClientScenarioState& operator=(const OptimisticClientScenarioState&) = delete;
    OptimisticClientScenarioState& operator=(OptimisticClientScenarioState&&) = delete;

    void Update() override;
    void Draw() override;

    std::string GetDescription() const override;
    std::string GetInstructions() const override;

private:
    bool HandleSocketError(const char* error_text);

    enum class Active_Control
    {
        Simple,
        Snapshot,
        Dead_Reckoning,
    } active_control_;

    SimpleSyncControl simple_local_control_;
    SimpleSyncControl simple_remote_control_;
    SnapshotControl snapshot_local_control_;
    SnapshotControl snapshot_remote_control_;
    DeadReckoningControl dr_local_control_;
    DeadReckoningControl dr_remote_control_;
    bool is_drawing_controls_;

    Player local_player_;
    Player remote_player_;

    Attack local_attack_;
    Attack remote_confirmed_attack_;
    bool is_attack_queued_;
    float remote_hit_timer_secs_;

    u_long local_frame_;
    u_long remote_frame_;
    float send_timer_secs_;
    float time_since_last_recv_;

    Packet packet_;
};