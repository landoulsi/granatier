/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLAYERAI_H
#define PLAYERAI_H

#include <QObject>
#include <memory>

class Game;
class Player;
class AIStrategy;
class AIStrategyLevel2;

/**
 * @brief This class implements the Computer/AI controller dispatcher for a Player.
 * It holds the configured level and delegates ticks to the polymorphic AIStrategy.
 */
class PlayerAI : public QObject
{
    Q_OBJECT

    friend class AIStrategy;
    friend class AIStrategyLevel1;
    friend class AIStrategyLevel2;

private:
    Game* m_game;
    Player* m_player;
    int m_level;
    std::unique_ptr<AIStrategy> m_strategy;

public:
    /**
     * @brief Constructor.
     * @param game The game model instance.
     * @param player The player model instance to control.
     * @param level The difficulty/AI level to run (default is 2).
     */
    PlayerAI(Game* game, Player* player, int level = 2);

    /**
     * @brief Destructor.
     */
    ~PlayerAI() override;

    /**
     * @brief Ticks/updates the AI decision logic. Delegates to the strategy.
     */
    void update();

    // Getters for strategy access
    Game* game() const { return m_game; }
    Player* player() const { return m_player; }
    int level() const { return m_level; }
};

#endif // PLAYERAI_H
