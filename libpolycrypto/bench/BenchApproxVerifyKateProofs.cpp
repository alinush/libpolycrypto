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
         << "Benchmarks computing poly evaluation proofs for k=f+1 out of n=2f+1 VSS/DKG schemes, with f starting at <min-f> doubling all the way up to <max-f>" << endl
         << endl
         << "   <efficient>     1 for efficient AMT, 0 for Kate et al" << endl
         << "   <min-f>         starting value of f" << endl
         << "   <max-f>         ending value of f" << endl
         << "   <out-file>      output file to write results in" << endl
         << endl;
}

int main(int argc, char * argv[]) {
    libpolycrypto::initialize(nullptr, 0);

    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }

    bool naive = static_cast<size_t>(std::stoi(argv[1])) == 0;
    int minf = std::stoi(argv[2]);
    int maxf = std::stoi(argv[3]);
    std::string fileName = argv[4];
    std::string scheme = naive ? "kate" : "amt";

    ofstream fout(fileName);

    if(fout.fail()) {
        throw std::runtime_error("Could not open " + fileName + " for writing");
    }

    fout << "k,"
        << "n,"
        << "scheme,"
        << "verify_single_usec,"        // the time to verify a proof
        << "verify_all_usec,"           // the time to verify all proofs against the same poly commitment
        << endl;

    loginfo << endl;
    loginfo << "Benchmarking " << (naive ? "Kate et al" : "AMT") << endl;
    loginfo << endl;

    const size_t maxPairingIters = 100;
    const size_t maxExpIters = 1000;
    //const size_t maxMulIters = 10000;

    AveragingTimer tp ("Pairing         ");
    AveragingTimer te ("GT exp          ");
    AveragingTimer te1("G1 exp          ");
    AveragingTimer tm ("GT mul          ");
    //                 "Pairing + G1 exp"

    logperf << "Benchmarking pairing..." << endl;
    for(size_t i = 0; i < maxPairingIters; i++) {
        G1 a = G1::random_element();
        G2 b = G2::random_element();

        tp.startLap();
        ReducedPairing(a, b);
        tp.endLap();

        if((i + 1) % (maxPairingIters / 10) == 0) {
            logperf << tp << endl;
        }
    }

    logperf << endl;
    logperf << "Benchmark G1 and GT exponentiation..." << endl;
    GT a = ReducedPairing(G1::one(), G2::one());
    G1 g = G1::one();
    std::vector<GT> randomElems;
    GT rt;
    G1 r1;
    for(size_t i = 0; i < maxExpIters; i++) {
        Fr e = Fr::random_element();

        te.startLap();
        rt = a^e;
        te.endLap();

        randomElems.push_back(rt);

        te1.startLap();
        r1 = e * g;
        te1.endLap();
        (void)r1;

        if((i + 1) % (maxExpIters / 10) == 0) {
            logperf << te << endl;
            logperf << te1 << endl;
        }
    }

    logperf << endl;
    logperf << "Benchmark GT multiplication..." << endl;
    for(size_t i = 0; i < randomElems.size() - 1; i++) {

        tm.startLap();
        GT c = randomElems[i]*randomElems[i+1];
        (void)c;
        tm.endLap();
    
        if((i + 1) % (randomElems.size() / 10) == 0) {
            logperf << tm << endl;
        }
    }

    auto pairing_usec = tp.averageLapTime();
    auto gt_exp_usec = te.averageLapTime();
    auto gt_mul_usec = tm.averageLapTime();
    auto g1_exp_usec = te1.averageLapTime();

    // true if we should use the pairing to compute e(g,g)^p(i)
    auto g1_exp_and_pairing_usec = pairing_usec + g1_exp_usec;
    bool usePairing = gt_exp_usec > g1_exp_and_pairing_usec;
    logperf << endl;
    logperf << tp << endl;
    logperf << te1 << endl;
    logperf << te << endl;
    logperf << "Pairing + G1 exp: " << g1_exp_and_pairing_usec << " microsecs" << endl;
    logperf << " * Faster than GT exp? " << (usePairing ? "Yes." : "No") << endl;
    logperf << tm << endl;
    logperf << endl;

    if(usePairing) {
        logperf << "Using pairing + G1 exp to compute e(g^p(i), g)" << endl;
    } else {
        logperf << "Using GT exp to compute e(g,g)^p(i)" << endl;
    }
    logperf << endl;

    for(int p = Utils::smallestPowerOfTwoAbove(minf); p <= maxf + 1; p *= 2) {
        size_t f = static_cast<size_t>(p - 1);
        size_t k = f + 1;
        size_t n = 2 * f + 1;

        loginfo << "k = " << k << ", n =  " << n << "." << endl;
        loginfo << "log2(n) = log2(" << n << ") = " << Utils::log2ceil(n) << endl;

        //loginfo << " * Done!" << endl;

        auto nn = static_cast<microseconds::rep>(n);
        //size_t proofSize = Utils::log2ceil(n) + 1;    the old (unoptimized) proof size
        size_t proofSize = Utils::log2floor(f) + 1;
        auto proofSizeCast = static_cast<microseconds::rep>(proofSize);

        microseconds::rep all_verify_usec_usec = pairing_usec;
        loginfo << endl;
        loginfo << "For verifying all proofs, need to compute: " << endl;
        loginfo << " - 1 pairing for e(g,g)^p(\\tau)" << endl;

        if(usePairing) {
            all_verify_usec_usec += nn * (pairing_usec + g1_exp_usec);
            loginfo << " - n = " << n << " pairings + G1 exp's";
        } else {
            all_verify_usec_usec += nn * gt_exp_usec;
            loginfo << " - n = " << n << " GT exp's";
        }

        std::cout << " for e(g,g)^p(i) LHS" << endl;

        loginfo << " - n = " << n << " GT muls for e(g,g)^p(\\tau) / e(g,g)^p(i)" << endl;
        all_verify_usec_usec += nn * gt_mul_usec;

        if(naive) {
            // in eVSS, to verify \pi_i for p(i), can do e(\pi_i, g^\tau / g^i) e(g,g)^p(i) = e(g,g)^p(\tau)
            // so need n pairings
            all_verify_usec_usec += nn * pairing_usec;
            loginfo << " - n = " << n << " pairings for Kate's e(\\pi_i, g^(\\tau-i)) RHS" << endl;
        } else {
            // in AMT VSS, to verify \pi_i for p(i), must do e(g,g)^p(\tau) = e(g,g)^p(i) \prod_{w\in path(i)} e(g^{q_w(\tau)}, g^{a_w(\tau)})
            // so need at most n*(\log{n} + 1) pairings
            // (in practice less because correct proofs will have same quotients in some nodes, and reconstructor can tell not to redo those pairings)
            // (the least could be 2n-1 pairings if all proofs are valid)
            all_verify_usec_usec += nn * proofSizeCast * (pairing_usec + gt_mul_usec);
            loginfo << " - n * (log2(f) + 1) = " << n << " * log2(" << f << ") + 1 = " << n * proofSize << " pairings for the AMT RHS" << endl;
            loginfo << " - n * log2(f)= " << n << " * log2(" << f << ") = " << n * (proofSize - 1) << " GT muls for the AMT RHS" << endl;
        }

        logperf << endl;
        logperf << " * Verify all proofs during reconstruction: " << Utils::humanizeMicroseconds(all_verify_usec_usec) << endl;
        
        microseconds::rep single_verify_usec = g1_exp_usec + pairing_usec;
        loginfo << endl;
        loginfo << "For a player to verify a proof, need to compute: " << endl;
        loginfo << " - 1 G1 exp for g^p(i)" << endl;
        loginfo << " - 1 pairing for e(g^p(\\tau) / g^p(i), g)" << endl;

        if(naive) {
            // in eVSS, to verify \pi_i for p(i), can do e(\pi_i, g^\tau / g^i) = e(g,g)^{p(\tau)-p(i)}
            single_verify_usec += pairing_usec;
            loginfo << " - 1 pairing for Kate's e(\\pi_i, g^(\\tau-i)) RHS" << endl;
        } else {
            // in AMT VSS, to verify \pi_i for p(i), must do e(g^p(\tau) / g^p(i), g) = \prod_{w\in path(i)} e(g^{q_w(\tau)}, g^{a_w(\tau)})
            single_verify_usec += proofSizeCast * (pairing_usec + gt_mul_usec);
            loginfo << " - log2(f) + 1 = log2(" << f << ") + 1 = " << proofSize << " pairings to verify the AMT RHS" << endl;
            loginfo << " - log2(f) = log2(" << f << ") = " << proofSize - 1 << " GT muls for the AMT RHS" << endl;
        }

        logperf << endl;
        logperf << " * Verify single proof: " << Utils::humanizeMicroseconds(single_verify_usec) << endl;

        logperf << endl;

        fout << k << "," 
             << n << ","
             << scheme << ","
             << single_verify_usec << ","
             << all_verify_usec_usec << ","
             << endl;
    }

    loginfo << "Benchmark ended successfully!" << endl;
    return 0;
}
