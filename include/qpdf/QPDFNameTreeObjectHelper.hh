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

#ifndef QPDFNAMETREEOBJECTHELPER_HH
#define QPDFNAMETREEOBJECTHELPER_HH

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHelper.hh>
#include <iterator>
#include <map>
#include <memory>

#include <qpdf/DLL.h>

// This is an object helper for name trees. See section 7.9.6 in the
// PDF spec (ISO 32000) for a description of name trees. When looking
// up items in the name tree, use UTF-8 strings. All names are
// normalized for lookup purposes.

// See examples/pdf-name-number-tree.cc for a demonstration of using
// QPDFNameTreeObjectHelper.

class NNTreeImpl;
class NNTreeIterator;
class NNTreeDetails;

class QPDF_DLL_CLASS QPDFNameTreeObjectHelper: public QPDFObjectHelper
{
  public:
    // The qpdf object is required so that this class can issue
    // warnings, attempt repairs, and add indirect objects.
    QPDF_DLL
    QPDFNameTreeObjectHelper(QPDFObjectHandle, QPDF&, bool auto_repair = true);

    // Create an empty name tree
    QPDF_DLL
    static QPDFNameTreeObjectHelper newEmpty(QPDF&, bool auto_repair = true);

    QPDF_DLL
    virtual ~QPDFNameTreeObjectHelper();

    // Return whether the number tree has an explicit entry for this
    // number.
    QPDF_DLL
    bool hasName(std::string const& utf8);

    // Find an object by name. If found, returns true and initializes
    // oh. See also find().
    QPDF_DLL
    bool findObject(std::string const& utf8, QPDFObjectHandle& oh);

    class QPDF_DLL_PRIVATE iterator
    {
        friend class QPDFNameTreeObjectHelper;

      public:
        typedef std::pair<std::string, QPDFObjectHandle> T;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = long;
        using pointer = T*;
        using reference = T&;

        virtual ~iterator() = default;
        QPDF_DLL
        bool valid() const;
        QPDF_DLL
        iterator& operator++();
        QPDF_DLL
        iterator
        operator++(int)
        {
            iterator t = *this;
            ++(*this);
            return t;
        }
        QPDF_DLL
        iterator& operator--();
        QPDF_DLL
        iterator
        operator--(int)
        {
            iterator t = *this;
            --(*this);
            return t;
        }
        QPDF_DLL
        reference operator*();
        QPDF_DLL
        pointer operator->();
        QPDF_DLL
        bool operator==(iterator const& other) const;
        QPDF_DLL
        bool
        operator!=(iterator const& other) const
        {
            return !operator==(other);
        }

        // DANGER: this method can create inconsistent trees if not
        // used properly! Insert a new item immediately after the
        // current iterator and increment so that it points to the new
        // item. If the current iterator is end(), insert at the
        // beginning. This method does not check for proper ordering,
        // so if you use it, you must ensure that the item you are
        // inserting belongs where you are putting it. The reason for
        // this method is that it is more efficient than insert() and
        // can be used safely when you are creating a new tree and
        // inserting items in sorted order.
        QPDF_DLL
        void insertAfter(std::string const& key, QPDFObjectHandle value);

        // Remove the current item and advance the iterator to the
        // next item.
        QPDF_DLL
        void remove();

      private:
        void updateIValue();

        iterator(std::shared_ptr<NNTreeIterator> const&);
        std::shared_ptr<NNTreeIterator> impl;
        value_type ivalue;
    };

    // The iterator looks like map iterator, so i.first is a string
    // and i.second is a QPDFObjectHandle. Incrementing end() brings
    // you to the first item. Decrementing end() brings you to the
    // last item.
    QPDF_DLL
    iterator begin() const;
    QPDF_DLL
    iterator end() const;
    // Return a bidirectional iterator that points to the last item.
    QPDF_DLL
    iterator last() const;

    // Find the entry with the given key. If return_prev_if_not_found
    // is true and the item is not found, return the next lower item.
    QPDF_DLL
    iterator
    find(std::string const& key, bool return_prev_if_not_found = false);

    // Insert a new item. If the key already exists, it is replaced.
    QPDF_DLL
    iterator insert(std::string const& key, QPDFObjectHandle value);

    // Remove an item. Return true if the item was found and removed;
    // otherwise return false. If value is not null, initialize it to
    // the value that was removed.
    QPDF_DLL
    bool remove(std::string const& key, QPDFObjectHandle* value = nullptr);

    // Return the contents of the name tree as a map. Note that name
    // trees may be very large, so this may use a lot of RAM. It is
    // more efficient to use QPDFNameTreeObjectHelper's iterator.
    QPDF_DLL
    std::map<std::string, QPDFObjectHandle> getAsMap() const;

    // Split a node if the number of items exceeds this value. There's
    // no real reason to ever set this except for testing.
    QPDF_DLL
    void setSplitThreshold(int);

  private:
    class QPDF_DLL_PRIVATE Members
    {
        friend class QPDFNameTreeObjectHelper;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(QPDFObjectHandle& oh, QPDF&, bool auto_repair);
        Members(Members const&) = delete;

        std::shared_ptr<NNTreeImpl> impl;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFNAMETREEOBJECTHELPER_HH
