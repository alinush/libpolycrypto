#include <polycrypto/Configuration.h>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/PolyOps.h>
#include <polycrypto/NtlLib.h>

#include <xassert/XAssert.h>
#include <xutils/Log.h>
#include <xutils/Timer.h>

using namespace libpolycrypto;

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    libpolycrypto::initialize(nullptr, 0);

    size_t count = 10;
    if(argc < 3) {
        cout << "Usage: " << argv[0] << " <n> <m> [<count>]" << endl;
        cout << endl;
        cout << "Divides random poly a(x) (of deg n) by b(x) = x^m - c (of deg m) <count> = " << count << " times." << endl;
        cout << "Measures how fast the optimized algorithm is relative to a naive one." << endl;
        return 1;
    }

    size_t n = static_cast<size_t>(std::stoi(argv[1]));
    size_t m = static_cast<size_t>(std::stoi(argv[2]));
    if(argc > 3)
        count = static_cast<size_t>(std::stoi(argv[3]));

    loginfo << "Dividing a (of deg n) by b (of deg m) " << count << " times..." << endl << endl;

    AveragingTimer 
        df("O(n) division      "),
        dn("O(n log n) division");
    for(size_t i = 0; i < count; i++) {

        loginfo << "n = " << n << ", m = " << m << endl;

        // Step 0: Pick random polynomials
        vector<Fr> a = random_field_elems(n+1);

        XncPoly b(m, Fr::random_element());
        //poly_print(a); std::cout << endl;

        // Step 1: Do our fast division
        vector<Fr> q, r, rhs;
        df.startLap();
        poly_divide_xnc(a, b, q, r);
        df.endLap();

        // INVARIANT: deg(r) < deg(b)
        testAssertStrictlyLessThan(r.size(), m + 1);
        if(a.size() >= b.n + 1) {
            testAssertEqual(q.size(), n - m + 1);
        }

        auto bf = b.toLibff();
        libfqfft::_polynomial_multiplication(rhs, bf, q);
        libfqfft::_polynomial_addition(rhs, rhs, r);

        //poly_print(a); std::cout << endl;
        //poly_print(rhs); std::cout << endl;
        testAssertEqual(a, rhs); 

        // Step 2: Do libntl division
        ZZ_pX za, zb, zq, zr;
        convLibffToNtl_slow(a, za);
        convLibffToNtl_slow(bf, zb);
        dn.startLap();
        NTL::DivRem(zq, zr, za, zb);
        dn.endLap();

        std::vector<Fr> rr, qq;
        convNtlToLibff(zr, rr);
        convNtlToLibff(zq, qq);

        testAssertEqual(rr, r);
        testAssertEqual(qq, q);
    }

    logperf << df << endl;
    logperf << dn << endl;

    return 0;
}
