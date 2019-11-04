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

#ifndef QPDFCRYPTOIMPL_HH
#define QPDFCRYPTOIMPL_HH

#include <qpdf/DLL.h>
#include <cstring>

// This class is part of qpdf's pluggable crypto provider support.
// Most users won't need to know or care about this class, but you can
// use it if you want to supply your own crypto implementation. To do
// so, provide an implementation of QPDFCryptoImpl, ensure that you
// register it by calling QPDFCryptoProvider::registerImpl, and make
// it the default by calling QPDFCryptoProvider::setDefaultProvider.

class QPDF_DLL_CLASS QPDFCryptoImpl
{
  public:
    QPDF_DLL
    QPDFCryptoImpl() = default;

    QPDF_DLL
    virtual ~QPDFCryptoImpl() = default;

    typedef unsigned char MD5_Digest[16];
    QPDF_DLL
    virtual void MD5_init() = 0;
    QPDF_DLL
    virtual void MD5_update(unsigned char const* data, size_t len) = 0;
    QPDF_DLL
    virtual void MD5_finalize() = 0;
    QPDF_DLL
    virtual void MD5_digest(MD5_Digest) = 0;

    // key_len of -1 means treat key_data as a null-terminated string
    QPDF_DLL
    virtual void RC4_init(unsigned char const* key_data, int key_len = -1) = 0;
    // out_data = 0 means to encrypt/decrypt in place
    QPDF_DLL
    virtual void RC4_process(unsigned char* in_data, size_t len,
                             unsigned char* out_data = 0) = 0;
    QPDF_DLL
    virtual void RC4_finalize() = 0;
};

#endif // QPDFCRYPTOIMPL_HH
