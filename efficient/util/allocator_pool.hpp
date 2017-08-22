// Copyright 2017 DeepFabric, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <vector>
#include <mutex>
#include <atomic>

namespace deepfabric
{
/*
	CLASS ALLOCATOR_POOL
	--------------------
*/
/*!
	@brief Simple block-allocator that internally allocates a large chunk then allocates smaller blocks from this larger block
	@details This is a simple block allocator that internally allocated a large single block from the C++ free-store (or operating system)
	and then allows calls to allocate small blocks from that one block.  These small blocks cannot be individually deallocated, but rather
	are all deallocated all at once when rewind() is called.  C++ allocators can easily be defined that allocate from a single object of this
	type, but that is left for other classes to manage (for example, class allocator_cpp).

	If the large memory block "runs out" then a second (and subsequent) block are allocated from the C++ free-store and they
	are chained together.  If the caller askes for a single piece of memory larger then that default_allocation_size then this
	class will allocate a chunk of the required size and return that to the caller.  Note that there is wastage at the end of
	each chunk as they cannot be guaranteed to lay squentially in memory.

	By default allocations by this class are not aligned to any particular boundary.  That is, if 1 byte is allocated then the next memory
	allocation is likely to be exactly one byte further on.  So allocation of a uint8_t followed by the allocation of a uint32_t is likely to
	result in the uint32_t being at an odd-numbered memory location.  Call malloc() with an alignment value to allocate an aligned piece of memory.
	On ARM, all memory allocations are word-aligned (sizeof(void *) by default because unaligned reads usually cause a fault.

	The use of new and delete in C++ (and malloc and free in C) is expensive as a substantial amount of work is necessary in order to maintain
	the heap.  This class reduces that cost - it exists for efficiency reasons alone.

	This allocator is thread safe.  A single allocator can be called from multiple threads concurrently and they will each
	return a valid pointer to a piece of memory that is not overlapping with any pointer returned from any other call and is of
	the requested size.
*/
class allocator_pool
{
protected:
    static const size_t default_allocation_size = 1024 * 1024 * 1024;	///< Allocations from the C++ free-store are this size
#ifdef __aarch64__
    static constexpr size_t alignment_boundary = sizeof(void *);		///< On ARM its necessary to align all memory allocations on word boundaries
#else
    static constexpr size_t alignment_boundary = 1;						///< Elsewhere don't bother with alignment (align on byte boundaries)
#endif

protected:
    std::atomic<size_t> used;			///< The number of bytes this object has passed back to the caller.
    std::atomic<size_t> allocated;		///< The number of bytes this object has allocated.
    size_t block_size;					///< The size (in bytes) of the large-allocations this object will make.

#ifdef USE_CRT_MALLOC
    std::vector<void *> crt_malloc_list;	///< When USE_CRT_MALLOC is defined the C RTL malloc() is called and this keeps track of those calls (so that rewind() works).
    std::mutex mutex;						///< Mutex used to control access to crt_malloc_list as it is not thread-safe.
#endif

protected:
    /*
    	CLASS ALLOCATOR_POOL::CHUNK
    	---------------------------
    */
    /*!
    	@brief Details of an individual large-allocation unit.
    	@details The large-allocations are kept in a linked list of chunks.  Each chunk stores a backwards pointer (of NULL if not backwards chunk) the
    	size of the allocation and details of it's use.  The large block that is allocated is actually the size of the caller's request plus the size of
    	this structure.  The large-block is layed out as this object at the start and data[] being of the user's requested length.  That is, if the user
    	asks for 1KB then the actual request from the C++ free store (or the Operating System) is 1BK + sizeof(allocator::chunk).
    */
    class chunk
    {
    public:
        std::atomic<uint8_t *> chunk_at;	///< Pointer to the next byte that can be allocated (within the current chunk).
        uint8_t *chunk_end;					///< Pointer to the end of the current chunk's large allocation (used to check for overflow).
        chunk *next_chunk;					///< Pointer to the previous large allocation (i.e. chunk).
        size_t chunk_size;					///< The size of this chunk.
#ifdef WIN32
#pragma warning(push)			// Xcode thinks thinks a 0-sized entity in a class is OK, but Visual Studio kicks up a fuss (but does it anyway).
#pragma warning(disable : 4200)
#endif
        uint8_t data[];					///< The data in this large allocation that is available for re-distribution.
#ifdef WIN32
#pragma warning(pop)
#endif
    };

protected:
    std::atomic<chunk *> current_chunk;			///< Pointer to the top of the chunk list (of large allocations).

private:
    /*
    	ALLOCATOR_POOL::ALLOCATOR_POOL()
    	--------------------------------
    */
    /*!
    	@brief Private copy constructor prevents object copying
    */
    allocator_pool(allocator_pool &)
    {
        assert(0);
    }

    /*
    	ALLOCATOR_POOL::OPERATOR=()
    	---------------------------
    */
    /*!
    	@brief Private assignment operator prevents assigning to this object
    */
    allocator_pool &operator=(const allocator_pool &)
    {
        assert(0);
        return *this;
    }

protected:
    /*
    	ALLOCATOR_POOL::ALLOC()
    	-----------------------
    */
    /*!
    	@brief Allocate more memory from the C++ free-store
    	@param size [in] The size (in bytes) of the requested block.
    	@return A pointer to a block of memory of size size, or NULL on failure.
    */
    void *alloc(size_t size) const
    {
        return ::malloc((size_t)size);
    }

