#include <polycrypto/PolyOps.h>
#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>
#include <polycrypto/Utils.h>

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

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);

    if(argc < 4) {
        cout << "Usage: " << argv[0] << "<t> <n> <r>" << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        cout << "   <t>    the degree of the evaluated polynomial + 1" << endl;
        cout << "   <n>    the number of points to evaluate at" << endl;  
        cout << "   <r>    the number of times to repeat the eval" << endl;
        cout << endl;

        return 1;
    }
    
    // Evaluating f at all points in u.
    std::vector<Fr> f;
    std::vector<Fr> evals, actualEvals, fftEvals;

    double maxEvalTime = 3.0;
    bool skipEval = false;
    size_t t = static_cast<size_t>(std::stoi(argv[1]));
    size_t n = static_cast<size_t>(std::stoi(argv[2]));
    long r = std::stoi(argv[3]);

    logperf << "Degree = " << t - 1 << ", # points evaluated = " << n << ", iters = " << r << endl;
    
    f.resize(t);
    actualEvals.resize(n);
    fftEvals.resize(n);

    AveragingTimer at("Accum tree");
    at.startLap();
    AccumulatorTree accs(n);
    at.endLap();
    logperf << at << endl;

    std::vector<Fr> omegas = accs.getAllNthRootsOfUnity();
    assertEqual(omegas.size(), Utils::smallestPowerOfTwoAbove(n));
    omegas.resize(n);

    AveragingTimer c1("Fast eval "),
                   c2("FFT eval  "),
                   c3("Naive eval");

    logperf << endl;
    for(int rep = 0; rep < r; rep++) {
        f = random_field_elems(t);

        // Step 1: Fast multipoint eval
        c1.startLap();
        RootsOfUnityEvaluation eval(f, accs);
        evals = eval.getEvaluations();
        auto lastLap = c1.endLap();

        logperf << "Our eval (iter " << rep+1 << "): " << Utils::humanizeMicroseconds(lastLap) << endl;

        // Step 2: Direct FFT
        c2.startLap();
        poly_fft(f, n, fftEvals);
        c2.endLap();
        
        size_t vms, rss;
        getMemUsage(vms, rss);
        logperf << "VMS: " << Utils::humanizeBytes(vms) << endl;
        logperf << "RSS: " << Utils::humanizeBytes(rss) << endl;

        if(n > 4096 || skipEval)
            continue;

        // Step 3: Slow, libff evaluation
        c3.startLap();
        for(size_t i = 0; i < omegas.size(); i++) {
            //logdbg << "w_n^" << i << " = " << omegas[i] << endl;
            actualEvals[i] = libfqfft::evaluate_polynomial(f.size(), f, omegas[i]);
        }
        c3.endLap();

        assertEqual(evals.size(), actualEvals.size());
        for(size_t k = 0; k < evals.size(); k++) {
            if(evals[k] != actualEvals[k] || evals[k] != fftEvals[k]) {
                logerror << "The multipoint evaluation function returned a different evaluation at "
                    << k << "!" << endl;
                logerror << " * Eval   at w_n^" << k << " = " << evals[k] << endl;
                logerror << " * Actual at w_n^" << k << " = " << actualEvals[k] << endl;
                logerror << " * FFT    at w_n^" << k << " = " << fftEvals[k] << endl;

                throw std::runtime_error(
                        "The multipoint evaluation function is wrong!");
            }
        }
    }

    logperf << endl;
    logperf << c1 << endl;
    logperf << c2 << endl;

    if(c3.numIterations() > 0) {
        logperf	<< c3 << endl;

        if(static_cast<double>(c3.averageLapTime()) / 1000000.0 >= maxEvalTime) {
            logperf << "libff eval exceeded " << maxEvalTime << " seconds, skipping from now on..." << endl;
            skipEval = true;
        }
    }
    
    logperf << endl;

    return 0;
}
