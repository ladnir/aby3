#pragma once
#include "cryptoTools/Common/config.h"
#include <array>
#include <vector>
#include <memory>
#include <new>
#include <utility>

namespace osuCrypto
{
    constexpr const size_t gDefaultAlignment = 32;

    // arrays of static and dynamic sizes aligned to the maximal useful alignment, meaning 32
    // bytes for avx or 16 bytes for SSE. Also includes an allocator for aligning std::vector.
    template<typename T, size_t N, size_t Alignment = gDefaultAlignment>
    struct alignas(Alignment) AlignedArray : public std::array<T, N>
    {
    private:
        using Base = std::array<T, N>;

        // Use std::array's constructors, etc.
    public:
        AlignedArray() = default;
        using Base::Base;
        using Base::operator=;
    };

    namespace detail
    {
        template<class T>
        struct max_objects
            : std::integral_constant<std::size_t,
            ~static_cast<std::size_t>(0) / sizeof(T)> { };

        template<class T, std::size_t Alignment>
        class aligned_allocator {

        public:
            typedef T value_type;
            typedef T* pointer;
            typedef const T* const_pointer;
            typedef void* void_pointer;
            typedef const void* const_void_pointer;
            typedef value_type& reference;
            typedef const value_type& const_reference;
            typedef std::size_t size_type;
            typedef std::ptrdiff_t difference_type;
            typedef std::true_type propagate_on_container_move_assignment;
            typedef std::true_type is_always_equal;

            template<class U>
            struct rebind {
                typedef aligned_allocator<U, Alignment> other;
            };

            aligned_allocator() = default;

            template<class U>
            aligned_allocator(const aligned_allocator<U, Alignment>&)
                noexcept { }


            static void* aligned_malloc(size_t required_bytes, size_t alignment)
            {
                void* p1; // original block
                void** p2; // aligned block
                int offset = alignment - 1 + sizeof(void*);
                if ((p1 = (void*)malloc(required_bytes + offset)) == NULL)
                {
                    return NULL;
                }
                p2 = (void**)(((size_t)(p1)+offset) & ~(alignment - 1));
                p2[-1] = p1;
                return p2;
            }

            static void aligned_free(void* p)
            {
                auto p1 = ((void**)p)[-1];
                free(p1);
            }


            static pointer allocate(size_type size, const_void_pointer = 0) {
                enum {
                    m = Alignment > alignof(T) ? Alignment : alignof(T)
                };
                if (size == 0) {
                    return 0;
                }

//#ifdef _MSC_VER
//                void* p = ::_aligned_malloc(sizeof(T) * size, m);
//#else
                void* p = aligned_malloc(sizeof(T) * size, m);
//#endif

                if (!p) {
                    throw std::bad_alloc();
                }
                return static_cast<T*>(p);
            }

            static void deallocate(pointer ptr, size_type) {
                aligned_free(ptr);
            }

            constexpr size_type max_size() const noexcept {
                return max_objects<T>::value;
            }

            template<class U, class... Args>
            void construct(U* ptr, Args&&... args) {
                ::new((void*)ptr) U(std::forward<Args>(args)...);
            }

            template<class U>
            void construct(U* ptr) {
                ::new((void*)ptr) U();
            }

            template<class U>
            void destroy(U* ptr) {
                (void)ptr;
                ptr->~U();
            }
        };

        template<class T, class U, std::size_t Alignment>
        inline bool
            operator==(const aligned_allocator<T, Alignment>&,
                const aligned_allocator<U, Alignment>&) noexcept
        {
            return true;
        }

        template<class T, class U, std::size_t Alignment>
        inline bool
            operator!=(const aligned_allocator<T, Alignment>&,
                const aligned_allocator<U, Alignment>&) noexcept
        {
            return false;
        }
    }

    template<typename T, size_t Alignment = gDefaultAlignment>
    using AlignedAllocator = detail::aligned_allocator<T, Alignment>;

    template<typename T, size_t Alignment = gDefaultAlignment>
    struct AlignedVector : public std::vector<T, AlignedAllocator<T, Alignment>>
    {
        using std::vector<T, AlignedAllocator<T, Alignment>>::vector;

        span<T> subspan(u64 offset, u64 size_ = ~0ull)
        {
            if (size_ == ~0ull)
                size_ = this->size() - offset;
            if (offset == this->size())
                return {};
            if (offset + size_ <= this->size())
                return { &*(this->begin() + offset), size_ };
            throw RTE_LOC;
        }

