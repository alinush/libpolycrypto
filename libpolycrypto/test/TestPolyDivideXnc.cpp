#include <polycrypto/Configuration.h>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/PolyOps.h>

#include <xassert/XAssert.h>
#include <xutils/Log.h>

using namespace libpolycrypto;

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    libpolycrypto::initialize(nullptr, 0);

    size_t count = 100;
    if(argc > 1 && strcmp(argv[1], "-h") == 0) {
        cout << "Usage: " << argv[0] << " <n> <m>" << endl;
        cout << endl;
        cout << "Divides random poly a(x) (of deg n) by b(x) = x^m - c (of deg m), doubles their degrees and repeats " << count << " times." << endl;
        return 1;
    }

    size_t a_deg = argc < 3 ? 8 : static_cast<size_t>(std::stoi(argv[1]));
    size_t b_deg = argc < 3 ? 4 : static_cast<size_t>(std::stoi(argv[2]));

    loginfo << "Dividing a (of deg n) by b (of deg m)..." << endl << endl;

    for(size_t i = 0; i < count; i++) {
        size_t n = a_deg*(i+1); // degree of a
        size_t m = b_deg*(i+1); // degree of b

        loginfo << "n = " << n << ", m = " << m << endl;

        vector<Fr> a = random_field_elems(n+1);

        XncPoly b(m, Fr::random_element());
        //poly_print(a); std::cout << endl;

        vector<Fr> q, r, rhs;
        poly_divide_xnc(a, b, q, r);

        // INVARIANT: deg(r) < deg(b)
        testAssertStrictlyLessThan(r.size(), m + 1);
        if(a.size() >= b.n + 1) {
            testAssertEqual(q.size(), n - m + 1);
        }

        libfqfft::_polynomial_multiplication(rhs, b.toLibff(), q);
        libfqfft::_polynomial_addition(rhs, rhs, r);

        //poly_print(a); std::cout << endl;
        //poly_print(rhs); std::cout << endl;
        testAssertEqual(a, rhs);
    }

    return 0;
}
