/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aistrategylevel1.h"
#include "playerai.h"
#include "game.h"
#include "player.h"
#include "bomb.h"
#include "arena.h"
#include "bonus.h"
#include "block.h"
#include "granatierglobals.h"

#include <queue>
#include <cmath>
#include <QDebug>

AIStrategyLevel1::AIStrategyLevel1(PlayerAI* ai)
    : AIStrategy(ai)
    , m_arena(nullptr)
    , m_rows(0)
    , m_cols(0)
{
    m_currentTargetCell = QPoint(-1, -1);
}

AIStrategyLevel1::~AIStrategyLevel1()
{
    if (m_ai && m_ai->player()) {
        m_ai->player()->stopMoving();
    }
}

std::vector<std::vector<bool>> AIStrategyLevel1::computeDangerGrid(const QPoint& simulatedBombPos)
{
    std::vector<std::vector<bool>> danger(m_rows, std::vector<bool>(m_cols, false));
    QList<Bomb*> bombs = m_ai->game()->getBombs();

    // Map existing bombs
    for (auto* bomb : bombs) {
        if (bomb->isDetonated()) continue;
        int r = m_arena->getRowFromY(bomb->getY());
        int c = m_arena->getColFromX(bomb->getX());
        if (r >= 0 && r < m_rows && c >= 0 && c < m_cols) {
            danger[r][c] = true;
            int power = bomb->bombPower();
            int rowOffsets[] = {-1, 1, 0, 0};
            int colOffsets[] = {0, 0, -1, 1};
            for (int i = 0; i < 4; ++i) {
                for (int step = 1; step <= power; ++step) {
                    int nr = r + rowOffsets[i] * step;
                    int nc = c + colOffsets[i] * step;
                    if (nr < 0 || nr >= m_rows || nc < 0 || nc >= m_cols) break;
                    if (m_arena->getCell(nr, nc).getType() == Granatier::Cell::WALL) break;
                    danger[nr][nc] = true;
                    if (!m_arena->getCell(nr, nc).getElements(Granatier::Element::BLOCK).isEmpty()) break;
                }
            }
        }
    }

    // Map simulated bomb
    if (simulatedBombPos.x() != -1 && simulatedBombPos.y() != -1) {
        int r = simulatedBombPos.y();
        int c = simulatedBombPos.x();
        if (r >= 0 && r < m_rows && c >= 0 && c < m_cols) {
            danger[r][c] = true;
            int power = m_ai->player()->getBombPower();
            int rowOffsets[] = {-1, 1, 0, 0};
            int colOffsets[] = {0, 0, -1, 1};
            for (int i = 0; i < 4; ++i) {
                for (int step = 1; step <= power; ++step) {
                    int nr = r + rowOffsets[i] * step;
                    int nc = c + colOffsets[i] * step;
                    if (nr < 0 || nr >= m_rows || nc < 0 || nc >= m_cols) break;
                    if (m_arena->getCell(nr, nc).getType() == Granatier::Cell::WALL) break;
                    danger[nr][nc] = true;
                    if (!m_arena->getCell(nr, nc).getElements(Granatier::Element::BLOCK).isEmpty()) break;
                }
            }
        }
    }

    return danger;
}

bool AIStrategyLevel1::isGreenBonus(int row, int col)
{
    QList<Element*> elements = m_arena->getCell(row, col).getElements(Granatier::Element::BONUS);
    for (auto* element : elements) {
        auto* bonus = dynamic_cast<Bonus*>(element);
        if (bonus && !bonus->isTaken()) {
            Granatier::Bonus::Type type = bonus->getBonusType();
            if (type == Granatier::Bonus::SPEED ||
                type == Granatier::Bonus::POWER ||
                type == Granatier::Bonus::BOMB ||
                type == Granatier::Bonus::SHIELD ||
                type == Granatier::Bonus::THROW ||
                type == Granatier::Bonus::KICK ||
                type == Granatier::Bonus::RESURRECT) {
                return true;
            }
        }
    }
    return false;
}

