#pragma once

#include "types.hpp"
#include <cstddef>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <new>

namespace backtest {

// ============================================================================
// Memory Pool for High-Performance Object Allocation
// ============================================================================
// This pool minimizes allocations by pre-allocating memory blocks
// and reusing them. Essential for low-latency order management.

template<typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    MemoryPool() : current_block_(nullptr), current_slot_(nullptr), 
                   last_slot_(nullptr), free_slots_(nullptr) {
        allocate_block();
    }
    
    ~MemoryPool() {
        for (auto* block : blocks_) {
            operator delete(block);
        }
    }
    
    // Non-copyable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    // Allocate memory for one object
    T* allocate() {
        if (free_slots_ != nullptr) {
            // Reuse freed slot
            T* result = reinterpret_cast<T*>(free_slots_);
            free_slots_ = free_slots_->next;
            return result;
        }
        
        if (current_slot_ >= last_slot_) {
            allocate_block();
        }
        
        return current_slot_++;
    }
    
    // Deallocate memory (add to free list)
    void deallocate(T* ptr) {
        if (ptr == nullptr) return;
        
        reinterpret_cast<Slot*>(ptr)->next = free_slots_;
        free_slots_ = reinterpret_cast<Slot*>(ptr);
    }
    
    // Construct object in-place
    template<typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }
    
    // Destroy and deallocate
    void destroy(T* ptr) {
        if (ptr == nullptr) return;
        ptr->~T();
        deallocate(ptr);
    }
    
    // Statistics
    size_t block_count() const { return blocks_.size(); }
    size_t total_capacity() const { return blocks_.size() * BlockSize; }
    
private:
    union Slot {
        T element;
        Slot* next;
        
        Slot() {}
        ~Slot() {}
    };
    
    static_assert(sizeof(Slot) >= sizeof(T), "Slot must be at least as large as T");
    
    void allocate_block() {
        // Allocate a new block of memory
        char* new_block = reinterpret_cast<char*>(
            operator new(BlockSize * sizeof(Slot)));
        
        blocks_.push_back(new_block);
        current_block_ = new_block;
        current_slot_ = reinterpret_cast<T*>(new_block);
        last_slot_ = reinterpret_cast<T*>(new_block + BlockSize * sizeof(Slot));
    }
    
    char* current_block_;
    T* current_slot_;
    T* last_slot_;
    Slot* free_slots_;
    std::vector<char*> blocks_;
};

// ============================================================================
// Thread-Safe Memory Pool (with lock)
// ============================================================================
template<typename T, size_t BlockSize = 4096>
class ThreadSafeMemoryPool {
public:
    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.allocate();
    }
    
    void deallocate(T* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.deallocate(ptr);
    }
    
    template<typename... Args>
    T* construct(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.construct(std::forward<Args>(args)...);
    }
    
    void destroy(T* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.destroy(ptr);
    }

private:
    MemoryPool<T, BlockSize> pool_;
    std::mutex mutex_;
};

// ============================================================================
// Lock-Free Memory Pool (Single Producer Single Consumer)
// ============================================================================
template<typename T, size_t Capacity = 65536>
class LockFreePool {
public:
    LockFreePool() : head_(0), tail_(0) {
        // Pre-allocate all slots
        for (size_t i = 0; i < Capacity; ++i) {
            slots_[i] = new T();
        }
    }
    
    ~LockFreePool() {
        for (size_t i = 0; i < Capacity; ++i) {
            delete slots_[i];
        }
    }
    
    // Get an object from the pool
    T* acquire() {
        size_t current_head = head_.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) % Capacity;
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return nullptr;  // Pool exhausted
        }
        
        T* obj = slots_[current_head];
        head_.store(next_head, std::memory_order_release);
        return obj;
    }
    
    // Return an object to the pool
    void release(T* obj) {
        if (obj == nullptr) return;
        
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        slots_[current_tail] = obj;
        tail_.store((current_tail + 1) % Capacity, std::memory_order_release);
    }

private:
    std::array<T*, Capacity> slots_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
};

// ============================================================================
// Object Pool with Unique Pointer Support
// ============================================================================
template<typename T>
class ObjectPool {
public:
    struct Deleter {
        ObjectPool<T>* pool;
        
        void operator()(T* ptr) {
            if (pool && ptr) {
                pool->release(ptr);
            }
        }
    };
    
    using UniquePtr = std::unique_ptr<T, Deleter>;
    
    ObjectPool(size_t initial_size = 1024) {
        objects_.reserve(initial_size);
        free_list_.reserve(initial_size);
        
        for (size_t i = 0; i < initial_size; ++i) {
            objects_.push_back(std::make_unique<T>());
            free_list_.push_back(objects_.back().get());
        }
    }
    
    UniquePtr acquire() {
        T* ptr = nullptr;
        
        if (!free_list_.empty()) {
            ptr = free_list_.back();
            free_list_.pop_back();
        } else {
            objects_.push_back(std::make_unique<T>());
            ptr = objects_.back().get();
        }
        
        return UniquePtr(ptr, Deleter{this});
    }
    
    void release(T* ptr) {
        free_list_.push_back(ptr);
    }
    
    size_t size() const { return objects_.size(); }
    size_t available() const { return free_list_.size(); }

private:
    std::vector<std::unique_ptr<T>> objects_;
    std::vector<T*> free_list_;
};

}  // namespace backtest
