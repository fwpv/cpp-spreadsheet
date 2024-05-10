#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

class DependencyGraph;

class Sheet : public SheetInterface {
public:
    Sheet();
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    const Cell* GetConcreteCell(Position pos) const;
    Cell* GetConcreteCell(Position pos);

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

private:
    std::unique_ptr<DependencyGraph> graph_;
	std::vector<std::vector<std::unique_ptr<Cell>>> cells_;
    Size printable_size_;

    static void ValidatePosition(Position pos);
    void PlaceCell(Position pos, std::unique_ptr<Cell> cell);
};