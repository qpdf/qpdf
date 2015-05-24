// Copyright (c) 2005-2015 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __POINTERHOLDER_HH__
#define __POINTERHOLDER_HH__

// This class is basically boost::shared_pointer but predates that by
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
	Data(Data const&);
	Data& operator=(Data const&);
    };

  public:
    // "tracing" is not used but is kept for interface backward compatbility
    PointerHolder(T* pointer = 0, bool tracing = false)
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
    ~PointerHolder()
	{
	    this->destroy();
	}
    bool operator==(PointerHolder const& rhs) const
    {
	return this->data->pointer == rhs.data->pointer;
    }
    bool operator<(PointerHolder const& rhs) const
    {
	return this->data->pointer < rhs.data->pointer;
    }

    // NOTE: The pointer returned by getPointer turns into a pumpkin
    // when the last PointerHolder that contains it disappears.
    T* getPointer()
	{
	    return this->data->pointer;
	}
    T const* getPointer() const
	{
	    return this->data->pointer;
	}
    int getRefcount() const
	{
	    return this->data->refcount;
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
	    {
		++this->data->refcount;
	    }
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

#endif // __POINTERHOLDER_HH__
