#ifndef KEY_VALUE_RANGE
#define KEY_VALUE_RANGE
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

#endif
