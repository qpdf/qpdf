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

#ifndef QPDFCRYPTOPROVIDER_HH
#define QPDFCRYPTOPROVIDER_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFCryptoImpl.hh>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

// This class is part of qpdf's pluggable crypto provider support.
// Most users won't need to know or care about this class, but you can
// use it if you want to supply your own crypto implementation. See
// also comments in QPDFCryptoImpl.hh.

class QPDFCryptoProvider
{
  public:
    // Methods for getting and registering crypto implementations.
    // These methods are not thread-safe.

    // Return an instance of a crypto provider using the default
    // implementation.
    QPDF_DLL
    static std::shared_ptr<QPDFCryptoImpl> getImpl();

    // Return an instance of the crypto provider registered using the
    // given name.
    QPDF_DLL
    static std::shared_ptr<QPDFCryptoImpl> getImpl(std::string const& name);

    // Register the given type (T) as a crypto implementation. T must
    // be derived from QPDFCryptoImpl and must have a constructor that
    // takes no arguments.
    template <typename T>
    QPDF_DLL static void registerImpl(std::string const& name);

    // Set the crypto provider registered with the given name as the
    // default crypto implementation.
    QPDF_DLL
    static void setDefaultProvider(std::string const& name);

    // Get the names of registered implementations
    QPDF_DLL
    static std::set<std::string> getRegisteredImpls();

    // Get the name of the default crypto provider
    QPDF_DLL
    static std::string getDefaultProvider();

  private:
    QPDFCryptoProvider();
    ~QPDFCryptoProvider() = default;
    QPDFCryptoProvider(QPDFCryptoProvider const&) = delete;
    QPDFCryptoProvider& operator=(QPDFCryptoProvider const&) = delete;

    static QPDFCryptoProvider& getInstance();

    std::shared_ptr<QPDFCryptoImpl>
    getImpl_internal(std::string const& name) const;
    template <typename T>
    void registerImpl_internal(std::string const& name);
    void setDefaultProvider_internal(std::string const& name);

    class Members
    {
        friend class QPDFCryptoProvider;

      public:
        Members() = default;
        QPDF_DLL
        ~Members() = default;

      private:
        Members(Members const&) = delete;
        Members& operator=(Members const&) = delete;

        typedef std::function<std::shared_ptr<QPDFCryptoImpl>()> provider_fn;
        std::string default_provider;
        std::map<std::string, provider_fn> providers;
    };

    std::shared_ptr<Members> m;
};

#endif // QPDFCRYPTOPROVIDER_HH
