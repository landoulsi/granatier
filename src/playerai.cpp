/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "playerai.h"
#include "aistrategylevel1.h"
#include "game.h"
#include "player.h"

PlayerAI::PlayerAI(Game* game, Player* player, int level)
    : QObject(game)
    , m_game(game)
    , m_player(player)
    , m_level(level)
{
    if (m_level == 1) {
        m_strategy = std::make_unique<AIStrategyLevel1>(this);
    }
}

PlayerAI::~PlayerAI()
{
    // unique_ptr handles deleting m_strategy automatically.
}

void PlayerAI::update()
{
    if (m_strategy) {
        m_strategy->update();
    }
}
