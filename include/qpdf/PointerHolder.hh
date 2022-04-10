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

#define POINTERHOLDER_IS_SHARED_POINTER

#ifndef POINTERHOLDER_TRANSITION

// #define POINTERHOLDER_TRANSITION 0 to suppress this warning, and see below.
// See also https://qpdf.readthedocs.io/en/stable/design.html#smart-pointers
# warning "POINTERHOLDER_TRANSITION is not defined -- see qpdf/PointerHolder.hh"

// undefined = define as 0 and issue a warning
// 0 = no deprecation warnings, backward-compatible API
// 1 = make PointerHolder<T>(T*) explicit
// 2 = warn for use of getPointer() and getRefcount()
// 3 = warn for all use of PointerHolder
// 4 = don't define PointerHolder at all
# define POINTERHOLDER_TRANSITION 0
#endif // !defined(POINTERHOLDER_TRANSITION)

#if POINTERHOLDER_TRANSITION < 4

// *** WHAT IS HAPPENING ***

// In qpdf 11, PointerHolder was replaced with std::shared_ptr
// wherever it appeared in the qpdf API. The PointerHolder object is
// now derived from std::shared_ptr to provide a backward-compatible
// interface and is mutually assignable with std::shared_ptr. Code
// that uses containers of PointerHolder will require adjustment.

// *** HOW TO TRANSITION ***

// The symbol POINTERHOLDER_TRANSITION can be defined to help you
// transition your code away from PointerHolder. You can define it
// before including any qpdf header files or including its definition
// in your build configuration. If not defined, it automatically gets
// defined to 0 (with a warning), which enables full backward
// compatibility. That way, you don't have to take action for your
// code to continue to work.

// If you want to work gradually to transition your code away from
// PointerHolder, you can define POINTERHOLDER_TRANSITION and fix the
// code so it compiles without warnings and works correctly. If you
// want to be able to continue to support old qpdf versions at the
// same time, you can write code like this:

// #ifndef POINTERHOLDER_IS_SHARED_POINTER
//  ... use PointerHolder as before 10.6
// #else
//  ... use PointerHolder or shared_ptr as needed
// #endif

// Each level of POINTERHOLDER_TRANSITION exposes differences between
// PointerHolder and std::shared_ptr. The easiest way to transition is
// to increase POINTERHOLDER_TRANSITION in steps of 1 so that you can
// test and handle changes incrementally.

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
// compatible with qpdf prior to 10.6 and not necessary with qpdf
// newer than 10.6.3. Like std::make_shared<T>, make_pointer_holder<T>
// can only be used when the constructor implied by its arguments is
// public. If you previously used this, you can replace it width
// std::make_shared now.

// POINTERHOLDER_TRANSITION = 2
//
// std::shared_ptr has get() and use_count(). PointerHolder has
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

// POINTERHOLDER_TRANSITION = 3
//
// Warn for all use of PointerHolder<T>. This helps you remove all use
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

# include <cstddef>
# include <memory>

template <class T>
class PointerHolder: public std::shared_ptr<T>
{
  public:
# if POINTERHOLDER_TRANSITION >= 3
    [[deprecated("use std::shared_ptr<T> instead")]]
# endif // POINTERHOLDER_TRANSITION >= 3
    PointerHolder(std::shared_ptr<T> other) :
        std::shared_ptr<T>(other)
    {
    }
# if POINTERHOLDER_TRANSITION >= 3
    [[deprecated("use std::shared_ptr<T> instead")]]
#  if POINTERHOLDER_TRANSITION >= 1
    explicit
#  endif // POINTERHOLDER_TRANSITION >= 1
# endif  // POINTERHOLDER_TRANSITION >= 3
        PointerHolder(T* pointer = 0) :
        std::shared_ptr<T>(pointer)
    {
    }
    // Create a shared pointer to an array
# if POINTERHOLDER_TRANSITION >= 3
    [[deprecated("use std::shared_ptr<T> instead")]]
# endif // POINTERHOLDER_TRANSITION >= 3
    PointerHolder(bool, T* pointer) :
        std::shared_ptr<T>(pointer, std::default_delete<T[]>())
    {
    }

    virtual ~PointerHolder() = default;

# if POINTERHOLDER_TRANSITION >= 2
    [[deprecated("use PointerHolder<T>::get() instead of getPointer()")]]
# endif // POINTERHOLDER_TRANSITION >= 2
    T*
    getPointer()
    {
        return this->get();
    }
# if POINTERHOLDER_TRANSITION >= 2
    [[deprecated("use PointerHolder<T>::get() instead of getPointer()")]]
# endif // POINTERHOLDER_TRANSITION >= 2
    T const*
    getPointer() const
    {
        return this->get();
    }

# if POINTERHOLDER_TRANSITION >= 2
    [[deprecated("use PointerHolder<T>::get() instead of getPointer()")]]
# endif // POINTERHOLDER_TRANSITION >= 2
    int
    getRefcount() const
    {
        return static_cast<int>(this->use_count());
    }

    PointerHolder&
    operator=(decltype(nullptr))
    {
        std::shared_ptr<T>::operator=(nullptr);
        return *this;
    }
    T const&
    operator*() const
    {
        return *(this->get());
    }
    T&
    operator*()
    {
        return *(this->get());
    }

    T const*
    operator->() const
    {
        return this->get();
    }
    T*
    operator->()
    {
        return this->get();
    }
};

template <typename T, typename... _Args>
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

#endif // POINTERHOLDER_TRANSITION < 4
#endif // POINTERHOLDER_HH
