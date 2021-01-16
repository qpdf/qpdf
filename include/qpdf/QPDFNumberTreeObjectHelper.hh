// Copyright (c) 2005-2021 Jay Berkenbilt
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

#ifndef QPDFNUMBERTREEOBJECTHELPER_HH
#define QPDFNUMBERTREEOBJECTHELPER_HH

#include <qpdf/QPDFObjectHelper.hh>
#include <qpdf/QPDFObjGen.hh>
#include <map>
#include <memory>

#include <qpdf/DLL.h>

// This is an object helper for number trees. See section 7.9.7 in the
// PDF spec (ISO 32000) for a description of number trees.

class NNTreeImpl;

class QPDFNumberTreeObjectHelper: public QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFNumberTreeObjectHelper(QPDFObjectHandle);
    QPDF_DLL
    virtual ~QPDFNumberTreeObjectHelper()
    {
    }

    typedef long long int numtree_number;

    // Return overall minimum and maximum indices
    QPDF_DLL
    numtree_number getMin();
    QPDF_DLL
    numtree_number getMax();

    // Return whether the number tree has an explicit entry for this
    // number.
    QPDF_DLL
    bool hasIndex(numtree_number idx);

    // Find an object with a specific index. If found, returns true
    // and initializes oh.
    QPDF_DLL
    bool findObject(numtree_number idx, QPDFObjectHandle& oh);
    // Find the object at the index or, if not found, the object whose
    // index is the highest index less than the requested index. If
    // the requested index is less than the minimum, return false.
    // Otherwise, return true, initialize oh to the object, and set
    // offset to the difference between the requested index and the
    // actual index. For example, if a number tree has values for 3
    // and 6 and idx is 5, this method would return true, initialize
    // oh to the value with index 3, and set offset to 2 (5 - 3).
    QPDF_DLL
    bool findObjectAtOrBelow(numtree_number idx, QPDFObjectHandle& oh,
                             numtree_number& offset);

    // Return the contents of the number tree as a map. Note that
    // number trees may be very large, so this may use a lot of RAM.
    // It is more efficient to use QPDFNumberTreeObjectHelper's
    // iterator.
    typedef std::map<numtree_number, QPDFObjectHandle> idx_map;
    QPDF_DLL
    idx_map getAsMap() const;

  private:
    class Members
    {
        friend class QPDFNumberTreeObjectHelper;
        typedef QPDFNumberTreeObjectHelper::numtree_number numtree_number;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members(QPDFObjectHandle& oh);
        Members(Members const&) = delete;

        std::shared_ptr<NNTreeImpl> impl;
    };

    PointerHolder<Members> m;
};

#endif // QPDFNUMBERTREEOBJECTHELPER_HH
