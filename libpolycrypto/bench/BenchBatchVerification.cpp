#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>
#include <polycrypto/Lagrange.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <libff/algebra/scalar_multiplication/multiexp.hpp>

#include <xutils/Log.h>
#include <xutils/Utils.h>
#include <xassert/XAssert.h>
#include <xutils/Timer.h>

using namespace libpolycrypto;
using namespace std;
using namespace libfqfft;

int main() {

    libpolycrypto::initialize(nullptr, 0);

    size_t f = 100000;
    for (size_t invalidPercent = 0; invalidPercent <= 10; invalidPercent += 1) {
        size_t k, n;
        k = f, n = 2 * f;

        ManualTimer t1, t2;

        loginfo << "k = " << k << ", n =  " << n << ", %invalid = " << invalidPercent << "%." << endl;

        G1 H = G1::random_element();

        vector<Fr> s, L;
        vector<G2> pkSigners;
        G2 pk;

        // generate public key pk and signer keys s
        t2.restart();
        generate_keys(n, k, pk, s, &pkSigners);
        string gen_keys_usec = to_string(t2.restart().count());
        //string gen_keys_usec = "N/A";


        vector<size_t> randset = random_subset(n, (100 - invalidPercent) * n / 100);
        // sigShare[i] = H(m)^s_i
        vector<G1> sigShare(n);
        t2.restart();
        for (size_t i : randset) {
            sigShare[i] = shareSign(s[i], H);
        }
        string share_sign_total_usec = to_string(t2.restart().count());

        // verify e(H(m)^s_i, g2) = e(H(m), g2^s_i)
        // naively verify one by one:
        t2.restart();
        for (size_t i = 0; i < n; i++) {
            if (ReducedPairing(sigShare[i], G2::one()) == ReducedPairing(H, pkSigners[i])) {

            }
        }
        string naive_verify_usec = to_string(t2.restart().count());

        t2.restart();
        // batch verification:
        vector<bool> isValidShare = batch_ver(sigShare, pkSigners, H);
        string batch_verify_usec = to_string(t2.restart().count());

        logperf << "naive_verify_usec: "     << naive_verify_usec << endl;
        logperf << "batch_verify_usec: "     << batch_verify_usec << endl << endl;
    }
    return 0;
}
