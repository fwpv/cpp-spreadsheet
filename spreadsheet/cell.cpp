#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

using namespace std::literals;

namespace CellImpl {

class Impl {
public:
    virtual ~Impl() = default;

    virtual CellInterface::Value GetValue(const SheetInterface& /*sheet_*/) const = 0;
    virtual std::string GetText() const = 0;

    virtual std::vector<Position> GetReferencedCells() const { return {}; }
};

class EmptyImpl : public Impl {
public:
    CellInterface::Value GetValue(const SheetInterface& /*sheet_*/) const override {
        return "";
    }
    std::string GetText() const override {
        return "";
    }
};

class TextImpl : public Impl {
public:
    TextImpl(std::string text)
    : text_(std::move(text)) {
    }

    CellInterface::Value GetValue(const SheetInterface& /*sheet_*/) const override {
        if (!text_.empty() && text_[0] == ESCAPE_SIGN) {
            return text_.substr(1);
        } else {
            return text_;
        }
    }

    std::string GetText() const override {
        return text_;
    }
private:
    std::string text_;
};

class FormulaImpl : public Impl {
public:
    FormulaImpl(std::string expression)
    : formula_(ParseFormula(std::move(expression))) {
    }

    CellInterface::Value GetValue(const SheetInterface& sheet_) const override {
        FormulaInterface::Value value = formula_->Evaluate(sheet_);
        if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        } else { // std::holds_alternative<FormulaError>(value)
            return std::get<FormulaError>(value);
        }
    }

    std::string GetText() const override {
        return "="s + formula_->GetExpression();
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }

private:
    std::unique_ptr<FormulaInterface> formula_;
};

}

Cell::Cell(const SheetInterface& sheet)
: impl_(std::make_unique<CellImpl::EmptyImpl>())
, sheet_(sheet) {
}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    if (text.empty()) {
        impl_ = std::make_unique<CellImpl::EmptyImpl>();
    } else if (text[0] == FORMULA_SIGN && text.size() > 1) {
        impl_ = std::make_unique<CellImpl::FormulaImpl>(text.substr(1));
    } else {
        impl_ = std::make_unique<CellImpl::TextImpl>(std::move(text));
    }
}

void Cell::Clear() {
    impl_ = std::make_unique<CellImpl::EmptyImpl>();
}

Cell::Value Cell::GetValue() const {
    if (!cache_.has_value()) {
        cache_ = impl_->GetValue(sheet_);
    }
    return cache_.value();
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

void Cell::ResetCache() const {
    cache_.reset();
}