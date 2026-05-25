/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLAYERAI_H
#define PLAYERAI_H

#include <QObject>
#include <QPoint>
#include <QList>
#include <vector>

class Game;
class Player;
class Bomb;
class Arena;

/**
 * @brief This class implements the smart computer/AI controller for a Player.
 * It makes decisions on navigation, bomb placement, threat evasion, and target prioritization.
 */
class PlayerAI : public QObject
{
    Q_OBJECT

private:
    Game* m_game;
    Player* m_player;
    Arena* m_arena;

    // Grid details
    int m_rows;
    int m_cols;

    // Movement state
    QList<QPoint> m_currentPath;
    QPoint m_currentTargetCell;

    // Pathfinding & threat structures
    struct GridNode {
        int r, c;
        int parentIdx;
        GridNode(int row, int col, int parent = -1) : r(row), c(col), parentIdx(parent) {}
    };

    /**
     * @brief Computes a grid of threats/danger where active bombs will explode.
     * @param simulatedBombPos If valid, simulates a bomb placed at this position.
     * @return 2D boolean grid where true means dangerous.
     */
    std::vector<std::vector<bool>> computeDangerGrid(const QPoint& simulatedBombPos = QPoint(-1, -1));

    /**
     * @brief BFS pathfinding from start cell to a list of potential targets or safe cells.
     * @param start The starting cell.
     * @param dangerGrid The danger grid to respect.
     * @param findSafeCell If true, we are searching for any cell where dangerGrid[r][c] == false.
     * @param targetType Standard target types (0: Safe cell, 1: Green Bonus, 2: Destructible Block, 3: Enemy player).
     * @return List of cell coordinates representing the path.
     */
    QList<QPoint> findPath(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType);
    QList<QPoint> findPathImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlySafe);
    QList<QPoint> findPathToEnemyDirectly(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid);

    /**
     * @brief Helper to check if a cell contains a green bonus.
     */
    bool isGreenBonus(int row, int col);

    /**
     * @brief Helper to check if a cell contains a bad bonus.
     */
    bool isBadBonus(int row, int col);

    /**
     * @brief Checks if a cell is adjacent to a destructible block.
     */
    bool isAdjacentToBlock(int row, int col);

    /**
     * @brief Checks if a cell is adjacent to an enemy player.
     */
    bool isAdjacentToEnemy(int row, int col);

    /**
     * @brief Directs the Player model towards a target cell in pixel coordinate space.
     */
    void moveTowardsCell(const QPoint& targetCell);

public:
    /**
     * @brief Constructor.
     * @param game The game model instance.
     * @param player The player model instance to control.
     */
    PlayerAI(Game* game, Player* player);

    /**
     * @brief Destructor.
     */
    ~PlayerAI() override;

    /**
     * @brief Ticks/updates the AI decision logic. Should be called in the game loop.
     */
    void update();
};

#endif // PLAYERAI_H
