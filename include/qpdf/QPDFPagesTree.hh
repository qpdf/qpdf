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

#ifndef QPDFPAGESTREE_HH
#define QPDFPAGESTREE_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFObjectHandle.hh>

class QPDFPagesTree
{
  public:
    QPDF_DLL
    QPDFPagesTree(QPDF&);

    QPDF_DLL
    virtual ~QPDFPagesTree() = default;

    QPDF_DLL
    size_t size();

    QPDF_DLL
    QPDFObjectHandle at(size_t);

    QPDF_DLL
    QPDFObjectHandle operator[](size_t);

    QPDF_DLL
    QPDFObjectHandle getPagesRoot() const;

    class iterator
    {
        friend class QPDFPagesTree;

      public:
        typedef QPDFObjectHandle T;
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

        // Insert before iterator. When called on end(), add to the
        // end.
        QPDF_DLL
        void insert(QPDFObjectHandle page);

        // Remove the current item and advance the iterator to the
        // next item.
        QPDF_DLL
        void remove();

      private:
        class PathElement
        {
          public:
            PathElement(QPDFObjectHandle const& node, int kid_number);

            QPDFObjectHandle node;
            int kid_number;
        };

        iterator(QPDFPagesTree const&);
        void updateIValue(bool allow_invalid = true);
        void addPathElement(QPDFObjectHandle const& node, int kid_number);
        void increment(bool backward);
        void maybeSetType(QPDFObjectHandle node, std::string const& type);
        QPDFObjectHandle getKidIndirect(QPDFObjectHandle node, int kid_number);
        bool deepen(QPDFObjectHandle node, bool first);
        void updateCounts(QPDFObjectHandle node);
        void split(
            QPDFObjectHandle to_split, std::list<PathElement>::iterator parent);

        class Members
        {
            friend class QPDFPagesTree::iterator;

          public:
            QPDF_DLL
            ~Members() = default;

          private:
            Members(QPDFPagesTree const&);
            Members(Members const&) = delete;

            QPDF& qpdf;
            QPDFObjectHandle pages_root;
            std::list<PathElement> path;
            QPDFObjectHandle ivalue;
            int split_threshold;
        };

        std::shared_ptr<Members> m;
    };

    // The iterators are circular, so incrementing end() gives you
    // begin(), decrementing end() gives you the last item, and
    // decrementing begin() gives you end().
    QPDF_DLL
    iterator begin() const;
    QPDF_DLL
    iterator end() const;

    // Split a node if the number of items exceeds this value. There's
    // no real reason to ever set this except for testing.
    QPDF_DLL
    void setSplitThreshold(int);

  private:
    class Members
    {
        friend class QPDFPagesTree;

      public:
        QPDF_DLL
        ~Members() = default;

      private:
        Members(QPDF& qpdf);
        Members(Members const&) = delete;

        QPDF& qpdf;
        QPDFObjectHandle pages_root;
        int split_threshold;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFPAGESTREE_HH
