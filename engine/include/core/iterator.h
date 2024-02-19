#define NK_DEFINE_ITERATOR(T, iterator_category_tag)                   \
    struct Iterator {                                                  \
    public:                                                            \
        using iterator_category = iterator_category_tag;               \
        using difference_type = std::ptrdiff_t;                        \
        using value_type = T;                                          \
        using pointer = T*;                                            \
        using reference = T&;                                          \
                                                                       \
        Iterator(pointer ptr) : m_ptr{ptr} {}                          \
                                                                       \
        reference operator*() const { return *m_ptr; }                 \
        pointer operator->() { return m_ptr; }                         \
                                                                       \
        Iterator& operator++() {                                       \
            m_ptr++;                                                   \
            return *this;                                              \
        }                                                              \
                                                                       \
        Iterator operator++(i32) {                                     \
            Iterator tmp = *this;                                      \
            ++(*this);                                                 \
            return tmp;                                                \
        }                                                              \
                                                                       \
        friend bool operator==(const Iterator& a, const Iterator& b) { \
            return a.m_ptr == b.m_ptr;                                 \
        }                                                              \
                                                                       \
        friend bool operator!=(const Iterator& a, const Iterator& b) { \
            return a.m_ptr != b.m_ptr;                                 \
        }                                                              \
                                                                       \
    private:                                                           \
        pointer m_ptr;                                                 \
    }