bool AIStrategyLevel1::isBadBonus(int row, int col)
{
    QList<Element*> elements = m_arena->getCell(row, col).getElements(Granatier::Element::BONUS);
    for (auto* element : elements) {
        auto* bonus = dynamic_cast<Bonus*>(element);
        if (bonus && !bonus->isTaken()) {
            Granatier::Bonus::Type type = bonus->getBonusType();
            if (type == Granatier::Bonus::SLOW ||
                type == Granatier::Bonus::HYPERACTIVE ||
                type == Granatier::Bonus::MIRROR ||
                type == Granatier::Bonus::SCATTY ||
                type == Granatier::Bonus::RESTRAIN) {
                return true;
            }
        }
    }
    return false;
}

bool AIStrategyLevel1::isAdjacentToBlock(int row, int col)
{
    int rowOffsets[] = {-1, 1, 0, 0};
    int colOffsets[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        int nr = row + rowOffsets[i];
        int nc = col + colOffsets[i];
        if (nr >= 0 && nr < m_rows && nc >= 0 && nc < m_cols) {
            if (!m_arena->getCell(nr, nc).getElements(Granatier::Element::BLOCK).isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

bool AIStrategyLevel1::isAdjacentToEnemy(int row, int col)
{
    QList<Player*> players = m_ai->game()->getPlayers();
    int power = m_ai->player()->getBombPower();
    for (auto* other : players) {
        if (other == m_ai->player() || !other->isAlive()) continue;
        
        // Teammate check (team 0 means Solo, teams 1+ are shared teammate groups)
        if (m_ai->player()->team() != 0 && m_ai->player()->team() == other->team()) continue;

        int er = m_arena->getRowFromY(other->getY());
        int ec = m_arena->getColFromX(other->getX());

        if (row == er) {
            int dist = std::abs(col - ec);
            if (dist <= power) {
                // Check if there is a wall or block blocking the path
                bool blocked = false;
                int startCol = std::min(col, ec);
                int endCol = std::max(col, ec);
                for (int c = startCol + 1; c < endCol; ++c) {
                    if (m_arena->getCell(row, c).getType() == Granatier::Cell::WALL ||
                        !m_arena->getCell(row, c).getElements(Granatier::Element::BLOCK).isEmpty()) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) return true;
            }
        } else if (col == ec) {
            int dist = std::abs(row - er);
            if (dist <= power) {
                // Check if there is a wall or block blocking the path
                bool blocked = false;
                int startRow = std::min(row, er);
                int endRow = std::max(row, er);
                for (int r = startRow + 1; r < endRow; ++r) {
                    if (m_arena->getCell(r, col).getType() == Granatier::Cell::WALL ||
                        !m_arena->getCell(r, col).getElements(Granatier::Element::BLOCK).isEmpty()) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) return true;
            }
        }
    }
    return false;
}

QList<QPoint> AIStrategyLevel1::findPath(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType)
{
    // When escaping (targetType == 0), prioritize safe cells strictly.
    // If no path consists of strictly safe cells, fall back to allowing dangerous cells.
    if (targetType == 0) {
        QList<QPoint> path = findPathImpl(start, dangerGrid, targetType, true);
        if (!path.isEmpty()) return path;
    }
    return findPathImpl(start, dangerGrid, targetType, false);
}

QList<QPoint> AIStrategyLevel1::findPathImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlySafe)
{
    QList<GridNode> nodesList;
    std::queue<int> q;
    std::vector<std::vector<bool>> visited(m_rows, std::vector<bool>(m_cols, false));

    nodesList.append(GridNode(start.y(), start.x()));
    q.push(0);
    visited[start.y()][start.x()] = true;

    int rowOffsets[] = {-1, 1, 0, 0};
    int colOffsets[] = {0, 0, -1, 1};

    while (!q.empty()) {
        int currIdx = q.front();
        q.pop();
        GridNode curr = nodesList[currIdx];

        // Check target match
        bool isTarget = false;
        if (targetType == 0) { // Safe cell
            isTarget = (dangerGrid[curr.r][curr.c] == false);
        } else if (targetType == 1) { // Green bonus
            isTarget = isGreenBonus(curr.r, curr.c);
        } else if (targetType == 2) { // Destructible block adjacent
            isTarget = isAdjacentToBlock(curr.r, curr.c);
        } else if (targetType == 3) { // Enemy player adjacent
            isTarget = isAdjacentToEnemy(curr.r, curr.c);
        }

        if (isTarget) {
            // Reconstruct path
            QList<QPoint> path;
            int idx = currIdx;
            while (idx != -1) {
                path.prepend(QPoint(nodesList[idx].c, nodesList[idx].r));
                idx = nodesList[idx].parentIdx;
            }
            return path;
        }

        // Expand neighbors
        for (int i = 0; i < 4; ++i) {
            int nr = curr.r + rowOffsets[i];
            int nc = curr.c + colOffsets[i];
            if (nr >= 0 && nr < m_rows && nc >= 0 && nc < m_cols && !visited[nr][nc]) {
                bool isStartCell = (nr == start.y() && nc == start.x());
                bool walkable = (isStartCell || m_arena->getCell(nr, nc).isWalkable(m_ai->player())) &&
                                m_arena->getCell(nr, nc).getType() != Granatier::Cell::HOLE;
                if (walkable && !isBadBonus(nr, nc)) {
                    bool isSafe = (dangerGrid[nr][nc] == false);
                    // If strictlySafe is true, we ONLY step on safe cells (except start cell).
                    // If strictlySafe is false, we can step on dangerous cells if escaping.
                    if (isSafe || (!strictlySafe && targetType == 0)) {
                        visited[nr][nc] = true;
                        nodesList.append(GridNode(nr, nc, currIdx));
                        q.push(nodesList.count() - 1);
                    }
                }
            }
        }
    }

    return QList<QPoint>();
}

void AIStrategyLevel1::moveTowardsCell(const QPoint& targetCell)
{
    int myRow = m_arena->getRowFromY(m_ai->player()->getY());
    int myCol = m_arena->getColFromX(m_ai->player()->getX());

    if (targetCell.x() > myCol) {
        m_ai->player()->goRight();
    } else if (targetCell.x() < myCol) {
        m_ai->player()->goLeft();
    } else if (targetCell.y() > myRow) {
        m_ai->player()->goDown();
    } else if (targetCell.y() < myRow) {
        m_ai->player()->goUp();
    } else {
        // Already in the same cell, align to center of the target cell
        qreal currX = m_ai->player()->getX();
        qreal currY = m_ai->player()->getY();
        qreal targetCenterX = (targetCell.x() + 0.5) * Granatier::CellSize;
        qreal targetCenterY = (targetCell.y() + 0.5) * Granatier::CellSize;
        qreal dx = targetCenterX - currX;
        qreal dy = targetCenterY - currY;
        qreal tolerance = 5.0;

        if (std::abs(dx) > tolerance) {
            if (dx > 0) m_ai->player()->goRight();
            else m_ai->player()->goLeft();
        } else if (std::abs(dy) > tolerance) {
            if (dy > 0) m_ai->player()->goDown();
            else m_ai->player()->goUp();
        } else {
            m_ai->player()->stopMoving();
        }
    }
}

void AIStrategyLevel1::update()
{
    Arena* currentArena = m_ai->game()->getArena();
    if (m_arena != currentArena) {
        m_arena = currentArena;
        if (m_arena) {
            m_rows = m_arena->getNbRows();
            m_cols = m_arena->getNbColumns();
        } else {
            m_rows = 0;
            m_cols = 0;
        }
        m_currentTargetCell = QPoint(-1, -1);
        m_currentPath.clear();
    }

    if (!m_arena) return;

    int myRow = m_arena->getRowFromY(m_ai->player()->getY());
    int myCol = m_arena->getColFromX(m_ai->player()->getX());
    QPoint myCell(myCol, myRow);

    std::vector<std::vector<bool>> dangerGrid = computeDangerGrid();

    // Validate current target/path
    if (m_currentTargetCell.x() != -1 && m_currentTargetCell.y() != -1) {
        bool pathValid = true;
        if (m_currentTargetCell.x() < 0 || m_currentTargetCell.x() >= m_cols ||
            m_currentTargetCell.y() < 0 || m_currentTargetCell.y() >= m_rows) {
            pathValid = false;
        } else {
            bool isWalkable = m_arena->getCell(m_currentTargetCell.y(), m_currentTargetCell.x()).isWalkable(m_ai->player()) &&
                              m_arena->getCell(m_currentTargetCell.y(), m_currentTargetCell.x()).getType() != Granatier::Cell::HOLE;
            if (!isWalkable) {
                pathValid = false;
            }
            if (pathValid && dangerGrid[m_currentTargetCell.y()][m_currentTargetCell.x()] && !dangerGrid[myRow][myCol]) {
                pathValid = false;
            }
        }
        if (!pathValid) {
            m_currentTargetCell = QPoint(-1, -1);
            m_currentPath.clear();
            m_ai->player()->stopMoving();
        }
    }

    // 1. Check if we are currently standing in a dangerous cell
    if (dangerGrid[myRow][myCol] == true) {
        // Find path to closest safe cell
        QList<QPoint> escapePath = findPath(myCell, dangerGrid, 0);
        if (!escapePath.isEmpty() && escapePath.size() > 1) {
            m_currentPath = escapePath;
            m_currentTargetCell = escapePath[1];
            moveTowardsCell(m_currentTargetCell);
            return;
        }
    }

    // 2. Strategic Bombing lookahead
    bool adjacentToTarget = isAdjacentToBlock(myRow, myCol) || isAdjacentToEnemy(myRow, myCol);
    if (adjacentToTarget && m_ai->player()->bombArmory() > 0 && dangerGrid[myRow][myCol] == false) {
        // Simulate dropping bomb here
        std::vector<std::vector<bool>> simDanger = computeDangerGrid(myCell);
        QList<QPoint> simEscapePath = findPath(myCell, simDanger, 0);
        
        // Only drop if there is a valid simulated escape route, OR if we are trapped next to a block/enemy (no escape route exists but no bombs belonging to this player are active)
        bool hasOwnActiveBomb = false;
        QList<Bomb*> bombs = m_ai->game()->getBombs();
        for (auto* bomb : bombs) {
            if (!bomb->isDetonated() && bomb->creator() == m_ai->player()) {
                hasOwnActiveBomb = true;
                break;
            }
        }
        if ((!simEscapePath.isEmpty() && simEscapePath.size() > 1) || (!hasOwnActiveBomb && adjacentToTarget)) {
            m_ai->player()->dropBomb();
            if (!simEscapePath.isEmpty() && simEscapePath.size() > 1) {
                m_currentPath = simEscapePath;
                m_currentTargetCell = simEscapePath[1];
                moveTowardsCell(m_currentTargetCell);
            } else {
                m_currentTargetCell = QPoint(-1, -1);
                m_currentPath.clear();
                m_ai->player()->stopMoving();
            }
            return;
        }
    }

    // 3. Normal path traversal
    if (m_currentTargetCell.x() != -1 && m_currentTargetCell.y() != -1) {
        if (m_currentTargetCell == myCell && m_currentPath.size() <= 1) {
            m_ai->player()->stopMoving();
            m_currentTargetCell = QPoint(-1, -1);
            m_currentPath.clear();
        } else {
            qreal targetX = (m_currentTargetCell.x() + 0.5) * Granatier::CellSize;
            qreal targetY = (m_currentTargetCell.y() + 0.5) * Granatier::CellSize;
            qreal currX = m_ai->player()->getX();
            qreal currY = m_ai->player()->getY();
            if (std::abs(targetX - currX) <= 5.0 && std::abs(targetY - currY) <= 5.0) {
                // Reached target cell, pop path
                if (!m_currentPath.isEmpty()) {
                    m_currentPath.removeFirst();
                }
                if (!m_currentPath.isEmpty()) {
                    m_currentTargetCell = m_currentPath.first();
                } else {
                    m_currentTargetCell = QPoint(-1, -1);
                }
            }
        }
    }

    if (m_currentTargetCell.x() != -1 && m_currentTargetCell.y() != -1) {
        // Continue moving along current path
        moveTowardsCell(m_currentTargetCell);
        return;
    }

    // 4. Seek targets
    // Target Prioritization:
    // A. Green bonuses
    QList<QPoint> path = findPath(myCell, dangerGrid, 1);
    if (path.isEmpty()) {
        // B. Enemies
        path = findPath(myCell, dangerGrid, 3);
    }
    if (path.isEmpty()) {
        // C. Block clearing
        path = findPath(myCell, dangerGrid, 2);
    }
    if (path.isEmpty()) {
        // D. Seek closest enemy directly to get in range
        path = findPathToEnemyDirectly(myCell, dangerGrid);
    }

    if (!path.isEmpty() && path.size() > 1) {
        m_currentPath = path;
        m_currentTargetCell = path[1];
        moveTowardsCell(m_currentTargetCell);
    } else {
        // No path/targets found, stay idle and stop moving
        m_ai->player()->stopMoving();
    }
}

QList<QPoint> AIStrategyLevel1::findPathToEnemyDirectly(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid)
{
    QList<Player*> players = m_ai->game()->getPlayers();
    Player* closestEnemy = nullptr;
    int minDist = 999999;
    int er = -1, ec = -1;

    for (auto* other : players) {
        if (other == m_ai->player() || !other->isAlive()) continue;
        if (m_ai->player()->team() != 0 && m_ai->player()->team() == other->team()) continue;

        int r = m_arena->getRowFromY(other->getY());
        int c = m_arena->getColFromX(other->getX());
        int dist = std::abs(start.y() - r) + std::abs(start.x() - c);
        if (dist < minDist) {
            minDist = dist;
            closestEnemy = other;
            er = r;
            ec = c;
        }
    }

    if (!closestEnemy) return QList<QPoint>();

    // BFS to find path to er, ec
    QList<GridNode> nodesList;
    std::queue<int> q;
    std::vector<std::vector<bool>> visited(m_rows, std::vector<bool>(m_cols, false));

    nodesList.append(GridNode(start.y(), start.x()));
    q.push(0);
    visited[start.y()][start.x()] = true;

    int rowOffsets[] = {-1, 1, 0, 0};
    int colOffsets[] = {0, 0, -1, 1};

    while (!q.empty()) {
        int currIdx = q.front();
        q.pop();
        GridNode curr = nodesList[currIdx];

        if (curr.r == er && curr.c == ec) {
            // Reconstruct path
            QList<QPoint> path;
            int idx = currIdx;
            while (idx != -1) {
                path.prepend(QPoint(nodesList[idx].c, nodesList[idx].r));
                idx = nodesList[idx].parentIdx;
            }
            return path;
        }

        for (int i = 0; i < 4; ++i) {
            int nr = curr.r + rowOffsets[i];
            int nc = curr.c + colOffsets[i];
            if (nr >= 0 && nr < m_rows && nc >= 0 && nc < m_cols && !visited[nr][nc]) {
                bool isStartCell = (nr == start.y() && nc == start.x());
                bool walkable = (isStartCell || m_arena->getCell(nr, nc).isWalkable(m_ai->player())) &&
                                m_arena->getCell(nr, nc).getType() != Granatier::Cell::HOLE;
                if (walkable && !isBadBonus(nr, nc)) {
                    bool isSafe = (dangerGrid[nr][nc] == false);
                    if (isSafe) {
                        visited[nr][nc] = true;
                        nodesList.append(GridNode(nr, nc, currIdx));
                        q.push(nodesList.count() - 1);
                    }
                }
            }
        }
    }

    return QList<QPoint>();
}
