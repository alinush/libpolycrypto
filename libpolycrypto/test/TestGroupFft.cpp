#include <polycrypto/Configuration.h>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/PolyOps.h>
#include <polycrypto/Utils.h>
#include <polycrypto/KZG.h>
#include <polycrypto/FFT.h>
#include <polycrypto/KatePublicParameters.h>

#include <cmath>
#include <ctime>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <xassert/XAssert.h>
#include <xutils/Log.h>
#include <xutils/Utils.h>
#include <xutils/NotImplementedException.h>
#include <xutils/Timer.h>

#include <libff/algebra/fields/field_utils.hpp>

using namespace std;
using namespace libfqfft;
using namespace libpolycrypto;

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);
    srand(static_cast<unsigned int>(time(NULL)));

    if(argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        cout << "Usage: " << argv[0] << "[<n>] [<deg>] [<public-params-file>]" << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        cout << "   <n>    the # of points to compute proofs for" << endl;  
        cout << "   <deg>  the degree of the polynomial" << endl;  
        cout << "   <public-params-file>  the public params file to use; if not provided, will generate random params" << endl;
        cout << endl;

        return 1;
    }
    
    size_t n = argc > 1 ? static_cast<size_t>(std::stoi(argv[1])) : 32;
    size_t N = Utils::smallestPowerOfTwoAbove(n);

    size_t deg = argc > 2 ? static_cast<size_t>(std::stoi(argv[2])) : n / 2;
    bool generateParams = argc <= 3;

    std::unique_ptr<KatePublicParameters> kpp;
    if(generateParams)
        kpp.reset(new KatePublicParameters(deg));
    else
        kpp.reset(new KatePublicParameters(argv[3], deg));

    loginfo << "Picking random, degree " << deg << " polynomial... ";
    std::vector<Fr> f = random_field_elems(deg + 1);
    std::cout << " done." << endl;
    
    loginfo << "Evaluating polynomial using FFT... ";
    std::vector<Fr> y;
    poly_fft(f, N, y);
    std::cout << " done." << endl;

    loginfo << "Committing to polynomial...";
    G1 c = multiExp<G1>(
        kpp->g1si.begin(),
        kpp->g1si.begin() + static_cast<long>(f.size()),
        f.begin(),
        f.end());
    std::cout << " done." << endl;

    //loginfo << "Computing " << deg << " h_i's in G1 (using trapdoor)... " << endl;
    //std::vector<G1> h = kpp->computeAllHisWithTrapdoor(f);
    ////for(size_t i = 0; i < h.size(); i++) {
    ////    logdbg << "h[i]:  " << h[i] << endl;
    ////}
    //loginfo << " ...done!" << endl;
    
    loginfo << "Computing " << deg << " h_i's in G1 normally... " << endl;
    std::vector<G1> H;
    {
        ScopedTimer<std::chrono::milliseconds> t(std::cout, "DFT on group elems time: ", " ms\n");
        H = kpp->computeAllHis(f);
    }
    for(size_t i = 0; i < H.size(); i++) {
        logdbg << "H[" << i << "]:  " << H[i] << endl;
        //testAssertEqual(h[i], H[i]);
    }
    loginfo << " ...done!" << endl;

    loginfo << "Computing proofs fast via FFT... " << endl;
    // resize H to size N, since the degree of f < N
    H.resize(N, G1::zero());
    //H[0]=H[N+2]; // ensure DEBUG mode actually catches this out-of-bounds error
    {
        ScopedTimer<std::chrono::milliseconds> t(std::cout, "DFT on group elems time: ", " ms\n");
        FFT<G1, Fr>(H);
    }
    for(size_t i = 0; i < n; i++) {
        logdbg << "proof[" << i << "]:  " << H[i] << endl;
    }
    loginfo << " ...done!" << endl;

    loginfo << "Verifying proofs..." << endl;
    Fr omega = libff::get_root_of_unity<Fr>(N);
    Fr w = Fr::one();
    for(size_t i = 0; i < n; i++) {
        if(KZG::verifyProof(c, H[i], y[i], w, kpp->g2si[1]) == false) {
            logerror << "The proof for w^" << i << " did NOT verify." << endl;
            auto q = std::get<0>(KZG::naiveProve(*kpp, f, w));
            logerror << " \\-> Real proof: " << q << endl;
            logerror << " \\-> Our proof:  " << H[i] << endl;
            return 1;
        }

        w *= omega; 
    }
    loginfo << " ...done!" << endl;
    
    loginfo << "Exited succsessfully!" << endl;

    return 0;
}
