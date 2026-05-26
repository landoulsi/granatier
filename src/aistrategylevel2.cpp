/*
    SPDX-FileCopyrightText: 2026 Ahmed Landoulsi <landoulsi.ahmed@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aistrategylevel2.h"
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

AIStrategyLevel2::AIStrategyLevel2(PlayerAI* ai)
    : AIStrategy(ai)
    , m_arena(nullptr)
    , m_rows(0)
    , m_cols(0)
{
    m_currentTargetCell = QPoint(-1, -1);
}

AIStrategyLevel2::~AIStrategyLevel2()
{
    if (m_ai && m_ai->player()) {
        m_ai->player()->stopMoving();
    }
}

std::vector<std::vector<bool>> AIStrategyLevel2::computeDangerGrid(const QPoint& simulatedBombPos)
{
    std::vector<std::vector<bool>> danger(m_rows, std::vector<bool>(m_cols, false));
    QList<Bomb*> bombs = m_ai->game()->getBombs();

    // Map existing bombs
    for (auto* bomb : bombs) {
        // CRITICAL LEVEL 2 FIX: Do NOT skip detonated bombs!
        // Lingering fire is extremely dangerous, so the AI stays out of the blast
        // radius until the explosion completely finishes and the bomb object is removed.
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

bool AIStrategyLevel2::isGreenBonus(int row, int col)
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

bool AIStrategyLevel2::isBadBonus(int row, int col)
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

bool AIStrategyLevel2::isShieldBonus(int row, int col)
{
    QList<Element*> elements = m_arena->getCell(row, col).getElements(Granatier::Element::BONUS);
    for (auto* element : elements) {
        auto* bonus = dynamic_cast<Bonus*>(element);
        if (bonus && !bonus->isTaken()) {
            if (bonus->getBonusType() == Granatier::Bonus::SHIELD) {
                return true;
            }
        }
    }
    return false;
}

bool AIStrategyLevel2::hasGreenBonusNearby(int row, int col, int maxDist)
{
    std::queue<std::pair<QPoint, int>> q;
    std::vector<std::vector<bool>> visited(m_rows, std::vector<bool>(m_cols, false));
    q.push({QPoint(col, row), 0});
    visited[row][col] = true;

    int rowOffsets[] = {-1, 1, 0, 0};
    int colOffsets[] = {0, 0, -1, 1};

    while (!q.empty()) {
        auto pair = q.front();
        q.pop();
        QPoint curr = pair.first;
        int dist = pair.second;

        if (dist > 0 && isGreenBonus(curr.y(), curr.x())) {
            return true;
        }

        if (dist < maxDist) {
            for (int i = 0; i < 4; ++i) {
                int nr = curr.y() + rowOffsets[i];
                int nc = curr.x() + colOffsets[i];
                if (nr >= 0 && nr < m_rows && nc >= 0 && nc < m_cols && !visited[nr][nc]) {
                    bool walkable = m_arena->getCell(nr, nc).isWalkable(m_ai->player()) &&
                                    m_arena->getCell(nr, nc).getType() != Granatier::Cell::HOLE;
                    if (walkable) {
                        visited[nr][nc] = true;
                        q.push({QPoint(nc, nr), dist + 1});
                    }
                }
            }
        }
    }
    return false;
}

bool AIStrategyLevel2::isAdjacentToBlock(int row, int col)
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

bool AIStrategyLevel2::isAdjacentToEnemy(int row, int col)
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

bool AIStrategyLevel2::isAdjacentToBadBonus(int row, int col)
{
    int rowOffsets[] = {-1, 1, 0, 0};
    int colOffsets[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        int nr = row + rowOffsets[i];
        int nc = col + colOffsets[i];
        if (nr >= 0 && nr < m_rows && nc >= 0 && nc < m_cols) {
            if (isBadBonus(nr, nc)) {
                return true;
            }
        }
    }
    return false;
}

int AIStrategyLevel2::countWalkableNeighbors(int row, int col)
{
    int count = 0;
    int rowOffsets[] = {-1, 1, 0, 0};
    int colOffsets[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        int nr = row + rowOffsets[i];
        int nc = col + colOffsets[i];
        if (nr >= 0 && nr < m_rows && nc >= 0 && nc < m_cols) {
            bool isStartCell = (nr == m_arena->getRowFromY(m_ai->player()->getY()) && nc == m_arena->getColFromX(m_ai->player()->getX()));
            bool walkable = (isStartCell || m_arena->getCell(nr, nc).isWalkable(m_ai->player())) &&
                            m_arena->getCell(nr, nc).getType() != Granatier::Cell::HOLE;
            if (walkable) {
                count++;
            }
        }
    }
    return count;
}

QList<QPoint> AIStrategyLevel2::findPath(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlyAvoidDeadEnds)
{
    // Tier 1: Try to find path by strictly avoiding bad/red bonuses
    if (targetType == 0) {
        QList<QPoint> path = findPathImpl(start, dangerGrid, targetType, true, true, strictlyAvoidDeadEnds);
        if (!path.isEmpty()) return path;
    }
    QList<QPoint> path = findPathImpl(start, dangerGrid, targetType, false, true, strictlyAvoidDeadEnds);
    if (!path.isEmpty()) return path;

    // Tier 2 Fallback: Allow stepping on bad/red bonuses since we are blocked
    if (targetType == 0) {
        QList<QPoint> path = findPathImpl(start, dangerGrid, targetType, true, false, strictlyAvoidDeadEnds);
        if (!path.isEmpty()) return path;
    }
    return findPathImpl(start, dangerGrid, targetType, false, false, strictlyAvoidDeadEnds);
}
QList<QPoint> AIStrategyLevel2::findPathImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, int targetType, bool strictlySafe, bool strictlyAvoidBadBonuses, bool strictlyAvoidDeadEnds)
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
        } else if (targetType == 4) { // Shield bonus
            isTarget = isShieldBonus(curr.r, curr.c);
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
                if (walkable) {
                    bool allowStep = !strictlyAvoidBadBonuses || !isBadBonus(nr, nc);
                    if (allowStep && strictlyAvoidDeadEnds) {
                        if (countWalkableNeighbors(nr, nc) <= 1) {
                            bool isTargetOrAdjacent = false;
                            if (targetType == 0) {
                                isTargetOrAdjacent = (dangerGrid[nr][nc] == false);
                            } else if (targetType == 1) {
                                isTargetOrAdjacent = isGreenBonus(nr, nc);
                            } else if (targetType == 2) {
                                isTargetOrAdjacent = isAdjacentToBlock(nr, nc);
                            } else if (targetType == 3) {
                                isTargetOrAdjacent = isAdjacentToEnemy(nr, nc);
                            } else if (targetType == 4) {
                                isTargetOrAdjacent = isShieldBonus(nr, nc);
                            }
                            if (!isTargetOrAdjacent) {
                                allowStep = false;
                            }
                        }
                    }
                    if (allowStep) {
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
    }

    return QList<QPoint>();
}

void AIStrategyLevel2::moveTowardsCell(const QPoint& targetCell)
{
    int myRow = m_arena->getRowFromY(m_ai->player()->getY());
    int myCol = m_arena->getColFromX(m_ai->player()->getX());

    qreal targetCenterX = (targetCell.x() + 0.5) * Granatier::CellSize;
    qreal targetCenterY = (targetCell.y() + 0.5) * Granatier::CellSize;
    qreal myCenterX = (myCol + 0.5) * Granatier::CellSize;
    qreal myCenterY = (myRow + 0.5) * Granatier::CellSize;

    qreal currX = m_ai->player()->getX();
    qreal currY = m_ai->player()->getY();

    qreal tolerance = 5.0;

    if (targetCell.x() != myCol && targetCell.y() != myRow) {
        // Diagonal target should not happen in BFS, but if it does, align to current cell center first
        qreal dx = myCenterX - currX;
        qreal dy = myCenterY - currY;
        if (std::abs(dx) > tolerance) {
            if (dx > 0) m_ai->player()->goRight();
            else m_ai->player()->goLeft();
        } else if (std::abs(dy) > tolerance) {
            if (dy > 0) m_ai->player()->goDown();
            else m_ai->player()->goUp();
        } else {
            m_ai->player()->stopMoving();
        }
        return;
    }

    if (targetCell.x() > myCol) {
        // We want to move Right. First align Vertically to our current cell center!
        qreal dy = myCenterY - currY;
        if (std::abs(dy) > tolerance) {
            if (dy > 0) m_ai->player()->goDown();
            else m_ai->player()->goUp();
        } else {
            m_ai->player()->goRight();
        }
    } else if (targetCell.x() < myCol) {
        // We want to move Left. First align Vertically to our current cell center!
        qreal dy = myCenterY - currY;
        if (std::abs(dy) > tolerance) {
            if (dy > 0) m_ai->player()->goDown();
            else m_ai->player()->goUp();
        } else {
            m_ai->player()->goLeft();
        }
    } else if (targetCell.y() > myRow) {
        // We want to move Down. First align Horizontally to our current cell center!
        qreal dx = myCenterX - currX;
        if (std::abs(dx) > tolerance) {
            if (dx > 0) m_ai->player()->goRight();
            else m_ai->player()->goLeft();
        } else {
            m_ai->player()->goDown();
        }
    } else if (targetCell.y() < myRow) {
        // We want to move Up. First align Horizontally to our current cell center!
        qreal dx = myCenterX - currX;
        if (std::abs(dx) > tolerance) {
            if (dx > 0) m_ai->player()->goRight();
            else m_ai->player()->goLeft();
        } else {
            m_ai->player()->goUp();
        }
    } else {
        // If we are already in the same cell, align to center of the target cell
        qreal dx = targetCenterX - currX;
        qreal dy = targetCenterY - currY;

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

void AIStrategyLevel2::update()
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

    // Compute distance to closest alive enemy player
    int closestEnemyDistance = 999999;
    QList<Player*> players = m_ai->game()->getPlayers();
    for (auto* other : players) {
        if (other == m_ai->player() || !other->isAlive()) continue;
        if (m_ai->player()->team() != 0 && m_ai->player()->team() == other->team()) continue;

        int r = m_arena->getRowFromY(other->getY());
        int c = m_arena->getColFromX(other->getX());
        int dist = std::abs(myRow - r) + std::abs(myCol - c);
        if (dist < closestEnemyDistance) {
            closestEnemyDistance = dist;
        }
    }

    // Trigger corridor/dead-end avoidance when threats are close
    bool strictlyAvoidDeadEnds = (closestEnemyDistance <= 4);

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

        // Pivot: If we are currently heading for a non-green bonus target (e.g. block or enemy), 
        // but there is a green bonus nearby (distance <= 3), discard the old path to grab it immediately!
        if (pathValid) {
            bool pathContainsGreenBonus = false;
            for (const auto& pt : m_currentPath) {
                if (isGreenBonus(pt.y(), pt.x())) {
                    pathContainsGreenBonus = true;
                    break;
                }
            }
            if (!pathContainsGreenBonus) {
                if (hasGreenBonusNearby(myRow, myCol, 3)) {
                    pathValid = false; // Discard path to trigger target re-evaluation!
                }
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
        // Find path to closest safe cell (always allow fleeing even to dead ends to save our life)
        QList<QPoint> escapePath = findPath(myCell, dangerGrid, 0, false);
        if (!escapePath.isEmpty() && escapePath.size() > 1) {
            m_currentPath = escapePath;
            m_currentTargetCell = escapePath[1];
            moveTowardsCell(m_currentTargetCell);
            return;
        }
    }

    // 2. Strategic Bombing lookahead (Level 2: Blocks, Enemies, AND Bad/Red Bonuses blocking corridors)
    bool adjacentToTarget = isAdjacentToBlock(myRow, myCol) || isAdjacentToEnemy(myRow, myCol) || isAdjacentToBadBonus(myRow, myCol);
    if (adjacentToTarget && m_ai->player()->bombArmory() > 0 && dangerGrid[myRow][myCol] == false) {
        // Simulate dropping bomb here
        std::vector<std::vector<bool>> simDanger = computeDangerGrid(myCell);

        // Prevent destroying green bonuses by checking the simulated blast zone
        bool greenBonusInBlast = false;
        for (int r = 0; r < m_rows; ++r) {
            for (int c = 0; c < m_cols; ++c) {
                if (simDanger[r][c] && isGreenBonus(r, c)) {
                    greenBonusInBlast = true;
                    break;
                }
            }
            if (greenBonusInBlast) break;
        }

        if (!greenBonusInBlast) {
            QList<QPoint> simEscapePath = findPath(myCell, simDanger, 0, strictlyAvoidDeadEnds);
            
            // Strict safety: Only place bomb if we have a valid simulated escape route
            if (!simEscapePath.isEmpty() && simEscapePath.size() > 1) {
                m_ai->player()->dropBomb();
                m_currentPath = simEscapePath;
                m_currentTargetCell = simEscapePath[1];
                moveTowardsCell(m_currentTargetCell);
                return;
            }
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
    QList<QPoint> path;

    // A. Priority Life Saver: Seek Shield bonus if we don't have one active and it is reachable
    bool seekShield = !m_ai->player()->hasShield();
    if (seekShield) {
        QList<QPoint> shieldPath = findPath(myCell, dangerGrid, 4, strictlyAvoidDeadEnds);
        if (!shieldPath.isEmpty() && shieldPath.size() > 1) {
            path = shieldPath;
        }
    }

    // B. Priority Collect: Seek ANY green bonus if it is extremely close (distance <= 3, path size <= 4)
    if (path.isEmpty()) {
        QList<QPoint> closeGreenPath = findPath(myCell, dangerGrid, 1, strictlyAvoidDeadEnds);
        if (!closeGreenPath.isEmpty() && closeGreenPath.size() > 1 && closeGreenPath.size() <= 4) {
            path = closeGreenPath;
        }
    }

    if (path.isEmpty()) {
        if (closestEnemyDistance <= 3) {
            // High Aggression: Target nearby enemies first
            path = findPath(myCell, dangerGrid, 3, strictlyAvoidDeadEnds);
            if (path.isEmpty()) {
                path = findPath(myCell, dangerGrid, 1, strictlyAvoidDeadEnds); // Green bonuses
            }
        } else {
            // High Development: Target green bonuses first
            path = findPath(myCell, dangerGrid, 1, strictlyAvoidDeadEnds);
            if (path.isEmpty()) {
                path = findPath(myCell, dangerGrid, 3, strictlyAvoidDeadEnds); // Enemies
            }
        }
    }

    if (path.isEmpty()) {
        // C. Block clearing
        path = findPath(myCell, dangerGrid, 2, strictlyAvoidDeadEnds);
    }
    if (path.isEmpty()) {
        // D. Seek closest enemy directly to get in range
        path = findPathToEnemyDirectly(myCell, dangerGrid, strictlyAvoidDeadEnds);
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

QList<QPoint> AIStrategyLevel2::findPathToEnemyDirectly(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, bool strictlyAvoidDeadEnds)
{
    // Tier 1: Try to reach enemy strictly avoiding bad/red bonuses
    QList<QPoint> path = findPathToEnemyDirectlyImpl(start, dangerGrid, true, strictlyAvoidDeadEnds);
    if (!path.isEmpty()) return path;

    // Tier 2 Fallback: Allow stepping on bad/red bonuses since we are blocked
    return findPathToEnemyDirectlyImpl(start, dangerGrid, false, strictlyAvoidDeadEnds);
}

QList<QPoint> AIStrategyLevel2::findPathToEnemyDirectlyImpl(const QPoint& start, const std::vector<std::vector<bool>>& dangerGrid, bool strictlyAvoidBadBonuses, bool strictlyAvoidDeadEnds)
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
                if (walkable) {
                    bool allowStep = !strictlyAvoidBadBonuses || !isBadBonus(nr, nc);
                    if (allowStep && strictlyAvoidDeadEnds) {
                        if (countWalkableNeighbors(nr, nc) <= 1) {
                            bool isTargetOrAdjacent = (nr == er && nc == ec) || (std::abs(nr - er) + std::abs(nc - ec) <= 1);
                            if (!isTargetOrAdjacent) {
                                allowStep = false;
                            }
                        }
                    }
                    if (allowStep) {
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
    }

    return QList<QPoint>();
}
