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

#include <array>
#include <atomic>
#include <thread>

#include "allocator_pool.hpp"

namespace deepfabric
{
/*
	CLASS DYNAMIC_ARRAY
	-------------------
*/
/*!
	@brief Thread-safe grow-only dynamic array using the thread-safe allocator.
	@details The array data is stored in a linked list of chunks where each chunk is larger then the previous as the array is growing.  Although random access
	is supported, it is slow as it is necessary to walk the linked list to find the given element (see operator[]()).  The iterator, however, does not need to do this and has
	O(1) access time to each element.
	@tparam TYPE The dynamic array is an array of this type.
*/
template <typename TYPE>
class dynamic_array
{
protected:
    /*
    	CLASS DYNAMIC_ARRAY::NODE
    	-------------------------
    */
    /*!
    	@brief The array is stored as a lined list of nodes where each node points to some number of elements.
    */
    class node
    {
    public:
        TYPE *data;						///< The array data for this node.
        node *next;						///< Pointer to the next node in the chain.
        size_t allocated;				///< The size of this node's data object (in elements).
        std::atomic<size_t> used;		///< The number of elements in data that are used (always <= allocated).

    public:
        /*
        	DYNAMIC_ARRAY::NODE::NODE()
        	---------------------------
        */
        /*!
        	@brief Constructor
        	@param pool [in] The pool allocator used to allocate the data controled by this node.
        	@param size [in] The size (in elements) of the data to be controlled by this node.
        */
        node(allocator_pool &pool, size_t size):
            next(nullptr),				// this is the end of the linked list.
            allocated(size),			// the data array is this size (in elements).
            used(0)						// the data array is empty.
        {
            /*
            	Allocate the data this node controls.
            */
            data = new (pool.malloc(size * sizeof(TYPE))) TYPE[size];
        }
    };
public:
    /*
    	CLASS DYNAMIC_ARRAY::ITERATOR
    	-----------------------------
    */
    /*!
    	@brief C++ iterator for iterating over a dynamic_array object.
    	@details See http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html for details on how to write a C++11 iterator.
    */
    class iterator
    {
    private:
        const node *current_node;		///< The node that this iterator is currently looking at.
        TYPE *data;						///< Pointer to the element within current_node that this object is looking at.

    public:
        /*
        	DYNAMIC_ARRAY::ITERATOR::ITERATOR()
        	-----------------------------------
        */
        /*!
        	@brief constructor
        	@param node [in] The node that this iterator should start looking at.
        	@param element [in] Which element within node this iterator should start looking at (normally 0).
        */
        iterator(node *node, size_t element):
            current_node(node),
            data(node == nullptr ? nullptr : node->data + element)					// the "end" is represented as (NULL, NULL)
        {
            /*
            	Nothing
            */
        }
        /*
        	DYNAMIC_ARRAY::ITERATOR::OPERATOR!=()
        	-------------------------------------
        */
        /*!
        	@brief Compare two iterator objects for non-equality.
        	@param other [in] The iterator object to compare to.
        	@return true if they differ, else false.
        */
        bool operator!=(const iterator &other) const
        {
            /*
            	If the data pointers are different then the two must be different.
            */
            if (data != other.data)
                return true;
            else
                return false;
        }

        /*
        	DYNAMIC_ARRAY::ITERATOR::OPERATOR*()
        	------------------------------------
        */
        /*!
        	@brief Return a reference to the element pointed to by this iterator.
        */
        TYPE &operator*() const
        {
            return *data;
        }

