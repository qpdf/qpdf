// Copyright (c) 2005-2019 Jay Berkenbilt
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

// Starting with qpdf 10, this class is a wrapper around
// std::shared_ptr with special handling for arrays and ordering.
// PointerHolder<T> can be used interchangeably with
// std::shared_ptr<T> as bidirectional conversion is supported. This
// goes for both the regular and array forms.

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

#include <memory>

template <class T>
class PointerHolder
{
  public:
    PointerHolder(T* pointer = 0) :
        p(pointer)
    {
    }
    // Special constructor indicating to free memory with delete []
    // instead of delete
    PointerHolder(bool, T* pointer) :
        p(pointer, std::default_delete<T[]>())
    {
    }
    // Construct with a shared_ptr
    PointerHolder(std::shared_ptr<T> const& p) :
        p(p)
    {
    }

    bool operator==(PointerHolder const& rhs) const
    {
	return this->p.get() == rhs.p.get();
    }
    bool operator<(PointerHolder const& rhs) const
    {
	return this->p.get() < rhs.p.get();
    }
    operator std::shared_ptr<T> const() const
    {
        return this->p;
    }
    operator std::shared_ptr<T>()
    {
        return this->p;
    }

    // NOTE: The pointer returned by getPointer turns into a pumpkin
    // when the last PointerHolder that contains it disappears.
    T* getPointer()
    {
        return this->p.get();
    }
    T const* getPointer() const
    {
        return this->p.get();
    }
    int getRefcount() const
    {
        return static_cast<int>(this->p.use_count());
    }

    T const& operator*() const
    {
        return *(this->p);
    }
    T& operator*()
    {
        return *(this->p);
    }

    T const* operator->() const
    {
        return this->p.get();
    }
    T* operator->()
    {
        return this->p.get();
    }

  private:
    std::shared_ptr<T> p;
};

#endif // POINTERHOLDER_HH
