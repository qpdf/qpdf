#include <qpdf/Pl_SHA2.hh>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <qpdf/QUtil.hh>

static void test(Pl_SHA2& sha2, char const* description, int bits,
                 char const* input, std::string const& output)
{
    sha2.resetBits(bits);
    sha2.write(QUtil::unsigned_char_pointer(input), strlen(input));
    sha2.finish();
    std::cout << description << ": ";
    if (output == sha2.getHexDigest())
    {
        std::cout << "passed\n";
    }
    else
    {
        std::cout << "failed\n"
                  << "  expected: " << output << "\n"
                  << "  actual:   " << sha2.getHexDigest() << "\n";
    }
}

int main( int argc, char *argv[] )
{
    Pl_SHA2 sha2;
    char million_a[1000001];
    memset(million_a, 'a', 1000000);
    million_a[1000000] = '\0';
    test(sha2, "256 short", 256,
         "abc",
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    test(sha2, "256 long", 256,
         "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    test(sha2, "256 million", 256,
         million_a,
         "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    test(sha2, "384 short", 384,
         "abc",
         "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded163"
         "1a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
    test(sha2, "384 long", 384,
         "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
         "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
         "09330c33f71147e83d192fc782cd1b4753111b173b3b05d2"
         "2fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039");
    test(sha2, "384 million", 384,
         million_a,
         "9d0e1809716474cb086e834e310a4a1ced149e9c00f24852"
         "7972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985");
    test(sha2, "512 short", 512,
         "abc",
         "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
         "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    test(sha2, "512 long", 512,
         "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
         "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
         "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
         "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
    test(sha2, "512 million", 512,
         million_a,
         "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
         "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");

    return 0;
}
