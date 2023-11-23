// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    template<typename T>
    class ring_buffer
    {
    public:
        static_assert(std::is_trivial_v<T>, "Trivial types are easier to optimize for");

        ring_buffer() = default;

        ring_buffer(const ring_buffer&) = delete;
        ring_buffer& operator=(const ring_buffer&) = delete;
        ring_buffer(ring_buffer&&) = delete;
        ring_buffer& operator=(ring_buffer&&) = delete;

        bool empty() const noexcept
        {
            return _size == 0;
        }

        size_t size() const noexcept
        {
            return _size;
        }

        void clear() noexcept
        {
            _writer = _beg;
            _reader = _beg;
            _size = 0;
        }

        __declspec(noinline) void write(const T& data)
        {
            const auto new_size = _size + 1;
            if (new_size > _capacity)
            {
                _grow(new_size);
            }

            const auto beg = _beg;
            const auto end = _end;
            auto writer = _writer;

            writer = _copy(writer, &data, 1);
            if (writer == end)
            {
                writer = beg;
            }

            _writer = writer;
            _size = new_size;
        }

        __declspec(noinline) void write(const T* data, size_t count)
        {
            const auto new_size = _size + count;
            if (new_size > _capacity)
            {
                _grow(new_size);
            }

            const auto beg = _beg;
            const auto end = _end;
            auto writer = _writer;

            const size_t available = end - writer;
            if (available > count)
            {
                writer = _copy(writer, data, count);
            }
            else
            {
                _copy(writer, data, available);
                writer = _copy(beg, data + available, count - available);
            }

            _writer = writer;
            _size = new_size;
        }

        __declspec(noinline)
            T* last_written() const noexcept
        {
            if (!_size)
            {
                return nullptr;
            }

            const auto writer = _writer;
            const auto p = writer == _beg ? _end : writer;
            return p - 1;
        }

        T* peek() noexcept
        {
            return _size ? _reader : nullptr;
        }

        bool read(T& data) noexcept
        {
            const auto beg = _beg;
            const auto end = _end;
            const auto reader = _reader;
            const auto size = _size;

            if (!size)
            {
                return false;
            }

            _copy(&data, reader, 1);

            auto new_reader = reader + 1;
            if (new_reader == end)
            {
                new_reader = beg;
            }

            _reader = new_reader;
            _size = size - 1;
            return true;
        }

        size_t read(T* data, size_t count) noexcept
        {
            const auto beg = _beg;
            const auto end = _end;
            auto reader = _reader;
            const auto size = _size;

            count = std::min(size, count);

            const size_t available = end - reader;
            if (available > count)
            {
                _copy(data, reader, count);
                reader = reader + count;
            }
            else
            {
                const auto remaining = count - available;
                _copy(data, reader, available);
                _copy(data + available, beg, remaining);
                reader = beg + remaining;
            }

            _reader = reader;
            _size -= count;
            return count;
        }

        void advance(size_t count) noexcept
        {
            const auto beg = _beg;
            const auto end = _end;
            auto reader = _reader;
            const auto size = _size;

            count = std::min(size, count);

            const size_t available = end - reader;
            if (available > count)
            {
                reader = reader + count;
            }
            else
            {
                reader = beg + (count - available);
            }

            _reader = reader;
            _size -= count;
        }

    private:
        static T* _allocate(size_t size)
        {
            if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                return static_cast<T*>(::operator new(size * sizeof(T)));
            }
            else
            {
                return static_cast<T*>(::operator new(size * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
            }
        }

        static void _deallocate(T* data) noexcept
        {
            if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(data);
            }
            else
            {
                ::operator delete(data, static_cast<std::align_val_t>(alignof(T)));
            }
        }

        static T* _copy(T* dst, const T* src, size_t len) noexcept
        {
            return static_cast<T*>(memcpy(dst, src, len * sizeof(T))) + len;
        }

        __declspec(noinline) void _grow(size_t new_size)
        {
            const auto new_cap = std::max(size_t{ 16 }, std::max(new_size, _capacity + _capacity / 2));
            const auto data = _allocate(new_cap);

            auto new_writer = data;
            new_writer = _copy(new_writer, _reader, _end - _reader);
            new_writer = _copy(new_writer, _beg, _writer - _beg);

            _deallocate(_beg);

            _beg = data;
            _end = data + new_cap;
            _reader = data;
            _writer = new_writer;
            _capacity = new_cap;
        }

        T* _beg = nullptr;
        T* _end = nullptr;
        T* _writer = nullptr;
        T* _reader = nullptr;
        size_t _size = 0;
        size_t _capacity = 0;
    };
}
