#pragma once

#include <memory>

// Arena Allocator design: https://medium.com/@sgn00/high-performance-memory-management-arena-allocators-c685c81ee338

namespace Util {
    class ArenaAllocator {
        public:
            // ArenaAllocator constructor
            // Uses char* since it is 1 byte
            explicit ArenaAllocator(std::size_t size)
                    : m_buffer(static_cast<char *>(::operator new(size))),
                    m_capacity(size), m_offset(0) {}

            ~ArenaAllocator() {
                callDestructors();
                ::operator delete(m_buffer);
            }

            template<typename T, typename... Args>
            T *allocate(Args &&... args) {
                auto [nodePointer, nodeOffset] = allocate(m_offset, sizeof(T), alignof(T));

                // If T has a trivial destructor
                if constexpr (std::is_trivially_destructible_v<T>) {
                    // Constructs the object in memory at 'nodePointer'
                    // std::forward<Args>(args)... passes the constructor arguments without changing lvalues and rvalues
                    T *obj = new(nodePointer) T(std::forward<Args>(args)...);
                    m_offset = nodeOffset;

                    return obj;
                }
                else {
                    // If T has a non-trivial destructor, create a destruction node in the arena allocator just after the node itself
                    auto [destructionPointer, destructionOffset] = allocate(nodeOffset, sizeof(DestructionNode),
                                                                        alignof(DestructionNode));

                    T *obj = new(nodePointer) T(std::forward<Args>(args)...);

                    // Lambda function to call T's destructor
                    auto dtorCall = [](void *pointer) {
                        static_cast<T *>(pointer)->~T();
                    };

                    // Constructs the object in memory at 'destructionPointer'
                    DestructionNode *destructionNode = new(destructionPointer) DestructionNode{dtorCall, lastDestructionNode, obj};

                    lastDestructionNode = destructionNode;
                    m_offset = destructionOffset;

                    return obj;
                }
            }

            void reset() {
                callDestructors();
                m_offset = 0;
            }

            // Non-copyable, Non-movable type
            ArenaAllocator(const ArenaAllocator &) = delete;

            ArenaAllocator(ArenaAllocator &&) = delete;

        private:
            struct DestructionNode {
                void (*dtor)(void *); // Lambda function to call the destructor of type T
                DestructionNode *prevDestructionNode; // Pointer to previous destruction node
                void *obj; // Pointer to the node
            };

            char *m_buffer;
            std::size_t m_capacity;
            std::size_t m_offset;
            DestructionNode *lastDestructionNode = nullptr;

            void callDestructors() {
                while (lastDestructionNode) {
                    lastDestructionNode->dtor(
                            lastDestructionNode->obj); // Calls obj's destructor using the lambda function dtor
                    lastDestructionNode = lastDestructionNode->prevDestructionNode;
                }
            };

            std::pair<void *, size_t> allocate(std::size_t curOffset, std::size_t size, std::size_t alignment) {
                char *curPointer = m_buffer + curOffset;
                std::size_t space = m_capacity - curOffset;
                void *alignedPointer = curPointer;

                // Aligns the pointer based on alignment of the node
                // Returns a nullptr if aligning means exceeding the memory allocated
                if (std::align(alignment, size, alignedPointer, space) == nullptr) {
                    throw std::bad_alloc();
                }

                // Subtracting static_cast<char*>(alignedPointer) with m_buffer returns the current offset before inserting the new node
                auto newOffset = static_cast<char *>(alignedPointer) - m_buffer + size;

                return {alignedPointer, newOffset};
            }
    };
}