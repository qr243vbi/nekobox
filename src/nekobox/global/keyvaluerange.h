#ifndef KEY_VALUE_RANGE
#define KEY_VALUE_RANGE
#include <type_traits>
#include <utility>
template<typename T> class KeyValueRange {
private:
    T iterable; // This is either a reference or a moved-in value. The map data isn't copied.
public:
    KeyValueRange(T &iterable) : iterable(iterable) { }
    KeyValueRange(std::remove_reference_t<T> &&iterable) noexcept : iterable(std::move(iterable)) { }
    auto begin() const { return iterable.keyValueBegin(); }
    auto end() const { return iterable.keyValueEnd(); }
};

template <typename T> auto asKeyValueRange(T &iterable) { return KeyValueRange<T &>(iterable); }
template <typename T> auto asKeyValueRange(const T &iterable) { return KeyValueRange<const T &>(iterable); }
template <typename T> auto asKeyValueRange(T &&iterable) noexcept { return KeyValueRange<T>(std::move(iterable)); }


#include <QList>
#include <cstddef>

template<typename T>
class ListRange {
private:
    T list;

public:
    ListRange(T &iterable) : list(iterable) { }
    ListRange(std::remove_reference_t<T> &&iterable) noexcept : list(std::move(iterable)) { }

    class iterator {
        using ListType =
            std::remove_reference_t<T>;
        using BaseIter =
            std::conditional_t<
                std::is_const_v<ListType>,
                typename ListType::const_iterator,
                typename ListType::iterator>;

        BaseIter it;
        std::size_t index;

    public:
        iterator(BaseIter it, std::size_t index)
            : it(it), index(index) {}

        auto operator*() const {
            return std::pair<std::size_t, decltype(*it)>(index, *it);
        }

        iterator& operator++() {
            ++it;
            ++index;
            return *this;
        }

        bool operator!=(const iterator& other) const {
            return it != other.it;
        }
    };

    auto begin() {
        return iterator(list.begin(), 0);
    }

    auto end() {
        return iterator(list.end(), 0);
    }
};

template<typename T>
auto asListRange(QList<T>& list) {
    return ListRange<QList<T>&>(list);
}

template<typename T>
auto asListRange(const QList<T>& list) {
    return ListRange<const QList<T>&>(list);
}

template<typename T>
auto asListRange(QList<T>&& list) {
    return ListRange<QList<T>>(std::move(list));
}

#endif
