/**
 * @file ArenaAllocator.h
 * @brief Arena-allocation based allocator class
 * @author Quinn Mikelson
 */

#ifndef OS_ARENAALLOCATOR_H
#define OS_ARENAALLOCATOR_H

#include <stdio.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <utility>

#ifndef assert
#define assert(x)                                                                          \
    if (!(x))                                                                              \
    {                                                                                      \
        printf("file: %s line: %d. Assertion Failure!\n    %s\n", __FILE__, __LINE__, #x); \
        fflush(stdout);                                                                    \
    }

#endif

template <typename T>
struct CommonAdaptor
{
    static T* malloc(const std::size_t max_elements)
    {
        return static_cast<T*>(aligned_alloc(alignof(T), sizeof(T) * max_elements));
    }

    static void free(T* ptr)
    {
        if (ptr)
            ::free(ptr);
    }
};

/** @brief Fixed-Size Bitmap Allocator optimized for embedded systems (MSP430)
 *  Uses a bitmap to track allocated blocks with minimal overhead.
 *  Memory overhead: max_elements / 8 bytes for bitmap
 */
template <class T, std::size_t max_elements, typename Adaptor = CommonAdaptor<T>>
struct ArenaAllocator
{
private:
    /// @brief Bitmap word type for efficient bit operations
    using bitmap_word_t = uint16_t;  // 16-bit for MSP430 efficiency
    static constexpr size_t BITS_PER_WORD = sizeof(bitmap_word_t) * 8;
    static constexpr size_t BITMAP_SIZE = (max_elements + BITS_PER_WORD - 1) / BITS_PER_WORD;

public:
    /// @brief Allocator type-aliases
    using value_type         = T;
    using pointer            = value_type*;
    using allocator_type     = std::allocator<value_type>;
    using reference          = value_type&;
    using const_reference    = value_type const&;
    using const_pointer      = typename std::pointer_traits<pointer>::template rebind<value_type const>;
    using void_pointer       = typename std::pointer_traits<pointer>::template rebind<void>;
    using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;
    using difference_type    = typename std::pointer_traits<pointer>::difference_type;
    using size_type          = std::make_unsigned_t<difference_type>;

    /// @brief allocator_traits hint aliases
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap            = std::true_type;
    using is_always_equal                        = std::false_type;

    /// @brief Rebind allocator to different type (required by std::allocator_traits)
    template <typename U>
    struct rebind
    {
        using other = ArenaAllocator<U, max_elements, Adaptor>;
    };

    /// @brief uninitialized memory begin
    std::shared_ptr<value_type> arena_begin{nullptr, Adaptor::free};
    /// @brief uninitialized memory end
    pointer arena_end{nullptr};
    /// @brief bitmap tracking allocated blocks (1 = allocated, 0 = free)
    bitmap_word_t bitmap[BITMAP_SIZE];

    /** @brief Sized constructor
     *  @param max_elements The maximum number of elements of [value_type] that can be created
     */
    explicit ArenaAllocator()
        : arena_begin{max_elements ? Adaptor::malloc(max_elements) : nullptr, Adaptor::free},
          arena_end{max_elements ? arena_begin.get() + max_elements : nullptr},
          bitmap{}  // Zero-initialize bitmap (all blocks free)
    {
    }

    ///@brief Disallow copy construction
    template <typename U, std::size_t N>
    ArenaAllocator(const ArenaAllocator<U, N, Adaptor>& other) = delete;

    /** @brief Explicit move constructor
     *  @param other The instance to move from
     */
    template <typename U, std::size_t N>
    ArenaAllocator(ArenaAllocator<U, N, Adaptor>&& other) noexcept
        : arena_begin{std::exchange(other.arena_begin, nullptr)}, 
          arena_end{std::exchange(other.arena_end, nullptr)},
          bitmap{}
    {
        // Copy bitmap
        for (size_t i = 0; i < BITMAP_SIZE; ++i)
        {
            bitmap[i] = other.bitmap[i];
            other.bitmap[i] = 0;
        }
    }

    ///@brief Disallow copy assignment
    template <typename U, std::size_t N>
    ArenaAllocator& operator=(const ArenaAllocator<U, N, Adaptor>&) = delete;

    /** @brief Move assignment operator
     *  @param other Allocator instance to move from
     *  @returns A reference to this
     */
    template <typename U, std::size_t N>
    ArenaAllocator& operator=(ArenaAllocator<U, N, Adaptor>&& other) noexcept
    {
        arena_begin = std::exchange(other.arena_begin, nullptr);
        arena_end   = std::exchange(other.arena_end, nullptr);
        
        // Copy bitmap
        for (size_t i = 0; i < BITMAP_SIZE; ++i)
        {
            bitmap[i] = other.bitmap[i];
            other.bitmap[i] = 0;
        }
        
        return *this;
    }

    /// @brief Allocator destructor, frees allocated memory
    ~ArenaAllocator()
    {
        arena_begin.reset();
        arena_end = nullptr;
    }

    /** Perfect forwarding item constructor, creates new element at ptr
     *  @tparam U The type to construct
     *  @param U Pointer to preallocated memory
     *  @param args Element constructor parameters
     */
    template <class U, class... Args>
    inline void construct(U* ptr, Args&&... args) const noexcept
    {
        ::new (static_cast<void_pointer>(ptr)) U(std::forward<Args>(args)...);
    }

    // @brief Object destructor method
    template <class U>
    inline void destroy(U* ptr)
    {
        ptr->~U();
    }

    ///@brief Required allocator_traits function, called when allocator is copied
    inline ArenaAllocator select_on_container_copy_construction() const noexcept
    {
        // Return a fresh allocator with the same parameters
        return ArenaAllocator{};
    }

private:
    /** @brief Check if a specific block is allocated
     *  @param block_index The block index to check
     *  @returns true if allocated, false if free
     */
    inline bool is_block_allocated(size_t block_index) const noexcept
    {
        const size_t word_index = block_index / BITS_PER_WORD;
        const size_t bit_index = block_index % BITS_PER_WORD;
        return (bitmap[word_index] & (bitmap_word_t(1) << bit_index)) != 0;
    }

    /** @brief Mark a block as allocated
     *  @param block_index The block index to mark
     */
    inline void mark_block_allocated(size_t block_index) noexcept
    {
        const size_t word_index = block_index / BITS_PER_WORD;
        const size_t bit_index = block_index % BITS_PER_WORD;
        bitmap[word_index] |= (bitmap_word_t(1) << bit_index);
    }

    /** @brief Mark a block as free
     *  @param block_index The block index to mark
     */
    inline void mark_block_free(size_t block_index) noexcept
    {
        const size_t word_index = block_index / BITS_PER_WORD;
        const size_t bit_index = block_index % BITS_PER_WORD;
        bitmap[word_index] &= ~(bitmap_word_t(1) << bit_index);
    }

    /** @brief Find n consecutive free blocks using first-fit strategy
     *  @param n Number of consecutive blocks needed
     *  @returns Starting block index, or max_elements if not found
     */
    inline size_t find_free_blocks(size_t n) const noexcept
    {
        if (n == 0 || n > max_elements)
            return max_elements;

        size_t consecutive = 0;
        size_t start_block = 0;

        for (size_t i = 0; i < max_elements; ++i)
        {
            if (!is_block_allocated(i))
            {
                if (consecutive == 0)
                    start_block = i;
                ++consecutive;
                
                if (consecutive >= n)
                    return start_block;
            }
            else
            {
                consecutive = 0;
            }
        }

        return max_elements;  // Not found
    }

public:

    /** @brief Allocates uninitialized storage
     *  @param n Number of elements to allocate
     *  @returns Pointer to memory region
     */
    inline pointer allocate(size_type n, const_void_pointer = const_void_pointer())
    {
        assert(n > 0);
        assert(n <= max_size());
        
        // Find n consecutive free blocks
        const size_t start_block = find_free_blocks(n);
        
        // Check if allocation failed
        assert(start_block < max_elements);
        
        // Mark blocks as allocated
        for (size_t i = 0; i < n; ++i)
        {
            mark_block_allocated(start_block + i);
        }
        
        // Return pointer to the start of the allocated region
        return arena_begin.get() + start_block;
    }

    /** @brief Deallocates storage
     * @param ptr Pointer to free
     * @param n Number of elements associated with ptr
     */
    inline void deallocate(pointer ptr, const size_type n) noexcept
    {
        assert(this->contains(ptr));
        assert(n > 0);
        
        // Calculate block index from pointer
        const size_t block_index = ptr - arena_begin.get();
        
        // Mark blocks as free
        for (size_t i = 0; i < n; ++i)
        {
            mark_block_free(block_index + i);
        }
    }

public:

    /** @brief Maximum number of elements, used by std containers
     *  @returns Maximum number of elements
     */
    inline size_type max_size() const noexcept
    {
        return std::distance(arena_begin.get(), arena_end);
    }

    /** @brief Determines whether given pointer is within our buffer
     *  @param ptr A pointer to an address
     *  @returns true if ptr is in range
     */
    inline bool contains(const pointer ptr) const
    {
        return ptr >= arena_begin.get() && ptr < arena_end;
    }
};

/** @brief Equality operator for allocators with same Adaptor
 *  @returns true if allocators share the same arena
 */
template <typename T1, std::size_t N1, typename T2, std::size_t N2, typename Adaptor>
inline bool operator==(ArenaAllocator<T1, N1, Adaptor> const& lhs, ArenaAllocator<T2, N2, Adaptor> const& rhs)
{
    return lhs.arena_begin.get() == rhs.arena_begin.get();
}

/** @brief Inequality operator for allocators
 *  @returns true if allocators don't share the same arena
 */
template <typename T1, std::size_t N1, typename T2, std::size_t N2, typename Adaptor>
inline bool operator!=(ArenaAllocator<T1, N1, Adaptor> const& lhs, ArenaAllocator<T2, N2, Adaptor> const& rhs)
{
    return !(lhs == rhs);
}

#endif /* OS_ARENAALLOCATOR_H */
