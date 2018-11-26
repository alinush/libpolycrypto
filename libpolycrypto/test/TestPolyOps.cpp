#include <polycrypto/Configuration.h>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/PolyOps.h>



#include <xassert/XAssert.h>
#include <xutils/Log.h>

using namespace libpolycrypto;

std::vector<Fr> poly_from_roots_naive(const std::vector<Fr>& roots) {
    std::vector<Fr> monom(2), acc(1, Fr::one());
    for(size_t i = 0; i < roots.size(); i++) {
        Fr r = roots[i];
        //logdbg << "Multiplying in root " << r << endl;

        monom[0] = -r;
        monom[1] = 1;
        libfqfft::_polynomial_multiplication_on_fft(acc, acc, monom);

        testAssertEqual(acc.size(), i+2);

        //poly_print(acc);
    }

    return acc;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    libpolycrypto::initialize(nullptr, 0);

    const size_t maxSize = 64;
    std::vector<Fr> roots;
    for(size_t i = 0; i < maxSize; i++) {
        roots.push_back(Fr::random_element());
        loginfo << "Testing interpolation from " << i+1 << " roots..." << endl;

        auto expected = poly_from_roots_naive(roots); 
        auto computed = poly_from_roots(roots); 

        testAssertEqual(computed.size(), roots.size() + 1);
        testAssertEqual(expected, computed);
    }

    return 0;
}
