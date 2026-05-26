/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AISTRATEGYLEVEL2_H
#define AISTRATEGYLEVEL2_H

#include "aistrategy.h"
#include <QPoint>
#include <QList>
#include <vector>

class Arena;

/**
 * @brief Level 2 AI Strategy implements lingering fire evasion, bad bonus bypass, and bad bonus bombing.
 */
class AIStrategyLevel2 : public AIStrategy
{
private:
    Arena* m_arena;
    int m_rows;
    int m_cols;

    // Movement state
    QList<QPoint> m_currentPath;
    QPoint m_currentTargetCell;

    struct GridNode {
        int r, c;
        int parentIdx;
        GridNode(int row, int col, int parent = -1) : r(row), c(col), parentIdx(parent) {}
    };

    struct RecentDanger {
        QPoint cell;
        int ticksLeft;
        RecentDanger() : ticksLeft(0) {}
        RecentDanger(const QPoint& pt, int ticks) : cell(pt), ticksLeft(ticks) {}
    };
    QList<RecentDanger> m_recentDangerCells;

    std::vector<std::vector<bool>> computeDangerGrid(const QPoint& simulatedBombPos = QPoint(-1, -1));
    QList<QPoint> findPath(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlyAvoidDeadEnds = false);
    QList<QPoint> findPathImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlySafe, bool strictlyAvoidBadBonuses, bool strictlyAvoidDeadEnds);
    QList<QPoint> findPathToEnemyDirectly(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, bool strictlyAvoidDeadEnds = false);
    QList<QPoint> findPathToEnemyDirectlyImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, bool strictlyAvoidBadBonuses, bool strictlyAvoidDeadEnds);

    bool isGreenBonus(int row, int col);
    bool isBadBonus(int row, int col);
    bool isShieldBonus(int row, int col);
    bool hasGreenBonusNearby(int row, int col, int maxDist);
    bool isAdjacentToBlock(int row, int col);
    bool isAdjacentToEnemy(int row, int col);
    bool isAdjacentToBadBonus(int row, int col);
    int countWalkableNeighbors(int row, int col);
    void moveTowardsCell(const QPoint& targetCell);

public:
    explicit AIStrategyLevel2(PlayerAI* ai);
    ~AIStrategyLevel2() override;

    void update() override;
};

#endif // AISTRATEGYLEVEL2_H
