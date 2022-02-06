// Copyright (c) 2005-2022 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef POINTERHOLDER_HH
#define POINTERHOLDER_HH

#ifndef POINTERHOLDER_TRANSITION

// In qpdf 11, #define POINTERHOLDER_IS_SHARED_POINTER

// In qpdf 11, issue a warning:
// #define POINTERHOLDER_TRANSITION 0 to suppress this warning, and see below.
// # warn "POINTERHOLDER_TRANSITION is not defined -- see qpdf/PointerHolder.hh"

// undefined = define as 0; will also issue a warning in qpdf 11
// 0 = no deprecation warnings
// 1 = make PointerHolder<T>(T*) explicit
// 2 = warn for use of getPointer() and getRefcount()
// 3 = warn for all use of PointerHolder
// 4 = don't define PointerHolder at all
# define POINTERHOLDER_TRANSITION 0
#endif // !defined(POINTERHOLDER_TRANSITION)

// *** WHAT IS HAPPENING ***

// In qpdf 11, PointerHolder will be replaced with std::shared_ptr
// wherever it appears in the qpdf API. The PointerHolder object will
// be derived from std::shared_ptr to provide a backward-compatible
// interface and will be mutually assignable with std::shared_ptr.
// Code that uses containers of PointerHolder will require adjustment.

// *** HOW TO TRANSITION ***

// The symbol POINTERHOLDER_TRANSITION can be defined to help you
// transition your code away from PointerHolder. You can define it
// before including any qpdf header files or including its definition
// in your build configuration. If not defined, it automatically gets
// defined to 0, which enables full backward compatibility. That way,
// you don't have to take action for your code to continue to work.

// If you want to work gradually to transition your code away from
// PointerHolder, you can define POINTERHOLDER_TRANSITION and fix the
// code so it compiles without warnings and works correctly. If you
// want to be able to continue to support old qpdf versions at the
// same time, you can write code like this:

// #ifndef POINTERHOLDER_TRANSITION
//  ... use PointerHolder as before 10.6
// #else
//  ... use PointerHolder or shared_ptr as needed
// #endif

// Each level of POINTERHOLDER_TRANSITION exposes differences between
// PointerHolder and std::shared_ptr. The easiest way to transition is
// to increase POINTERHOLDER_TRANSITION in steps of 1 so that you can
// test and handle changes incrementally.

// *** Transitions available starting at qpdf 10.6.0 ***

// POINTERHOLDER_TRANSITION = 1
//
// PointerHolder<T> has an implicit constructor that takes a T*, so
// you can replace a PointerHolder<T>'s pointer by directly assigning
// a T* to it or pass a T* to a function that expects a
// PointerHolder<T>. std::shared_ptr does not have this (risky)
// behavior. When POINTERHOLDER_TRANSITION = 1, PointerHolder<T>'s T*
// constructor is declared explicit. For compatibility with
// std::shared_ptr, you can still assign nullptr to a PointerHolder.
// Constructing all your PointerHolder<T> instances explicitly is
// backward compatible, so you can make this change without
// conditional compilation and still use the changes with older qpdf
// versions.
//
// Also defined is a make_pointer_holder method that acts like
// std::make_shared. You can use this as well, but it is not
// compatible with qpdf prior to 10.6. Like std::make_shared<T>,
// make_pointer_holder<T> can only be used when the constructor
// implied by its arguments is public. If you use this, you should be
// able to just replace it with std::make_shared when qpdf 11 is out.

// POINTERHOLDER_TRANSITION = 2
//
// std::shared_ptr as get() and use_count(). PointerHolder has
// getPointer() and getRefcount(). In 10.6.0, get() and use_count()
// were added as well. When POINTERHOLDER_TRANSITION = 2, getPointer()
// and getRefcount() are deprecated. Fix deprecation warnings by
// replacing with get() and use_count(). This breaks compatibility
// with qpdf older than 10.6. Search for CONST BEHAVIOR for an
// additional note.
//
// Once your code is clean at POINTERHOLDER_TRANSITION = 2, the only
// remaining issues that prevent simple replacement of PointerHolder
// with std::shared_ptr are shared arrays and containers, and neither
// of these are used in the qpdf API.

// *** Transitions available starting at qpdf 11.0.0 **

// NOTE: Until qpdf 11 is released, this is a plan and is subject to
// change. Be sure to check again after qpdf 11 is released.

// POINTERHOLDER_TRANSITION = 3
//
// Warn for all use of PointerHolder<T>. This help you remove all use
// of PointerHolder from your code and use std::shared_ptr instead.
// You will also have to transition any containers of PointerHolder in
// your code.

// POINTERHOLDER_TRANSITION = 4
//
// Suppress definition of the PointerHolder<T> type entirely.

// CONST BEHAVIOR

// PointerHolder<T> has had a long-standing bug in its const behavior.
// const PointerHolder<T>'s getPointer() method returns a T const*.
// This is incorrect and is not how regular pointers or standard
// library smart pointers behave. Making a PointerHolder<T> const
// should prevent reassignment of its pointer but not affect the thing
// it points to. For that, use PointerHolder<T const>. The new get()
// method behaves correctly in this respect and is therefore slightly
// different from getPointer(). This shouldn't break any correctly
// written code. If you are relying on the incorrect behavior, use
// PointerHolder<T const> instead.

