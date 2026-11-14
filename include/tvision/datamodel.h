#ifndef TVISION_DATAMODEL_H
#define TVISION_DATAMODEL_H
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace tvision
{
std::string columnLetters(size_t column);
// Числовые ячейки сравниваются по точному десятичному значению, включая большие целые числа.
int naturalCompare(const std::string &a, const std::string &b);
std::vector<std::vector<std::string>> parseCsv(const std::string &text, char delimiter = ',');
std::string writeCsv(const std::vector<std::vector<std::string>> &rows, char delimiter = ',');
class TableModel
{
  public:
    virtual ~TableModel() = default;
    virtual size_t rowCount() const = 0;
    virtual size_t columnCount() const = 0;
    virtual std::string cell(size_t row, size_t column) const = 0;
    virtual std::string header(size_t column) const
    {
        return columnLetters(column);
    }
    virtual bool editable(size_t, size_t) const
    {
        return false;
    }
    virtual bool setCell(size_t, size_t, const std::string &)
    {
        return false;
    }
    virtual bool sort(size_t, bool)
    {
        return false;
    }
};
class VecTableModel : public TableModel
{
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
    size_t columns_;
    bool editable_{false};

  public:
    VecTableModel(std::vector<std::string> headers = {},
                  std::vector<std::vector<std::string>> rows = {});
    size_t rowCount() const override
    {
        return rows_.size();
    }
    size_t columnCount() const override
    {
        return columns_;
    }
    std::string cell(size_t, size_t) const override;
    std::string header(size_t) const override;
    bool editable(size_t r, size_t c) const override
    {
        return editable_ && r < rowCount() && c < columnCount();
    }
    void setEditable(bool value)
    {
        editable_ = value;
    }
    bool setCell(size_t, size_t, const std::string &) override;
    bool sort(size_t, bool) override;
    const std::vector<std::vector<std::string>> &rows() const
    {
        return rows_;
    }
};
class FnTableModel : public TableModel
{
    size_t rows_, columns_;
    std::function<std::string(size_t, size_t)> getter_;
    std::function<std::string(size_t)> header_;

  public:
    FnTableModel(size_t rows, size_t columns, std::function<std::string(size_t, size_t)> getter,
                 std::function<std::string(size_t)> header = {});
    size_t rowCount() const override
    {
        return rows_;
    }
    size_t columnCount() const override
    {
        return columns_;
    }
    std::string cell(size_t r, size_t c) const override;
    std::string header(size_t c) const override;
};
} // пространство имён tvision
#endif
