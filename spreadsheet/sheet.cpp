#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <assert.h>
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

//-------------------DependencyGraph-----------------------

class DependencyGraph {
public:
    struct Node {
        Position cell_;
        std::unordered_set<Node*> forward_;
        std::unordered_set<Node*> backward_;
    };

    void AddDependency(Position from, Position to);
    void RemoveDependency(Position from, Position to);
    bool Contains(Position cell) const;
    void AddCell(Position cell);
    void RemoveCell(Position cell);
    bool CheckCyclicDependencies(Position cell);
    void ResetCache(Position cell, std::function<void(Position)>& reseter);

private:
    struct PositionHasher {
        std::size_t operator()(Position p) const noexcept {
            return p.row * Position::MAX_ROWS + p.col;
        }
    };
    std::unordered_map<Position, Node, PositionHasher> nodes_;
};

void DependencyGraph::AddDependency(Position from, Position to) {
    assert(nodes_.count(from));
    assert(nodes_.count(to));

    nodes_[from].forward_.insert(&nodes_[to]);
    nodes_[to].backward_.insert(&nodes_[from]);
}

void DependencyGraph::RemoveDependency(Position from, Position to) {
    auto it_from = nodes_.find(from);
    auto it_to = nodes_.find(to);

    assert(it_from != nodes_.end());
    assert(it_to != nodes_.end());

    it_from->second.forward_.erase(&nodes_[to]);
    it_to->second.backward_.erase(&nodes_[from]);
}

bool DependencyGraph::Contains(Position cell) const {
    return nodes_.find(cell) != nodes_.end();
}

void DependencyGraph::AddCell(Position cell) {
    if (!Contains(cell)) {
        nodes_[cell].cell_ = cell;
    }
}

void DependencyGraph::RemoveCell(Position cell) {
    auto it = nodes_.find(cell);
    if (it != nodes_.end()) {
        for (auto& forward_node : it->second.forward_) {
            forward_node->backward_.erase(&it->second);
        }
        for (auto& backward_node : it->second.backward_) {
            backward_node->forward_.erase(&it->second);
        }
        nodes_.erase(it);
    }
}

bool DependencyGraph::CheckCyclicDependencies(Position cell) {
    std::unordered_set<const Node*> verified_nodes;
    auto it = nodes_.find(cell);
    assert(it != nodes_.end());
    const Node* start_node = &it->second;

    std::function<bool(const Node*)> check_node = [&](const Node* current) {
        for (const Node* next_node : current->forward_) {
            if (verified_nodes.count(next_node)) {
                continue;
            }
            if (next_node == start_node) {
                return false;
            } else {
                if (!check_node(next_node)) {
                    return false;
                } else {
                    verified_nodes.insert(next_node);
                }
            }
        }
        return true; // циклических зависимостей нет
    };
    return check_node(start_node);
}

void DependencyGraph::ResetCache(Position cell, std::function<void(Position)>& reseter) {
    std::unordered_set<const Node*> verified_nodes;
    auto it = nodes_.find(cell);
    assert(it != nodes_.end());
    const Node* start_node = &it->second;

    std::function<void(const Node*)> reset_node = [&](const Node* current) {
        reseter(current->cell_);
        for (const Node* next_node : current->backward_) {
            if (verified_nodes.count(next_node)) {
                continue;
            }
            reset_node(next_node);
            verified_nodes.insert(next_node);
        }
    };
    reset_node(start_node);
}

//------------------------Sheet----------------------------

