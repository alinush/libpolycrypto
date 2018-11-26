#include <polycrypto/PolyOps.h>
#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>
#include <polycrypto/Utils.h>
#include <polycrypto/DkgCommon.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

using namespace std;
using namespace libfqfft;
using namespace libpolycrypto;
using Dkg::DkgParams;

static size_t pow2(size_t exp) {
    return 1u << exp;
}

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);
    srand(static_cast<unsigned int>(time(NULL)));

    if(argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        //cout << "Usage: " << argv[0] << "[<maxT> <maxN>] [r]" << endl;
        cout << "Usage: " << argv[0] << "[<maxN>]" << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        //cout << "   <maxT>    the degree of the evaluated polynomial + 1" << endl;
        cout << "   <maxN>    the # of points to evaluate at (i.e., # of AMT leaves)" << endl;  
        //cout << "   <r>       the # of times to repeat a test for each (t,n) threshold " << endl;
        cout << endl;

        return 1;
    }
    
    size_t maxN = argc > 2 ? static_cast<size_t>(std::stoi(argv[1])) : 32;
    //size_t r    = argc > 3 ? static_cast<size_t>(std::stoi(argv[2])) : 100;

    size_t maxT = maxN - 1; 
    size_t maxDeg = maxT - 1;

    auto kpp = Dkg::KatePublicParameters::getRandom(maxDeg);

    // NOTE: This code shows that AccumulatorTree's are subsets of one another.
    // Just keep in mind that w_{2N}^{2k} = w_N^k.
    //for(size_t i = 2; i <= 16; i *= 2)
    //{
    //    AccumulatorTree accs(i);
    //    cout << "Size: " << i << endl << accs.toString() << endl << endl;
    //}

    // Testing that a_4 is a subset of a_8!
    AccumulatorTree a4(4);
    AuthAccumulatorTree aa4(a4, kpp, 5);
    AccumulatorTree a8(8);
    AuthAccumulatorTree aa8(a8, kpp, 9);

    for(size_t level = 0; level < 3; level++) {
        for(size_t idx = 0; idx < pow2(2 - level); idx++) {
            //logdbg << level << ", " << idx << endl;
			testAssertEqual(a4.tree[level][idx], a8.tree[level][idx]);
			testAssertEqual(aa4.tree[level][idx], aa8.tree[level][idx]);
		}
    }

    // Authenticate AccumulatorTree
    AccumulatorTree maxAccs(maxN);
    AuthAccumulatorTree authAccs(maxAccs, kpp, maxT);
    authAccs._assertValid();

    //for(size_t i = 0; i < r; i++) {
    loginfo << "Testing thresholds: " << std::flush;
        for(size_t n = 3; n <= maxN; n++) {
            for(size_t t = 2; t < n; t++) {
                cout << "(" << t << ", " << n << ") " << std::flush;

                std::vector<Fr> f = random_field_elems(t);

                // Step 1: Fast multipoint eval
                DkgParams params(t, n, true);
                params.setAuthAccumulators(&authAccs);
                RootsOfUnityEvaluation eval(f, *params.accs);

                // Step 2: Authenticate Multipoint Evaluation
                //loginfo << "Computing AMT: ";
                for(bool isSimulated : { true, false }) {
                    AuthRootsOfUnityEvaluation authEval(eval, kpp, isSimulated);

                    // Verify all monomial commitments
                    for(size_t i = 0; i < n; i++) {
                        const auto& vk = params.getMonomialCommitment(i);
                        //loginfo << "vk[" << i << "]: " << vk << endl;

                        testAssertEqual(
                            kpp.getG2toS() - params.omegas[i] * G2::one(), 
                            vk);
                    }
                }

                // Step 3: Verify all proofs
            }
        }
    //}
    cout << endl;

    // TODO: Step 4: Verify AMT proofs
    
    loginfo << "Exited succsessfully!" << endl;

    return 0;
}
