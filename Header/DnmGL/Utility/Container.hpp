#pragma once

#include <algorithm>
#include <cstdint>

template <typename T>
class ContainerNonHandled {
public:
    class Iterator {
    public:
        constexpr Iterator(T* ptr) : ptr(ptr) {}
        ~Iterator() = default;

        Iterator(const Iterator& other)
            : ptr(other.ptr) {}

        constexpr T& operator*() { return *ptr; }
        constexpr T* operator->() { return ptr; }

        constexpr Iterator& operator++() { ++ptr; return *this; }
        constexpr bool operator!=(const Iterator& other) const { return (other.ptr != ptr); }
    private:
        T* ptr;
    };

    Iterator begin() const {
        return { m_ptr };
    }

    [[nodiscard]] Iterator end() const {
        return { m_ptr + m_elementCount };
    }

    ContainerNonHandled(const uint32_t capacity = 1)
        : m_capacity(std::max(capacity, 1u)), m_elementCount(0) {
        m_ptr = static_cast<T*>(malloc(capacity * sizeof(T)));
    }

    ContainerNonHandled(ContainerNonHandled&& other) noexcept
        : m_ptr(other.m_ptr),
        m_capacity(other.m_capacity),
        m_elementCount(other.m_elementCount) {
        other.m_ptr = nullptr;
    }

    ContainerNonHandled& operator=(ContainerNonHandled&& other) noexcept {
        if (this == &other)
            return *this;

        m_ptr = other.m_ptr;
        m_capacity = other.m_capacity;
        m_elementCount = other.m_elementCount;
        other.m_ptr = nullptr;

        return *this;
    }

    ~ContainerNonHandled() {
        if (m_ptr)
            free(m_ptr);
    }

    T& operator[](const uint32_t index) const {
        return GetElement(index);
    }

    ContainerNonHandled(const ContainerNonHandled&) = delete;
    ContainerNonHandled& operator=(const ContainerNonHandled&) = delete;

    template<typename U>
    requires std::same_as<std::remove_cvref_t<U>, T>
    uint32_t AddElement(U&& data) {
        if (m_elementCount >= m_capacity) {
            Reserve(m_capacity * 1.5 + 2 /* for m_capacity little values*/);
        }

        m_ptr[m_elementCount++] = std::forward<U>(data);

        return m_elementCount - 1;
    }

    void SetElement(const T& data, uint32_t i) {
        if (i >= m_elementCount) return;
        m_ptr[i] = data;
    }

    T& GetElement(uint32_t i) const {
        //capacity always more than 1
        if (i >= m_elementCount) return m_ptr[0];
        return m_ptr[i];
    }

    void DeleteElement(uint32_t i) {
        if (i >= m_elementCount--) return;
        if (i == m_elementCount) return;
        std::swap(m_ptr[i], m_ptr[m_elementCount]);
    }

    void Reserve(const uint32_t capacity) {
        if (capacity == 0) [[unlikely]] return;

        m_capacity += capacity;
        m_ptr = static_cast<T*>(realloc(m_ptr, m_capacity * sizeof(T)));
        if (m_ptr == nullptr) [[unlikely]] return;
    }

    [[nodiscard]] T* GetPtr() const {
        return m_ptr;
    }

    [[nodiscard]] uint32_t GetLastElementIndex() const {
        return m_elementCount - 1;
    }

    [[nodiscard]] T& GetLastElement() {
        return GetElement(GetLastElementIndex());
    }

    [[nodiscard]] uint32_t GetElementCount() const {
        return m_elementCount;
    }

    [[nodiscard]] uint32_t GetCapacity() const {
        return m_capacity;
    }
private:
    T* m_ptr;
    uint32_t m_capacity;
    uint32_t m_elementCount;
};

template <typename T>
class Container {
public:
    //pointers life cycle is managed by Array
    class Handle {
    public:
        Handle() = default;
        bool operator==(const Handle& other) const {
            return GetValue() == other.GetValue();
        }
    private:
        explicit Handle(uint32_t value) : value(new uint32_t(value)) {}