    /*
    	ALLOCATOR_POOL::DEALLOC()
    	-------------------------
    */
    /*!
    	@brief Hand back to the C++ free store (or Operating system) a chunk of memory that has previously been allocated with allocator_pool::alloc().
    	@param buffer [in] A pointer previously returned by allocator_pool::alloc()
    */
    void dealloc(void *buffer) const
    {
        ::free(buffer);
    }

    /*
    	ALLOCATOR_POOL::ADD_CHUNK()
    	---------------------------
    */
    /*!
    	@brief Get memory from the C++ free store (or the Operating System) and add it to the linked list of large-allocations.
    	@details This is a maintenance method whose function is to allocate large chunks of memory and to maintain the liked list
    	of these large chunks.  As an allocator this object is allocating memory for the caller, so it may as well manage its own list.
    	The bytes parameter to this method is an indicator of the minimum amount of memory the caller requires, this object will allocate
    	at leat that amount of space plus any space necessary for housekeeping.
    	@param bytes [in] Allocate space so that it is possible to return an allocation is this parameter is size.
    	@return A pointer to a chunk containig at least this amount of free space.
    */
    chunk *add_chunk(size_t bytes);

public:
    /*
    	ALLOCATOR_POOL::ALLOCATOR_POOL()
    	--------------------------------
    */
    /*!
    	@brief Constructor
    	@param block_size_for_allocation [in] This size of the large-chunk allocation from the C++ free store or the Operating System.
    */
    allocator_pool(size_t block_size_for_allocation = default_allocation_size);

    /*
    	ALLOCATOR_POOL::~ALLOCATOR_POOL()
    	---------------------------------
    */
    /*!
    	@brief Destructor.
    */
    ~allocator_pool();

    /*
    	ALLOCATOR_POOL::OPERATOR==()
    	----------------------------
    */
    /*!
    	@brief Compare for equality two objects of this class type.
    	@details Any two unused allocator_pool objects are equal until one or the other is first used.
    	@param with [in] The object to compare to.
    	@return True if this == that, else false.
    */
    bool operator==(const allocator_pool &with)
    {
        try
        {
            /*
            	Downcast the parameter to an allocator_memory and see if it matches this.
            */
            auto other = dynamic_cast<const allocator_pool *>(&with);
            return (used == other->size()) && (allocated == other->capacity()) && (current_chunk.load() == other->current_chunk.load());
        }
        catch (...)
        {
            return false;
        }
    }

    /*
    	ALLOCATOR_POOL::OPERATOR!=()
    	----------------------------
    */
    /*!
    	@brief Compare for inequlity two objects of this class type.
    	@details Any two unused allocator_pool objects are equal until one or the other is first used.
    	@param with [in] The object to compare to.
    	@return True if this != that, else false.
    */
    bool operator!=(const allocator_pool &with)
    {
        try
        {
            /*
            	Downcast the parameter to an allocator_memory and see if it matches this.
            */
            auto other = dynamic_cast<const allocator_pool *>(&with);
            return (used != other->size()) || (allocated != other->capacity()) || (current_chunk.load() != other->current_chunk.load());
        }
        catch (...)
        {
            return false;
        }
    }

    size_t capacity(void) const
    {
        return allocated;
    }

    size_t size(void) const
    {
        return used;
    }

    /*
    	ALLOCATOR::REALIGN()
    	--------------------
    */
    /*!
    	@brief Compute the number of extra bytes of memory necessary for an allocation to start on an aligned boundary.
    	@details Aligning all allocations on a machine-word boundary is a space / space trade off.  Allocating a string of single
    	bytes one after the other and word-aligned would result in a machine word being used per byte.  To avoid this wastage this
    	class, by default, does not word-align any allocations.  However, it is sometimes necessary to word-align because some
    	assembly instructions require word-alignment.  This method return the number of bytes of padding necessary to make an
    	address word-aligned.
    	@param address [in] Compute the number of wasted bytes from this address to the next bounday
    	@param boundary [in] The byte-boundary to which this address should be alligned (e.g. 4 will ensure the least significant 2 bits are alwasys 00)
    	@return The number of bytes to add to address to make it aligned
    */
    static size_t realign(const void *address, size_t boundary)
    {
        /*
        	Get the pointer as an integer
        */
        uintptr_t current_pointer = (uintptr_t)address;

        /*
        	Compute the amount of padding that is needed to pad to a boundary of size alignment_boundary
        */
        size_t padding = (current_pointer % boundary == 0) ? 0 : boundary - current_pointer % boundary;

        /*
        	Return the number of bytes that must be addedd to address to make it aligned
        */
        return padding;
    }

    /*
    	ALLOCATOR_POOL::MALLOC()
    	------------------------
    */
    /*!
    	@brief Allocate a small chunk of memory from the internal block and return a pointer to the caller
    	@param bytes [in] The size of the chunk of memory.
    	@param alignment [in] If a word-aligned piece of memory is needed then this is the word-size (e.g. sizeof(void*))
    	@return A pointer to a block of memory of size bytes, or NULL on failure.
    */
    void *malloc(size_t bytes, size_t alignment = alignment_boundary);

    /*
    	ALLOCATOR_POOL::REWIND()
    	------------------------
    */
    /*!
    	@brief Throw away (without calling delete) all objects allocated in the memory space of this object.
    	@details This method rolls-back the memory that has been allocated by handing it all back to the C++ free store
    	(or operating system).  delete is not called for any objects allocated in this space, the memory is simply re-claimed.
    */
    void rewind(void);
};
}
