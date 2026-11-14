#ifndef TVISION_CONSTMAP_H
#define TVISION_CONSTMAP_H

#include <tvision/tv.h>
#include <internal/strings.h>

#include <unordered_map>

namespace tvision
{

// Аналог unordered_map с константным operator[].

template<typename Key, typename Value>
class const_unordered_map_base : public std::unordered_map<Key, Value>
{

    using super = std::unordered_map<Key, Value>;

public:

    using super::super;

    Value operator[](const Key &key) const noexcept
    {
        auto it = super::find(key);
        if (it == super::end())
            return {};
        return it->second;
    }

};

template<typename Key, typename Value>
class const_unordered_map : public const_unordered_map_base<Key, Value>
{

    using super = const_unordered_map_base<Key, Value>;

public:

    using super::super;

};

// Специализация const_unordered_map для Key=uint64_t с дополнительной
// функцией создания with_string_keys. Она позволяет использовать
// короткие строки в качестве ключей, переинтерпретируя их как целые
// числа на лету. Например, "012" превращается в (uint64_t)0x323130.
// Полный пример: const auto map = const_unordered_map<uint64_t,
// bool>::with_string_keys({ { "012", true }, }); assert(map["012"] ==
// true); assert(map[0x323130] == true); assert(map["abc"] == false);
//
//
//
//
//
//
//
//

template<typename Value>
class const_unordered_map<uint64_t, Value> : public const_unordered_map_base<uint64_t, Value>
{

    using super = const_unordered_map_base<uint64_t, Value>;

    struct StringAsIntPair : const_unordered_map::super::value_type
    {
        using super = typename const_unordered_map::super::value_type;

        constexpr StringAsIntPair() noexcept
        {
        }

        constexpr StringAsIntPair(TStringView s, Value v) noexcept :
            super(string_as_int<uint64_t>(s), (Value &&) v)
        {
        }
    };

public:

    using super::super;

    static const_unordered_map with_string_keys(std::initializer_list<StringAsIntPair> init) noexcept
    {
        return const_unordered_map(
            static_cast<const typename super::value_type *>(init.begin()),
            static_cast<const typename super::value_type *>(init.end())
        );
    }

    Value operator[](TStringView key) const noexcept
    {
        return super::operator[](string_as_int<uint64_t>(key));
    }

};

} // пространство имён tvision

#endif // TVISION_CONSTMAP_H