        void SetValue(const uint32_t newValue) const {
            *value = newValue;
        }

        [[nodiscard]] uint32_t GetValue() const {
            return *value;
        }

        void Invalidate() {
            delete value;
            value = nullptr;
        }

        bool isNull() const {
            return value == nullptr;
        }

        uint32_t* value{};
        friend class Container;
    };

    class Iterator {
    public:
        constexpr Iterator(T* ptr) : ptr(ptr) {}
        ~Iterator() = default;

        Iterator(const Iterator& other)
            : ptr(other.ptr) {}

        constexpr T& operator*() { return *ptr; }
        constexpr T* operator->() { return ptr; }

        constexpr Iterator& operator++() { ++ptr; return *this; }
        constexpr bool operator!=(const Iterator& other) const { return (other.ptr != ptr); }
    private:
        T* ptr;
    };

    Iterator begin() const {
        return { m_ptr };
    }

    [[nodiscard]] Iterator end() const {
        return { m_ptr + GetElementCount() };
    }

    Container(const uint32_t capacity = 1)
        : m_handles(capacity) {
        m_ptr = static_cast<T*>(malloc(capacity * sizeof(T)));
    }

    Container(Container&& other) noexcept
    : m_handles(std::move(other.m_handles)),
    m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    Container& operator=(Container&& other) noexcept {
        if (this == &other)
            return *this;

        m_ptr = other.m_ptr;
        m_handles = std::move(other.m_handles);
        other.m_ptr = nullptr;

        return *this;
    }

    ~Container() {
        for (auto& handle : m_handles) {
            handle.Invalidate();
        }

        if (m_ptr)
            free(m_ptr);
    }

    Handle AddElement(const T& element) {
        if (GetElementCount() >= GetCapacity()) {
            Reserve(GetCapacity() * 1.5 + 2 /* for m_capacity little values*/);
        }

        m_ptr[GetElementCount()] = element;

        auto outHandle = Handle(GetElementCount());

        m_handles.AddElement(std::move(outHandle));

        return std::move(outHandle);
    }

    void SetElement(const T& data, const Handle& handle) {
        if (handle.isNull() || (handle.GetValue() > GetElementCount()-1)) [[unlikely]] return;
        m_ptr[handle.GetValue()] = data;
    }

    T& GetElement(const Handle& handle) {
        //capacity always more than 1
        if (handle.isNull() || (handle.GetValue() > GetElementCount()-1)) [[unlikely]] return m_ptr[0];
        return m_ptr[handle.GetValue()];
    }

    void DeleteElement(Handle& handle) {
        if (handle.isNull() || (handle.GetValue() > GetElementCount()-1)) [[unlikely]] return;

        if (handle.GetValue() == m_handles.GetLastElement().GetValue()
            || GetElementCount() == 1) {

            m_handles.DeleteElement(handle.GetValue());
            handle.Invalidate();
            return;
        }

        m_ptr[handle.GetValue()] = m_ptr[m_handles.GetLastElement().GetValue()];
        auto& lastElementHandle = m_handles.GetLastElement();

        lastElementHandle.SetValue(handle.GetValue());

        m_handles.DeleteElement(handle.GetValue());
        handle.Invalidate();
    }

    void Reserve(const uint32_t capacity) {
        if (capacity == 0) [[unlikely]] return;

        m_handles.Reserve(capacity);

        m_ptr = static_cast<T*>(realloc(m_ptr, GetCapacity() * sizeof(T)));
        if (m_ptr == nullptr) [[unlikely]] return;
    }

    [[nodiscard]] T* GetPtr() const {
        return m_ptr;
    }

    [[nodiscard]] uint32_t GetElementCount() const {
        return m_handles.GetElementCount();
    }

    [[nodiscard]] uint32_t GetCapacity() const {
        return m_handles.GetCapacity();
    }

    [[nodiscard]] const auto& GetHandles() const {
        return m_handles;
    }
private:
    ContainerNonHandled<Handle> m_handles;
    T* m_ptr;
};