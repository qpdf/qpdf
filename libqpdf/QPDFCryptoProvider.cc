#include <qpdf/QPDFCryptoProvider.hh>

#include <qpdf/QUtil.hh>
#include <qpdf/qpdf-config.h>
#include <stdexcept>

#ifdef USE_CRYPTO_NATIVE
# include <qpdf/QPDFCrypto_native.hh>
#endif
#ifdef USE_CRYPTO_GNUTLS
# include <qpdf/QPDFCrypto_gnutls.hh>
#endif
#ifdef USE_CRYPTO_OPENSSL
# include <qpdf/QPDFCrypto_openssl.hh>
#endif

std::shared_ptr<QPDFCryptoImpl>
QPDFCryptoProvider::getImpl()
{
    QPDFCryptoProvider& p = getInstance();
    if (p.m->default_provider.empty()) {
        throw std::logic_error("QPDFCryptoProvider::getImpl called with no default provider.");
    }
    return p.getImpl_internal(p.m->default_provider);
}

std::shared_ptr<QPDFCryptoImpl>
QPDFCryptoProvider::getImpl(std::string const& name)
{
    return getInstance().getImpl_internal(name);
}

void
QPDFCryptoProvider::registerImpl(std::string const& name, provider_fn f)
{
    getInstance().registerImpl_internal(name, std::move(f));
}

void
QPDFCryptoProvider::setDefaultProvider(std::string const& name)
{
    getInstance().setDefaultProvider_internal(name);
}

QPDFCryptoProvider::QPDFCryptoProvider() :
    m(std::make_shared<Members>())
{
#ifdef USE_CRYPTO_NATIVE
    registerImpl_internal("native", std::make_shared<QPDFCrypto_native>);
#endif
#ifdef USE_CRYPTO_GNUTLS
    registerImpl_internal("gnutls", std::make_shared<QPDFCrypto_gnutls>);
#endif
#ifdef USE_CRYPTO_OPENSSL
    registerImpl_internal("openssl", std::make_shared<QPDFCrypto_openssl>);
#endif
    std::string default_crypto;
    if (!QUtil::get_env("QPDF_CRYPTO_PROVIDER", &default_crypto)) {
        default_crypto = DEFAULT_CRYPTO;
    }
    setDefaultProvider_internal(default_crypto);
}

QPDFCryptoProvider&
QPDFCryptoProvider::getInstance()
{
    static QPDFCryptoProvider instance;
    return instance;
}

std::shared_ptr<QPDFCryptoImpl>
QPDFCryptoProvider::getImpl_internal(std::string const& name) const
{
    auto iter = m->providers.find(name);
    if (iter == m->providers.end()) {
        throw std::logic_error(
            "QPDFCryptoProvider requested unknown implementation \"" + name + "\"");
    }
    return m->providers[name]();
}

void
QPDFCryptoProvider::registerImpl_internal(std::string const& name, provider_fn f)
{
    m->providers[name] = std::move(f);
}

void
QPDFCryptoProvider::setDefaultProvider_internal(std::string const& name)
{
    if (!m->providers.count(name)) {
        throw std::logic_error(
            "QPDFCryptoProvider: request to set default provider to unknown implementation \"" +
            name + "\"");
    }
    m->default_provider = name;
}

std::set<std::string>
QPDFCryptoProvider::getRegisteredImpls()
{
    std::set<std::string> result;
    QPDFCryptoProvider& p = getInstance();
    for (auto const& iter: p.m->providers) {
        result.insert(iter.first);
    }
    return result;
}

std::string
QPDFCryptoProvider::getDefaultProvider()
{
    return getInstance().m->default_provider;
}
