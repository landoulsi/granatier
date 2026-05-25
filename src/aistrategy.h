/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AISTRATEGY_H
#define AISTRATEGY_H

class PlayerAI;

/**
 * @brief Base class for AI strategies. Subclasses implement specific difficulty levels.
 */
class AIStrategy
{
protected:
    PlayerAI* m_ai;

public:
    explicit AIStrategy(PlayerAI* ai) : m_ai(ai) {}
    virtual ~AIStrategy() = default;
    virtual void update() = 0;
};

#endif // AISTRATEGY_H
