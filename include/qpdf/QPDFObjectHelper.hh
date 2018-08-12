// Copyright (c) 2005-2018 Jay Berkenbilt
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

#ifndef QPDFOBJECTHELPER_HH
#define QPDFOBJECTHELPER_HH

#include <qpdf/DLL.h>

#include <qpdf/QPDFObjectHandle.hh>

// This is a base class for QPDF Object Helper classes. Object helpers
// are classes that provide a convenient, higher-level API for working
// with specific types of QPDF objects. Object helpers are always
// initialized with a QPDFObjectHandle, and the underlying object
// handle can always be retrieved. The intention is that you may
// freely intermix use of document helpers with the underlying QPDF
// objects unless there is a specific comment in a specific helper
// method that says otherwise. The pattern of using helper objects was
// introduced to allow creation of higher level helper functions
// without polluting the public interface of QPDFObjectHandle.

class QPDFObjectHelper
{
  public:
    QPDF_DLL
    QPDFObjectHelper(QPDFObjectHandle oh) :
        oh(oh)
    {
    }
    QPDF_DLL
    virtual ~QPDFObjectHelper()
    {
    }
    QPDF_DLL
    QPDFObjectHandle getObjectHandle()
    {
        return this->oh;
    }
    QPDF_DLL
    QPDFObjectHandle const getObjectHandle() const
    {
        return this->oh;
    }

  protected:
    QPDFObjectHandle oh;
};

#endif // QPDFOBJECTHELPER_HH
