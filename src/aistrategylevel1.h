/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef AISTRATEGYLEVEL1_H
#define AISTRATEGYLEVEL1_H

#include "aistrategy.h"
#include <QPoint>
#include <QList>
#include <vector>

class Arena;

/**
 * @brief Level 1 AI Strategy implements the BFS escape, bonus collection, and block clearing logic.
 */
class AIStrategyLevel1 : public AIStrategy
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

    std::vector<std::vector<bool>> computeDangerGrid(const QPoint& simulatedBombPos = QPoint(-1, -1));
    QList<QPoint> findPath(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType);
    QList<QPoint> findPathImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlySafe);
    QList<QPoint> findPathToEnemyDirectly(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid);

    bool isGreenBonus(int row, int col);
    bool isBadBonus(int row, int col);
    bool isAdjacentToBlock(int row, int col);
    bool isAdjacentToEnemy(int row, int col);
    void moveTowardsCell(const QPoint& targetCell);

public:
    explicit AIStrategyLevel1(PlayerAI* ai);
    ~AIStrategyLevel1() override;

    void update() override;
};

#endif // AISTRATEGYLEVEL1_H