        span<const T> subspan(u64 offset, u64 size_ = ~0ull) const
        {
            if (size_ == ~0ull)
                size_ = this->size() - offset;
            if (offset == this->size())
                return {};
            if (offset + size_ <= this->size())
                return { &*(this->begin() + offset), size_ };
            throw RTE_LOC;
        }
    };



    // an aligned uninitialized vector like class.
    template<typename T, size_t Alignment = gDefaultAlignment>
    class AlignedUnVector
    {
        static_assert(std::is_trivially_constructible<T>::value, "");
        static_assert(std::is_trivially_destructible<T>::value, "");

        using Allocator = AlignedAllocator<T, Alignment>;

        size_t mCapacity = 0;
        span<T> mSpan;

    public:

        typedef T element_type;
        typedef typename std::remove_cv< T >::type value_type;

        typedef T& reference;
        typedef T* pointer;
        typedef T const* const_pointer;
        typedef T const& const_reference;

        typedef size_t    size_type;

        typedef pointer        iterator;
        typedef const_pointer  const_iterator;

        typedef std::ptrdiff_t difference_type;

        typedef std::reverse_iterator< iterator >       reverse_iterator;
        typedef std::reverse_iterator< const_iterator > const_reverse_iterator;

        ~AlignedUnVector()
        {
            clear();
        }

        AlignedUnVector() = default;
        AlignedUnVector(AlignedUnVector && o)
            : mCapacity(std::exchange(o.mCapacity, 0))
            , mSpan(std::exchange(o.mSpan, span<T>{}))
        {}

        AlignedUnVector(const AlignedUnVector& o)
        {
            *this = o;
        }

        AlignedUnVector(size_t n)
        {
            resize(n);
        }

        AlignedUnVector& operator=(const AlignedUnVector& o)
        {
            resize(o.size());
            std::copy(o.begin(), o.end(), mSpan.begin());
            return *this;
        }
        AlignedUnVector& operator=(AlignedUnVector&& o)
        {
            clear();
            mCapacity = std::exchange(o.mCapacity, 0);
            mSpan = std::exchange(o.mSpan, span<T>{});
            return *this;
        }

        auto capacity() const { return mCapacity; }
        auto size() const { return mSpan.size(); }
        auto empty() const { return this->size() == 0; }

        auto begin() { return mSpan.begin(); }
        auto begin() const { return mSpan.begin(); }
        auto end() { return mSpan.end(); }
        auto end() const { return mSpan.end(); }

        auto& front() { return mSpan.front(); }
        auto& front() const { return mSpan.front(); }
        auto& back() { return mSpan.back(); }
        auto& back() const { return mSpan.back(); }

        auto data() { return mSpan.data(); }
        auto data() const { return mSpan.data(); }

        void resize(size_t n, AllocType type = AllocType::Uninitialized)
        {
            auto oldCap = mCapacity;
            auto oldSpan = mSpan;
            if (capacity() >= n)
            {
                mSpan = span<T>(data(), n);
            }
            else
            {
                mSpan = span<T>(Allocator::allocate(n), n);
                mCapacity = n;

                if (oldCap)
                {
                    auto m = std::min<size_t>(oldSpan.size(), n);
                    std::copy(oldSpan.begin(), oldSpan.begin() + m, mSpan.begin());

                    Allocator::deallocate(oldSpan.data(), mCapacity);
                }
            }

            if (type == AllocType::Zeroed && oldSpan.size() < n)
            {
                memset(data() + oldSpan.size(), 0, (n - oldSpan.size()) * sizeof(T));
            }
        }

        void reserve(size_t n)
        {
            u64 s = this->size();

            if (n > capacity())
            {
                resize(n);
                resize(s);
            }
        }

        void clear()
        {
            if (mCapacity)
            {
                Allocator::deallocate(mSpan.data(), mCapacity);
                mSpan = {};
                mCapacity = 0;
            }
        }

        auto& operator[](size_t i) { return mSpan[i]; }
        auto& operator[](size_t i) const { return mSpan[i]; }

        span<T> subspan(u64 offset) { return mSpan.subspan(offset); }
        span<const T> subspan(u64 offset) const { return mSpan.subspan(offset); }
        span<T> subspan(u64 offset, size_t size) { return mSpan.subspan(offset, size); }
        span<const T> subspan(u64 offset, size_t size) const { return mSpan.subspan(offset, size); }

    };


    //using AlignedPtr = AlignedPtrT<block>;
    //using AlignedAllocator = AlignedAllocatorT<block>;
    //using AlignedAllocator2 = AlignedAllocatorT<std::array<block, 2>>;

}


