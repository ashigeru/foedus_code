/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_MEMORY_PAGE_POOL_PIMPL_HPP_
#define FOEDUS_MEMORY_PAGE_POOL_PIMPL_HPP_
#include <foedus/fwd.hpp>
#include <foedus/initializable.hpp>
#include <foedus/memory/aligned_memory.hpp>
#include <foedus/memory/page_pool.hpp>
#include <cassert>
#include <mutex>
#include <vector>
namespace foedus {
namespace memory {
/**
 * @brief Pimpl object of PagePool.
 * @ingroup MEMORY
 * @details
 * A private pimpl object for PagePool.
 * Do not include this header from a client program unless you know what you are doing.
 */
class PagePoolPimpl final : public DefaultInitializable {
 public:
    PagePoolPimpl() = delete;
    explicit PagePoolPimpl(Engine* engine);
    ErrorStack  initialize_once() override;
    ErrorStack  uninitialize_once() override;

    ErrorCode   grab(uint32_t desired_grab_count, PagePoolOffsetChunk *chunk);
    void        release(uint32_t desired_release_count, PagePoolOffsetChunk *chunk);
    PagePoolOffset resolve_page(storage::Page *page) const;
    storage::Page* resolve_offset(PagePoolOffset offset) const;

    Engine* const                   engine_;

    /** The whole memory region of the pool. */
    AlignedMemory                   memory_;

    /** Just an auxiliary variable to the beginning of the pool. Same as memory_.get_block(). */
    storage::Page*                  pool_base_;

    /** Just an auxiliary variable of the size of pool. Same as memory_.get_size()/PAGE_SIZE. */
    uint64_t                        pool_size_;

    /**
     * This many first pages are used for free page maintainance.
     * This also means that "Page-0" never appears in our engine, thus we can use offset=0 as null.
     * In other words, all offsets grabbed/released should be equal or larger than this value.
     * Immutable once initialized.
     */
    uint64_t                        pages_for_free_pool_;

    /**
     * We maintain free pages as a simple circular queue.
     * We append new/released pages to tail while we eat from head.
     */
    PagePoolOffset*                 free_pool_;
    /** Size of free_pool_. Immutable once initialized. */
    uint64_t                        free_pool_capacity_;
    /** Inclusive head of the circular queue. Be aware of wrapping around. */
    uint64_t                        free_pool_head_;
    /** Number of free pages. */
    uint64_t                        free_pool_count_;

    /**
     * grab()/release() are protected with this lock.
     * This lock is not contentious at all because we pack many pointers in a chunk.
     */
    std::mutex                      lock_;
};
}  // namespace memory
}  // namespace foedus
#endif  // FOEDUS_MEMORY_PAGE_POOL_PIMPL_HPP_