#pragma once

namespace uih::direct_write {
template <class CacheValue>
class TextLayoutCache {
public:
    template <typename String>
    struct GenericCacheKey {
        String text;
        float width{};
        float height{};
        bool enable_ellipses{};
        DWRITE_TEXT_ALIGNMENT alignment{};

        template <typename Other>
        Other as() const
        {
            return Other{decltype(Other::text)(text), width, height, enable_ellipses, alignment};
        }

        template <typename OtherString>
        bool operator==(const GenericCacheKey<OtherString>& other) const
        {
            return text == other.text && width == other.width && height == other.height
                && enable_ellipses == other.enable_ellipses && alignment == other.alignment;
        }
    };

    using CacheKey = GenericCacheKey<std::wstring>;
    using CacheKeyView = GenericCacheKey<std::wstring_view>;

    struct CacheKeyViewHash {
        size_t operator()(const CacheKeyView& key_view) const
        {
            constexpr auto make_hash = [](auto&& value) { return std::hash<std::decay_t<decltype(value)>>{}(value); };

            size_t hash{};
            hash ^= make_hash(key_view.text);
            hash ^= make_hash(key_view.width) << 1;
            hash ^= make_hash(key_view.height) << 2;
            hash ^= make_hash(key_view.enable_ellipses) << 3;
            hash ^= make_hash(key_view.alignment) << 4;

            return hash;
        }
    };

    explicit TextLayoutCache(size_t max_size) : m_max_size(max_size) {}

    bool contains(const CacheKeyView& key_view) { return m_cache_map.contains(key_view); }

    /**
     * Get an existing item from the cache if present.
     *
     * Creates a space for a new item if at capacity and the item is not found.
     */
    CacheValue get(const CacheKeyView& key_view)
    {
        if (m_max_size == 0)
            return {};

        auto iter = m_cache_map.find(key_view);

        if (iter == m_cache_map.end()) {
            shrink_one_if_at_capacity();
            return {};
        }

        m_cache_list.splice(m_cache_list.begin(), m_cache_list, iter->second);

        return iter->second->layout;
    }

    /**
     * Put a new item in the cache.
     *
     * Can only be used if the item is not already in the cache.
     */
    void put_new(const CacheKeyView& key_view, CacheValue layout)
    {
        if (m_max_size == 0)
            return;

        assert(!m_cache_map.contains(key_view));

        auto& stored_key = m_cache_list.emplace_front(key_view.template as<CacheKey>(), std::move(layout));
        m_cache_map.emplace(stored_key.key.template as<CacheKeyView>(), m_cache_list.begin());
    }

private:
    void shrink_one_if_at_capacity()
    {
        if (m_cache_list.size() < m_max_size)
            return;

        auto& lru_key = m_cache_list.back().key;
        m_cache_map.erase(lru_key.template as<CacheKeyView>());
        m_cache_list.pop_back();
    }

    struct CacheEntry {
        CacheKey key;
        CacheValue layout;
    };

    using CacheList = std::list<CacheEntry>;
    using CacheMap = std::unordered_map<CacheKeyView, typename CacheList::iterator, CacheKeyViewHash>;

    size_t m_max_size{};
    CacheList m_cache_list;
    CacheMap m_cache_map;
};

} // namespace uih::direct_write
