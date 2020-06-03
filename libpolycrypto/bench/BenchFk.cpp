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
    
    if(argc < 5) {
        cout << "Usage: " << argv[0] << "<deg> <n> <r>" << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        cout << "   <pp-file>  the Kate public parameters file" << endl;
        cout << "   <deg>      the degree of the evaluated polynomial (i.e., t-1)" << endl;
        cout << "   <n>        the # of points to construct proofs for" << endl;  
        cout << "   <r>        the # of times to repeat the benchmark" << endl;
        cout << endl;

        return 1;
    }
    
    std::string ppFile = argv[1];
    size_t deg = static_cast<size_t>(std::stoi(argv[2]));
    size_t n = static_cast<size_t>(std::stoi(argv[3]));
    size_t r = static_cast<size_t>(std::stoi(argv[4]));
    size_t N = Utils::smallestPowerOfTwoAbove(n);

    // TODO: what happens if deg > n?

    std::unique_ptr<Dkg::KatePublicParameters> kpp(
        new Dkg::KatePublicParameters(ppFile, deg));

    loginfo << endl;
    loginfo << "Degree " << deg << " poly, evaluated at n = " << n << " points, iters = " << r << endl;

    std::vector<Fr> f = random_field_elems(deg + 1);

    AveragingTimer ht("Computing h[i]'s");
    std::chrono::microseconds::rep mus;
    std::vector<G1> H;
    for(size_t i = 0; i < r; i++) {
        ht.startLap();
        H = kpp->computeAllHis(f);
        mus = ht.endLap();
        
        logperf << " - Computing h[i]'s (iter " << i << "): " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    // resize H to size n, since the degree of f < n
    H.resize(N, G1::zero());
    
    AveragingTimer ft("FFT on h[i]'s");
    for(size_t i = 0; i < r; i++) {
        ft.startLap();
        FFT<G1, Fr>(H);
        mus = ft.endLap();
        
        logperf << " - FFT on h[i]'s (iter " << i << "): " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    logperf << endl;
    logperf << ht << endl;
    logperf << ft << endl;
    logperf << endl;

    loginfo << "Exited successfully!" << endl;

    return 0;
}