Sheet::Sheet()
: graph_(std::make_unique<DependencyGraph>()) {
}

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    ValidatePosition(pos);

    // Создать новую ячейку
    std::unique_ptr<Cell> new_cell = std::make_unique<Cell>(*this);
    new_cell->Set(std::move((text)));
    
    std::vector<Position> new_poses = new_cell->GetReferencedCells();
    std::vector<Position> new_empty_poses;
    for (Position next : new_poses) {
        if (next == pos) {
            std::string msg = "Сell references itself";
            throw CircularDependencyException(msg);
        }

        // Создать для позиций на которые ссылается новая ячейка пустые ячейки
        CellInterface* next_cell = GetCell(next);
        if (!next_cell) {
            std::unique_ptr<Cell> new_empty_cell = std::make_unique<Cell>(*this);
            PlaceCell(next, std::move(new_empty_cell));
            new_empty_poses.push_back(next);
        }
    }

    bool contained = graph_->Contains(pos);
    std::vector<Position> old_poses;
    if (contained) {
        Cell* old_cell = GetConcreteCell(pos);
        assert(old_cell);
        old_poses = old_cell->GetReferencedCells();

        // Удалить зависимости старой ячейки с графа
        for (Position next : old_poses) {
            graph_->RemoveDependency(pos, next);
        }
    }

    // Добавить на граф новую ячейку
    graph_->AddCell(pos);

    // Добавить зависимости новой ячейки на граф
    for (Position next : new_poses) {
        graph_->AddCell(next);
        graph_->AddDependency(pos, next);
    }

    if (contained) {
        if (!graph_->CheckCyclicDependencies(pos)) {
            // Удалить временно созданные пустые ячейки с листа
            for (Position next : new_empty_poses) {
                ClearCell(next);
            }

            // Вернуть зависимости старой ячейки
            for (Position next : old_poses) {
                graph_->AddCell(next);
                graph_->AddDependency(pos, next);
            }
            std::string msg = "Attempt to add a cell resulted in circular references";
            throw CircularDependencyException(msg);
        } else {
            
            // callback функция для сброса кэша ячейки
            std::function<void(Position)> reseter
                = [this](Position pos) {
                    const Cell* cell_ = this->GetConcreteCell(pos);
                    assert(cell_);
                    cell_->ResetCache();
                };

            // Сбросить кэш
            graph_->ResetCache(pos, reseter);
        }
    }

    // Заменить старую ячейку на новую в листе
    PlaceCell(pos, std::move(new_cell));
}

const CellInterface* Sheet::GetCell(Position pos) const {
    return const_cast<Sheet*>(this)->GetCell(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
    ValidatePosition(pos);
    return GetConcreteCell(pos);
}

const Cell* Sheet::GetConcreteCell(Position pos) const {
    return const_cast<Sheet*>(this)->GetConcreteCell(pos);
}

Cell* Sheet::GetConcreteCell(Position pos) {
    if (pos.row < static_cast<int>(cells_.size())) {
        std::vector<std::unique_ptr<Cell>>& row = cells_[pos.row];
        if (pos.col < static_cast<int>(row.size())) {
            return row[pos.col].get();
        }
    }
    return nullptr;
}

void Sheet::ClearCell(Position pos) {
    ValidatePosition(pos);

    if (pos.row < static_cast<int>(cells_.size())) {
        std::vector<std::unique_ptr<Cell>>& row = cells_[pos.row];
        if (pos.col < static_cast<int>(row.size())) {
            row[pos.col].reset();
            if (pos.row + 1 == printable_size_.rows || pos.col + 1 == printable_size_.cols) {
                for (int r = printable_size_.rows; r >= 0; --r) {
                    std::vector<std::unique_ptr<Cell>>& row = cells_[r];
                    for (int c = printable_size_.cols; c >= 0; --c) {
                        if (c < static_cast<int>(row.size()) && row[c] != nullptr) {
                            printable_size_ = {r + 1, c + 1};
                            return;
                        }
                    }
                }
                printable_size_ = {0, 0};
            }
        }
    }    
}

Size Sheet::GetPrintableSize() const {
    return printable_size_;
}

void Sheet::PrintValues(std::ostream& output) const {
    for (int r = 0; r < printable_size_.rows; ++r) {
        const std::vector<std::unique_ptr<Cell>>& row = cells_[r];
        for (int c = 0; c < printable_size_.cols; ++c) {
            if (c > 0) {
                output << "\t";
            }
            if (c < static_cast<int>(row.size()) && row[c] != nullptr) {
                Cell::Value value = row[c]->GetValue();
                std::visit([&output](const auto &elem) { output << elem; }, value);
            } else {
                output << "";
            }
        }
        output << "\n";
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    for (int r = 0; r < printable_size_.rows; ++r) {
        const std::vector<std::unique_ptr<Cell>>& row = cells_[r];
        for (int c = 0; c < printable_size_.cols; ++c) {
            if (c > 0) {
                output << "\t";
            }
            if (c < static_cast<int>(row.size()) && row[c] != nullptr) {
                output << row[c]->GetText();
            } else {
                output << "";
            }
        }
        output << "\n";
    }
}

void Sheet::ValidatePosition(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position: row = "s + std::to_string(pos.row)
            + ", col = "s + std::to_string(pos.col));
    }
}

void Sheet::PlaceCell(Position pos, std::unique_ptr<Cell> cell) {
    if (pos.row >= static_cast<int>(cells_.size())) {
        cells_.resize(pos.row + 1);
    }
    std::vector<std::unique_ptr<Cell>>& row = cells_[pos.row];
    if (pos.col >= static_cast<int>(row.size())) {
        row.resize(pos.col + 1);
    }
    row[pos.col] = std::move(cell);
    printable_size_.rows = std::max(printable_size_.rows, pos.row + 1);
    printable_size_.cols = std::max(printable_size_.cols, pos.col + 1);
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}