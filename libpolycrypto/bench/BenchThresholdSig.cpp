#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>
#include <polycrypto/Lagrange.h>
#include <polycrypto/Utils.h>
#include <polycrypto/FFThresh.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <xutils/Log.h>
#include <xutils/Utils.h>
#include <xassert/XAssert.h>
#include <xutils/Timer.h>

using namespace libpolycrypto;
using namespace std;
using namespace libfqfft;

void printUsage(const char * prog) {
    cout << "Usage: " << prog << " <efficient> <min-f> <max-f> <out-file>" << endl
         << endl
         << "Benchmarks f+1 out of 2*f + 1 threshold signature schemes (naive or FFT-based) with f starting at <min-f> going all the way up to <max-f>" << endl
         << endl
         << "   <efficient>     1 for efficient Lagrange, 0 for naive" << endl
         << "   <min-f>         starting value of f" << endl
         << "   <max-f>         ending value of f" << endl
         << "   <num-samples>   how many times to measure Lagrange + multiexp?" << endl
         << "   <out-file>      ouput file to write results in" << endl
         << endl;
}

int main(int argc, char * argv[]) {
    libpolycrypto::initialize(nullptr, 0);

    srand(static_cast<unsigned int>(time(NULL)));

    if (argc < 6) {
        printUsage(argv[0]);
        return 1;
    }

    bool naive = static_cast<size_t>(std::stoi(argv[1])) == 0;
    size_t minf = static_cast<size_t>(std::stoi(argv[2]));
    size_t maxf = static_cast<size_t>(std::stoi(argv[3]));
    int numSamples = std::stoi(argv[4]);
    std::string fileName = argv[5];

    ofstream fout(fileName);

    if(fout.fail()) {
        throw std::runtime_error("Could not open " + fileName + " for writing");
    }

    fout << "k,n,interpolation_method,num_samples,lagr_usec,multiexp_usec,total_usec,lagr_hum,multiexp_hum,total_hum,date" << endl;

    loginfo << endl;
    loginfo << "Fast FFT-based Lagrange? "         << (naive ? "No." : "Yes.") << endl;
    loginfo << endl;

    std::vector<size_t> fs;
    loginfo << "Benchmarking thresholds: " << endl;
    for(size_t p = Utils::smallestPowerOfTwoAbove(minf); p <= maxf + 1; p *= 2) {
        size_t f = p - 1;
        size_t k = f + 1;
        size_t n = 2*f + 1;
        loginfo << " * " << k << " out of " << n  << " threshold sigs" << endl;
        fs.push_back(f);
    }
    loginfo << endl;
    
    for(auto f : fs) {
        size_t k = f + 1;
        size_t n = 2 * f + 1;

        loginfo << "k = " << k << ", n = " << n << "." << endl;

        // can't hash in libff, so we're just picking a random group element to sign
        G1 hash = G1::random_element();

        vector<Fr> sk;
        G2 pk;

        {
        loginfo << std::flush;
        ScopedTimer<std::chrono::seconds> t(std::cout, "Generating keys took: ", " seconds\n");
        // generate public key pk and signer keys s
        generate_keys(n, k, pk, sk, nullptr);
        }

        vector<G1> allSigShares(n);
        {
        loginfo << std::flush;
        ScopedTimer<std::chrono::seconds> t(std::cout, "Signing took: ", " seconds\n");
        // sigShare[i] = H(m)^s_i
        for (size_t i = 0; i < n; i++) {
           allSigShares[i] = shareSign(sk[i], hash);
        }
        }

        AveragingTimer tl("Lagrange");
        AveragingTimer tm("Multiexp");
        vector<Fr> lagr;
        size_t N = Utils::smallestPowerOfTwoAbove(n);
        std::vector<Fr> omegas = get_all_roots_of_unity(N);
        //logdbg << "omegas.size() = " << omegas.size() << endl;

        for(int sample = 0; sample < numSamples; sample++) {
            loginfo << "Taking measurement #" << sample + 1 << endl;
            // pick random subset of valid signers
            vector<size_t> signers = random_subset(k, n);
            vector<Fr> someOmegas;
            for(auto id : signers) {
                assertStrictlyLessThan(id, omegas.size());
                someOmegas.push_back(omegas[id]);
            }

            // sigSharesSubset[i] stores the share for signers[i]
            vector<G1> sigSharesSubset = align_shares(allSigShares, signers);

            // computes lagrange coefficients L_i
            tl.startLap();
            if(naive) {
                lagrange_coefficients_naive(lagr, omegas, someOmegas, signers);
            } else {
                lagrange_coefficients(lagr, omegas, someOmegas, signers);
            }
            tl.endLap();

            // signature = \prod_i ( H(m)^(s_i) )^L_i =  H(m)^( \sum_i(L_i * s_i) ) = H(m)^p(0) = H(m)^s
            tm.startLap();
            G1 signature = aggregate(sigSharesSubset, lagr);
            tm.endLap();

            // verify e(H(m)^s, g2) = e(H(m), g2^s)
            testAssertEqual(ReducedPairing(signature, G2::one()), ReducedPairing(hash, pk));
        }

        auto lagr_usec = tl.averageLapTime();
        auto multiexp_usec = tm.averageLapTime();
        auto total_usec = lagr_usec + multiexp_usec;
        logperf << " * " << tl << endl;
        logperf << " * " << tm << endl; 
        logperf << " * Lag+mexp: " << Utils::humanizeMicroseconds(total_usec) << endl;
        logperf << endl;

        fout << k << "," 
             << n << ","
             << (naive == true ? "naive-lagr-wnk" : "fft-eval") << ","
             << numSamples << ","
             << lagr_usec << ","
             << multiexp_usec << ","
             << total_usec << ","
             << Utils::humanizeMicroseconds(lagr_usec, 2) << ","
             << Utils::humanizeMicroseconds(multiexp_usec, 2) << ","
             << Utils::humanizeMicroseconds(total_usec, 2) << ","
             << timeToString()
             << endl;
    }

    loginfo << "Benchmark ended successfully!" << endl;
    return 0;
}
