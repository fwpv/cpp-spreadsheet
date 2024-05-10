#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <functional>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression)
    : ast_(ParseFormulaAST(std::move(expression))) {
    }
    
    Value Evaluate(const SheetInterface& sheet) const override {
        // callback функция для извлечения значения ячейки
        std::function<CellInterface::Value(Position)> cell_value_getter
            = [&sheet](Position pos) {
                const CellInterface* cell_ = sheet.GetCell(pos);
                if (!cell_) { 
                    return CellInterface::Value{0.0};
                }
                return cell_->GetValue();
            };
        try {
            return ast_.Execute(cell_value_getter);
        } catch (const FormulaError& error) {
            return error;
        }
    }
    std::string GetExpression() const override {
        std::ostringstream oss;
        ast_.PrintFormula(oss);
        return oss.str();
    }

    std::vector<Position> GetReferencedCells() const {
        auto list_of_cells = ast_.GetCells();
        std::vector<Position> cells = {list_of_cells.begin(), list_of_cells.end()};
        auto it = std::unique(cells.begin(), cells.end());
        cells.erase(it, cells.end());
        return cells;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}