// OLD DOCUMENTATION

// This class is basically std::shared_ptr but predates that by
// several years.

// This class expects to be initialized with a dynamically allocated
// object pointer.  It keeps a reference count and deletes this once
// the reference count goes to zero.  PointerHolder objects are
// explicitly safe for use in STL containers.

// It is very important that a client who pulls the pointer out of
// this holder does not let the holder go out of scope until it is
// finished with the pointer.  It is also important that exactly one
// instance of this object ever gets initialized with a given pointer.
// Otherwise, the pointer will be deleted twice, and before that, some
// objects will be left with a pointer to a deleted object.  In other
// words, the only legitimate way for two PointerHolder objects to
// contain the same pointer is for one to be a copy of the other.
// Copy and assignment semantics are well-defined and essentially
// allow you to use PointerHolder as a means to get pass-by-reference
// semantics in a pass-by-value environment without having to worry
// about memory management details.

// Comparison (== and <) are defined and operate on the internally
// stored pointers, not on the data.  This makes it possible to store
// PointerHolder objects in sorted lists or to find them in STL
// containers just as one would be able to store pointers.  Comparing
// the underlying pointers provides a well-defined, if not
// particularly meaningful, ordering.

#include <cstddef>

template <class T>
class PointerHolder
{
  private:
    class Data
    {
      public:
        Data(T* pointer, bool array) :
            pointer(pointer),
            array(array),
            refcount(0)
        {
        }
        ~Data()
        {
            if (array)
            {
                delete [] this->pointer;
            }
            else
            {
                delete this->pointer;
            }
        }
        T* pointer;
        bool array;
        int refcount;
      private:
        Data(Data const&) = delete;
        Data& operator=(Data const&) = delete;
    };

  public:
#if POINTERHOLDER_TRANSITION >= 1
    explicit
#endif // POINTERHOLDER_TRANSITION >= 1
    PointerHolder(T* pointer = 0)
    {
        this->init(new Data(pointer, false));
    }
    // Special constructor indicating to free memory with delete []
    // instead of delete
    PointerHolder(bool, T* pointer)
    {
        this->init(new Data(pointer, true));
    }
    PointerHolder(PointerHolder const& rhs)
    {
        this->copy(rhs);
    }
    PointerHolder& operator=(PointerHolder const& rhs)
    {
        if (this != &rhs)
        {
            this->destroy();
            this->copy(rhs);
        }
        return *this;
    }
    PointerHolder& operator=(decltype(nullptr))
    {
        this->operator=(PointerHolder<T>());
        return *this;
    }
    ~PointerHolder()
    {
        this->destroy();
    }
    bool operator==(PointerHolder const& rhs) const
    {
        return this->data->pointer == rhs.data->pointer;
    }
    bool operator==(decltype(nullptr)) const
    {
        return this->data->pointer == nullptr;
    }
    bool operator<(PointerHolder const& rhs) const
    {
        return this->data->pointer < rhs.data->pointer;
    }

    // get() is for interface compatibility with std::shared_ptr
    T* get() const
    {
        return this->data->pointer;
    }

    // NOTE: The pointer returned by getPointer turns into a pumpkin
    // when the last PointerHolder that contains it disappears.
#if POINTERHOLDER_TRANSITION >= 2
    [[deprecated("use PointerHolder<T>::get() instead of getPointer()")]]
#endif // POINTERHOLDER_TRANSITION >= 2
    T* getPointer()
    {
        return this->data->pointer;
    }
#if POINTERHOLDER_TRANSITION >= 2
    [[deprecated("use PointerHolder<T>::get() instead of getPointer()")]]
#endif // POINTERHOLDER_TRANSITION >= 2
    T const* getPointer() const
    {
        return this->data->pointer;
    }
#if POINTERHOLDER_TRANSITION >= 2
    [[deprecated("use use_count() instead of getRefcount()")]]
#endif // POINTERHOLDER_TRANSITION >= 2
    int getRefcount() const
    {
        return this->data->refcount;
    }

    // use_count() is for compatibility with std::shared_ptr
    long use_count()
    {
        return static_cast<long>(this->data->refcount);
    }

    T const& operator*() const
    {
        return *this->data->pointer;
    }
    T& operator*()
    {
        return *this->data->pointer;
    }

    T const* operator->() const
    {
        return this->data->pointer;
    }
    T* operator->()
    {
        return this->data->pointer;
    }

  private:
    void init(Data* data)
    {
        this->data = data;
        ++this->data->refcount;
    }
    void copy(PointerHolder const& rhs)
    {
        this->init(rhs.data);
    }
    void destroy()
    {
        bool gone = false;
        {
            if (--this->data->refcount == 0)
            {
                gone = true;
            }
        }
        if (gone)
        {
            delete this->data;
        }
    }

    Data* data;
};

template<typename T, typename... _Args>
inline PointerHolder<T>
make_pointer_holder(_Args&&... __args)
{
    return PointerHolder<T>(new T(__args...));
}

template <typename T>
PointerHolder<T>
make_array_pointer_holder(size_t n)
{
    return PointerHolder<T>(true, new T[n]);
}

#endif // POINTERHOLDER_HH
