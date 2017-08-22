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

#include <stdint.h>
#include <array>
#include <atomic>
#include <thread>

#include "allocator_pool.hpp"

namespace deepfabric
{
/*!
	@brief Thread-safe grow-only dynamic array using the thread-safe allocator compressed using variable byte encoding.
	@details The array data is stored in a linked list of chunks where each chunk is larger then the previous as the array is growing.  
*/
class compressed_dynamic_array
{
protected:

#pragma pack(push,1)
    class node
    {
    public:
        size_t allocated;				///< The size of this node's data object (in elements).
        std::atomic<size_t> used;		///< The number of elements in data that are used (always <= allocated).
        node *next;                     ///< Pointer to the next node in the chain.
        uint8_t *data;                  ///< The array data for this node.

    public:
        /*!
        	@brief Constructor
        	@param pool [in] The pool allocator used to allocate the data controled by this node.
        	@param size [in] The size (in elements) of the data to be controlled by this node.
        */
        node(allocator_pool &pool, size_t size):
            allocated(size),            // the data array is this size (in elements).
            used(0),                    // the data array is empty.
            next(nullptr)				// this is the end of the linked list.
        {
            data = new (pool.malloc(size * sizeof(uint8_t))) uint8_t[size];
        }
    };
#pragma pack(pop)

public:

    class iterator
    {
    private:
        const node *current_node;		///< The node that this iterator is currently looking at.
        uint32_t element;               ///< Currently decoded element
        uint8_t *data;					///< Pointer to the byte within current_node that this object is looking at.

        const uint32_t read_word()
        {
            uint8_t b = *(data++);
            uint32_t i = b & 0x7F;
            for (uint32_t shift = 7; (b & 0x80) != 0; shift += 7)
            {
                b = *(data++);
                i |= (b & 0x7FL) << shift;
            }
            return i;
        }

    public:
        iterator(node *node):
            current_node(node),
            element(0),
            data(nullptr)
        {
        }

        bool operator!=(const iterator &other) const
        {
            if (data != other.data)
                return true;
            else
                return false;
        }

        const uint32_t &operator*() const
        {
            return element;
        }

        const iterator &operator++()
        {
            element = read_word();

            if (data >= current_node->data + current_node->used)
            {
                current_node = current_node->next;

                if (current_node == nullptr)
                    data = nullptr;
                else
                    data = current_node->data;
            }
            return *this;
        }
    };

public:
    allocator_pool &pool;				///< The pool allocator used for all allocation by this object.
    node *head;							///< Pointer to the head of the linked list of blocks of data.
    std::atomic<node *> tail;			///< Pointer to the tail of the linked list of blocks of data.  It std::atomic<> so that it can grow lock-free
    double growth_factor;				///< The next chunk in the linked list is this much larger than the previous.

public:
    /*!
    	@brief Constructor.
    	@param pool [in] The pool allocator used for all allocation done by this object.
    	@param initial_size [in] The size (in elements) of the initial allocation in the linked list.
    	@param growth_factor [in] The next node in the linked list stored an element this many times larger than the previous (as an integer).
    */
    explicit compressed_dynamic_array(allocator_pool &pool, size_t initial_size = 1, double growth_factor = 1.5) :
        pool(pool),
        growth_factor(growth_factor)
    {
        head = tail = new (pool.malloc(sizeof(node))) node(pool, initial_size);
    }

    iterator begin(void) const
    {
        /*
        	If there's nothing in the array then we're at the end, else we're at the head.
        */
        if (head->used == 0)
            return end();
        else
            return iterator(head);
    }

    iterator end(void) const
    {
        return iterator(nullptr);
    }

    void push_back(const uint32_t &element)
    {
        do
        {
            node *last = tail;

            ///At least 4 free space for variable byte encoding
            if (7 < last->allocated - last->used)
            {
                uint32_t val = element;
                while ((val & ~0x7F) != 0)
                {
                    last->data[last->used++] = ((uint8_t)((val & 0x7f) | 0x80));
                    val >>= 7;
                }
                last->data[last->used++] = (uint8_t)val;
                break;
            }
            else
            {
                node *another = new (pool.malloc(sizeof(node))) node(pool, (size_t)(last->allocated * growth_factor));
                /*
                	Atomicly make it the tail and if we succeed than make the previous node in the list point to this one.
                	If we fail then the pool allocator won't take the memory back so ignore and re-try
                */
                if (tail.compare_exchange_strong(last, another))
                    last->next = another;
            }
        }
        while(true);
    }

};
}
