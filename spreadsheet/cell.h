#pragma once

#include "common.h"
#include "formula.h"

#include <optional>

namespace CellImpl {
class Impl;
}

class Cell : public CellInterface {
public:
    Cell(const SheetInterface& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;

    std::vector<Position> GetReferencedCells() const override;

    void ResetCache() const;

private:
    std::unique_ptr<CellImpl::Impl> impl_;
    const SheetInterface& sheet_;
    mutable std::optional<Value> cache_;
};