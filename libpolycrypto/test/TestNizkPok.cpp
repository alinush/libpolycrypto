#include <polycrypto/Configuration.h>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/NizkPok.h>

#include <xassert/XAssert.h>
#include <xutils/Log.h>

using namespace libpolycrypto;

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    libpolycrypto::initialize(nullptr, 0);

    for(size_t i = 0; i < 128; i++) {
        Fr x = Fr::random_element();
        Fr y = Fr::random_element();
        G1 g = G1::one();
        G1 h = G1::random_element();
        G1 gToX = x * g;

        NizkPok pi = NizkPok::prove(g, x, gToX);
        testAssertTrue(NizkPok::verify(pi, g, gToX));
        testAssertFalse(NizkPok::verify(pi, h, gToX));
        testAssertFalse(NizkPok::verify(pi, g, y*g));
        testAssertFalse(NizkPok::verify(pi, h, y*g));
    }

    std::cout << "Test '" << argv[0] << "' finished successfully" << std::endl;

    return 0;
}