        /*
        	DYNAMIC_ARRAY::ITERATOR::OPERATOR++()
        	------------------------------------
        */
        /*!
        	@brief Increment this iterator.
        */
        const iterator &operator++()
        {
            /*
            	Just move on to the next element
            */
            data++;

            /*
            	but if we're past the end of the current node then move on to the next node
            */
            if (data >= current_node->data + current_node->used)
            {
                current_node = current_node->next;

                /*
                	If we're past the end of the list then we're done.
                */
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
    /*
    	DYNAMIC_ARRAY::DYNAMIC_ARRAY()
    	------------------------------
    */
    /*!
    	@brief Constructor.
    	@param pool [in] The pool allocator used for all allocation done by this object.
    	@param initial_size [in] The size (in elements) of the initial allocation in the linked list.
    	@param growth_factor [in] The next node in the linked list stored an element this many times larger than the previous (as an integer).
    */
    explicit dynamic_array(allocator_pool &pool, size_t initial_size = 1, double growth_factor = 1.5) :
        pool(pool),
        growth_factor(growth_factor)
    {
        /*
        	Allocate space for the first write
        */
        head = tail = new (pool.malloc(sizeof(node))) node(pool, initial_size);
    }

    /*
    	DYNAMIC_ARRAY::BEGIN()
    	----------------------
    */
    /*!
    	@brief Return an iterator pointing to the start of the array.
    	@return Iterator pointing to start of array.
    */
    iterator begin(void) const
    {
        /*
        	If there's nothing in the array then we're at the end, else we're at the head.
        */
        if (head->used == 0)
            return end();
        else
            return iterator(head, 0);
    }

    /*
    	DYNAMIC_ARRAY::END()
    	--------------------
    */
    /*!
    	@brief Return an iterator pointing to the end of the array.
    	@return Iterator pointing to end of array.
    */
    iterator end(void) const
    {
        return iterator(nullptr, 0);
    }

    /*
    	DYNAMIC_ARRAY::BACK()
    	---------------------
    */
    /*!
    	@brief Return an reference to the final (used) element in the dynamic array.
    	@return Reference to the last used element in the array.
    */
    TYPE &back(void) const
    {
        return tail.load()->data[tail.load()->used - 1];
    }

    /*
    	DYNAMIC_ARRAY::PUSH_BACK()
    	--------------------------
    */
    /*!
    	@brief Add an element to the end of the array.
    	@param element [in] The element to add.
    */
    void push_back(const TYPE &element)
    {
        do
        {
            /*
            	Take a copy of the pointer to the end of the list
            */
            node *last = tail;

            /*
            	Take a slot (using std::atomic<>++)
            */
            size_t slot = last->used++;

            /*
            	If that slot is within range then copy the element into the array
            */
            if (slot < last->allocated)
            {
                /*
                	Copy and done.
                */
                last->data[slot] = element;
                break;
            }
            else
            {
                /*
                	We've walked past the end so we allocate space for a new node (and elements in that node) and add it to the list.
                */
                last->used = last->allocated;
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

    /*
    	DYNAMIC_ARRAY::OPERATOR[]()
    	---------------------------
    */
    /*!
    	@brief Return a reference to the given element (counting from 0).
    	@details This method must walk the linked list to find
    	the requested element and then returns a reference to it.
    	Since the growth factor might be 1 and the initial
    	allocation size might be 1, the worst case for requesting
    	the final element is O(n) where n is the number of
    	elements in the array.  Walking through the array
    	accessing each element is therefore O(n^2) - so don't do
    	this.  The preferred method for iterating over the array
    	is to use a for each iterator (i.e. through begin() and
    	end()). The C++ std::array has "undefined behavior" if
    	the given index is out-of-range.  This, too, has
    	undefined behaviour in that case.
    	@param element [in] The element to find.
    */
    TYPE &operator[](size_t element)
    {
        /*
        	Walk the linked list until we find the requested element
        */
        for (node *current = head; current != nullptr; current = current->next)
            if (element < current->used)
                return current->data[element];				// got it
            else
                element -= current->used;						// its further down the list

        /*
        	The undefined behaviour is to return the first element in the array.
        	Which will probably cause a crash if there are no elements in the array.
        */
        return head->data[0];
    }

};
}
