#ifndef TVISION_JSON_H
#define TVISION_JSON_H
#include <stdexcept>
#include <tvision/datamodel.h>
#include <utility>
namespace tvision
{
struct Json
{
    enum class Kind
    {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };
    Kind kind{Kind::Null};
    bool boolean{false};
    std::string text;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;
    static Json string(std::string value);
    static Json number(std::string value);
    static Json flag(bool value);
    static Json list(std::vector<Json> value = {});
    static Json record(std::vector<std::pair<std::string, Json>> value = {});
    const Json *find(const std::string &key) const;
    Json *find(const std::string &key);
    std::string display() const;
};
struct JsonError : std::runtime_error
{
    size_t offset, line, column;
    bool cancelled;
    JsonError(const std::string &message, size_t offset, size_t line, size_t column,
              bool cancelled = false);
};
using JsonProgress = std::function<bool(size_t, size_t)>;
Json parseJson(const std::string &text, JsonProgress progress = {});
std::string writeJson(const Json &value, bool pretty = false);
class JsonTableModel : public TableModel
{
    Json data_;
    std::vector<std::string> columns_;
    bool editable_{false};

  public:
    explicit JsonTableModel(Json data);
    size_t rowCount() const override
    {
        return data_.array.size();
    }
    size_t columnCount() const override
    {
        return columns_.size();
    }
    std::string cell(size_t, size_t) const override;
    std::string header(size_t) const override;
    void setEditable(bool value)
    {
        editable_ = value;
    }
    bool editable(size_t r, size_t c) const override
    {
        return editable_ && r < rowCount() && c < columnCount();
    }
    bool setCell(size_t, size_t, const std::string &) override;
    bool sort(size_t, bool) override;
    const Json &data() const
    {
        return data_;
    }
};
} // пространство имён tvision
#endif
