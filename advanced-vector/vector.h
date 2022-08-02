#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity))
            , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::move(other.buffer_);
        other.buffer_ = nullptr;
        capacity_ = std::move(other.capacity_);
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::move(rhs.buffer_);
        rhs.buffer_ = nullptr;
        capacity_ = std::move(rhs.capacity_);
        rhs.capacity_ = 0;
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

public:
    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    [[maybe_unused]] T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

public:
    Vector() = default;

    explicit Vector(size_t size)
            : data_(RawMemory<T>(size))
            , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
            : data_(RawMemory<T>(other.size_))
            , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (size_ > rhs.size_) {
                    DestroyN(data_.GetAddress(), size_ - rhs.size_);
                    size_ = rhs.size_;
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                }
                else if (size_ < rhs.size_){
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    auto it_to_undef_lhs = data_.GetAddress() + size_;
                    auto it_to_current_rhs = rhs.data_.GetAddress() + size_;
                    std::uninitialized_copy_n(it_to_current_rhs, rhs.size_ - size_, it_to_undef_lhs);
                    size_ = rhs.size_;
                }
                else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    size_ = rhs.size_;
                }
            }
        }
        return *this;
    }


    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~Vector() {
        DestroyN(data_.GetAddress(), size_);
    }

public:
    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    [[maybe_unused]] const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    [[maybe_unused]] const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

public:
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data{new_capacity};

        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        DestroyN(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size == size_) {
            return;
        }

        if (new_size < size_) {
            const size_t count_to_delete = size_ - new_size;
            for (size_t i = 0; i < count_to_delete; i++) {
                PopBack();
            }
        }
        else {
            size_t pos_to_end = size_;
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + pos_to_end, new_size - pos_to_end);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        if (data_.Capacity() > size_) {
            new (data_.GetAddress() + size_) T(value);
        }
        else {
            size_t new_size = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data{new_size};
            new (new_data.GetAddress() + size_) T(value);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            DestroyN(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (data_.Capacity() > size_) {
            new (data_.GetAddress() + size_) T(std::move(value));
        }
        else {
            size_t new_size = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data{new_size};
            new (new_data.GetAddress() + size_) T(std::move(value));

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            DestroyN(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (data_.Capacity() > size_) {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        else {
            size_t new_size = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data{new_size};
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            DestroyN(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;

        return *(data_.GetAddress() + (size_ - 1));
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t num_pos = pos - begin();

        if (data_.Capacity() > size_) {
            if (pos == end()) {
                new (end()) T(std::forward<Args>(args)...);
            }
            else {
                T value{std::forward<Args>(args)...};
                new (end()) T(std::forward<T>(*(end() - 1)));
                std::move_backward(begin() + num_pos, end() - 1, end());
                *(begin() + num_pos) = std::forward<T>(value);
            }
        }
        else {
            size_t new_size = size_ == 0 ? 1 : size_ * 2;
            RawMemory<T> new_data{new_size};
            new (new_data.GetAddress() + num_pos) T(std::forward<Args>(args)...);

            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(),
                                              num_pos,
                                              new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(),
                                              num_pos,
                                              new_data.GetAddress());
                }
            }
            catch (...) {
                Destroy(new_data.GetAddress() + num_pos);
                throw;
            }

            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(begin() + num_pos,
                                              size_ - num_pos,
                                              new_data.GetAddress() + (num_pos + 1));
                } else {
                    std::uninitialized_copy_n(begin() + num_pos,
                                              size_ - num_pos,
                                              new_data.GetAddress() + (num_pos + 1));
                }
            }
            catch (...) {
                DestroyN(new_data.GetAddress() + num_pos, size_ - num_pos + 1);
                throw;
            }

            DestroyN(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return begin() + num_pos;
    }

    iterator Erase(const_iterator pos) {
        size_t num_pos = pos - begin();
        if (size_ > 0) {
            std::move(begin() + num_pos + 1, end(), begin() + num_pos);
            --size_;
            Destroy(begin() + size_);
        }
        return (size_ + 1) == 1 ? end() : begin() + num_pos;
    }

    void PopBack() noexcept {
        if (size_ > 0) {
            Destroy(data_.GetAddress() + (size_ - 1));
            --size_;
        }
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    [[maybe_unused]] static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
