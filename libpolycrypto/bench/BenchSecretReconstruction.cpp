#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>
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
         << "Benchmarks k=f+1 out of n=2f+1 secret reconstruction in our VSS/DKG scheme, with f starting at <min-f> doubling all the way up to <max-f>" << endl
         << "Only focuses on combining Lagr and shares." << endl
         << "Ignores Lagrange interpolation time, which we can extract from the threshold signature experiments." << endl
         << "Ignores share verification time, which we can extract from VSS/DKG experiments." << endl
         << endl
         << "   <min-f>         starting value of f" << endl
         << "   <max-f>         ending value of f" << endl
         << "   <num-samples>   how many times to measure Lagrange of size k and 2n + 1 pairings" << endl
         << "   <out-file>      output file to write results in" << endl
         << endl;
}

int main(int argc, char * argv[]) {
    libpolycrypto::initialize(nullptr, 0);

    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }

    int minf = std::stoi(argv[1]);
    int maxf = std::stoi(argv[2]);
    int numSamples = std::stoi(argv[3]);
    std::string fileName = argv[4];

    ofstream fout(fileName);

    if(fout.fail()) {
        throw std::runtime_error("Could not open " + fileName + " for writing");
    }

    fout << "k,"
        << "n,"
        << "num_samples,"
        << "combine_usec,"                  // the time to compute s = \sum_i \Ell_i(0) s_i
        << endl;

    std::vector<Fr> shares;
    vector<Fr> lagr;    // Lagrange coeffs
    for(int p = Utils::smallestPowerOfTwoAbove(minf); p <= maxf + 1; p *= 2) {
        size_t f = static_cast<size_t>(p - 1);
        size_t k = f + 1;
        size_t n = 2 * f + 1;

        loginfo << "k = " << k << ", n =  " << n << "." << endl;

        loginfo << "Picking " << k - shares.size() << " random shares and Lagrange coeffs..." << endl;
        while(shares.size() < k) {
            shares.push_back(Fr::random_element());
            lagr.push_back(Fr::random_element());
        }
        //loginfo << " * Done!" << endl;

        AveragingTimer tc("Combine");
        loginfo << "Taking " << numSamples << " measurements..." << endl;
        for(int sample = 0; sample < numSamples; sample++) {

            testAssertEqual(lagr.size(), shares.size());

            // Bench: recover secret as \sum_i s_i * L_i(0) = p(0) = s
            tc.startLap();
            Fr s = Fr::zero();
            for(size_t i = 0; i < k; i++) {
                s += shares[i] * lagr[i];
            }
            tc.endLap();
        }

        logperf << " * " << tc << endl;

        auto combine_usec = tc.averageLapTime();

        fout << k << "," 
             << n << ","
             << numSamples << ","
             << combine_usec << ","
             << endl;
    }

    loginfo << "Benchmark ended successfully!" << endl;
    return 0;
}
