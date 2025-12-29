#ifndef CUSTOM_CONTAINERS_H
#define CUSTOM_CONTAINERS_H

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace Custom {

template <typename T>
class Vector {
private:
    T* data;
    size_t sz;
    size_t cap;

public:
    Vector() : data(nullptr), sz(0), cap(0) {}
    
    explicit Vector(size_t n) : sz(n), cap(n) {
        data = (n > 0) ? new T[n] : nullptr;
    }

    // Move constructor
    Vector(Vector&& other) noexcept : data(other.data), sz(other.sz), cap(other.cap) {
        other.data = nullptr;
        other.sz = 0;
        other.cap = 0;
    }

    // Move assignment
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            sz = other.sz;
            cap = other.cap;
            other.data = nullptr;
            other.sz = 0;
            other.cap = 0;
        }
        return *this;
    }

    // Copy constructor
    Vector(const Vector& other) : sz(other.sz), cap(other.sz) {
        data = (sz > 0) ? new T[sz] : nullptr;
        for (size_t i = 0; i < sz; ++i) {
            data[i] = other.data[i];
        }
    }

    // Copy assignment
    Vector& operator=(const Vector& other) {
        if (this != &other) {
            T* new_data = (other.sz > 0) ? new T[other.sz] : nullptr;
            for (size_t i = 0; i < other.sz; ++i) {
                new_data[i] = other.data[i];
            }
            delete[] data;
            data = new_data;
            sz = other.sz;
            cap = other.sz;
        }
        return *this;
    }

    ~Vector() {
        delete[] data;
    }

    void reserve(size_t new_cap) {
        if (new_cap <= cap) return;
        T* new_data = new T[new_cap];
        for (size_t i = 0; i < sz; ++i) {
            new_data[i] = std::move(data[i]);
        }
        delete[] data;
        data = new_data;
        cap = new_cap;
    }

    void push_back(const T& value) {
        if (sz == cap) {
            reserve(cap == 0 ? 1 : cap * 2);
        }
        data[sz++] = value;
    }

    void push_back(T&& value) {
        if (sz == cap) {
            reserve(cap == 0 ? 1 : cap * 2);
        }
        data[sz++] = std::move(value);
    }

    void pop_back() {
        if (sz > 0) --sz;
    }

    void clear() {
        sz = 0;
    }

    size_t size() const { return sz; }
    size_t capacity() const { return cap; }
    bool empty() const { return sz == 0; }

    T& operator[](size_t index) {
        return data[index];
    }

    const T& operator[](size_t index) const {
        return data[index];
    }

    T& back() {
        if (sz == 0) throw std::runtime_error("Vector empty");
        return data[sz - 1];
    }

    T* data_ptr() { return data; }
    const T* data_ptr() const { return data; }

    T* begin() { return data; }
    T* end() { return data + sz; }
    const T* begin() const { return data; }
    const T* end() const { return data + sz; }
};

template <typename T>
class Stack {
private:
    Vector<T> container;

public:
    void push(const T& value) { container.push_back(value); }
    void push(T&& value) { container.push_back(std::move(value)); }
    void pop() { container.pop_back(); }
    T& top() { return container.back(); }
    const T& top() const { return container.back(); }
    bool empty() const { return container.empty(); }
    size_t size() const { return container.size(); }
};

} // namespace Custom

#endif
