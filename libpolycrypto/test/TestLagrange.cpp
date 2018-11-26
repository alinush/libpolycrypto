#include <polycrypto/PolyCrypto.h>

#include <polycrypto/RootsOfUnityEval.h>
#include <polycrypto/Lagrange.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <xutils/Log.h>
#include <xassert/XAssert.h>
#include <xutils/Timer.h>

using namespace libpolycrypto;
using namespace std;
using namespace libfqfft;

using libpolycrypto::Fr;

int main() {
    libpolycrypto::initialize(nullptr, 0);
    srand(static_cast<unsigned int>(time(NULL)));

    for (size_t f = 2; f < 1024*8; f *= 2) {
        // get all roots of unity w_n^k, for k = 0, ..., n-1, where n = 2f+1
        size_t n = 2*f + 1;
        size_t N = Utils::smallestPowerOfTwoAbove(n);
        std::vector<Fr> omegas = get_all_roots_of_unity(N);
        testAssertEqual(omegas.size(), N);

        loginfo << f + 1 << " out of " << n << endl;

        // pick random subset of f+1 w_n^k's
        std::vector<Fr> someOmegas;
        std::vector<size_t> idxs;
        Utils::randomSubset<size_t>(idxs, static_cast<int>(n), static_cast<int>(f + 1));
        for(auto i : idxs) {
            someOmegas.push_back(omegas[i]);
        }
        testAssertEqual(someOmegas.size(), f + 1);

        // computes lagrange coefficients L_i(0) w.r.t. to the points in 'someOmegas'
        std::vector<Fr> L, L_naive;
        lagrange_coefficients(L, omegas, someOmegas, idxs);
        lagrange_coefficients_naive(L_naive, omegas, someOmegas, idxs);

        testAssertEqual(L, L_naive);

        //logdbg << "L = " << endl << L << endl;
        //logdbg << endl;
        //logdbg << "L_naive = " << endl << L << endl;
    }

    loginfo << "Test ended successfully!" << endl;
    return 0;
}
