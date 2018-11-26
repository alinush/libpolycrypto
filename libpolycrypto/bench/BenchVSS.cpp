#include <polycrypto/PolyOps.h>
#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>
#include <polycrypto/Dkg.h>
#include <polycrypto/Lagrange.h>
#include <polycrypto/Utils.h>
#include <polycrypto/FFThresh.h> // for random_subset()

#include <algorithm>
#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

#include "Utils.h"

using namespace std;

#define BEST_CASE_RECONSTR
#define WORST_CASE_RECONSTR

void benchmarkVss(
    size_t f,
    size_t numDealIters,
    size_t numVerIters,
    size_t numReconstrIters,
    const std::string& vssType,
    const Dkg::KatePublicParameters* kpp,
    const AuthAccumulatorTree* authAccs,
    std::ofstream& fout)
{
    Dkg::DkgParams params(f+1, 2*f+1, vssType != "feld");
    testAssertStrictlyGreaterThan(params.n, 1);

    std::unique_ptr<Dkg::FeldmanPublicParameters> fpp;
    if(needsFeldmanPublicParams({ vssType })) 
        fpp.reset(new Dkg::FeldmanPublicParameters(params));
    if(needsKatePublicParams({ vssType }))
        params.setAuthAccumulators(authAccs);

    std::vector<Dkg::AbstractPlayer*> players;
    
    loginfo << "Benchmarking " << f + 1 << " out of " << 2*f+1 << " '" << vssType << "' VSS ..." << endl;

    // we restrict the # of players we allocate to save memory
    size_t numReal = std::min(numDealIters, params.n);
    bool isSimulated = boost::contains(vssType, "-sim");

    // we create numReal <= numDealIters players, which we use both as dealers and verifiers
    loginfo << " * Creating " << numReal << " players..." << endl;
    for(size_t i = 0; i < numReal; i++) {
        // WARNING: We set the active players to be those with larger IDs, because otherwise Feldman share verification
        // is much faster for ID w_N^0 = 1, since they have to compute \sum_{i=0}^{t-1} c_i^{1^i}, so no exponentiations will happen
        players.push_back(createPlayer(vssType, params, (params.n - 1) - i, kpp, fpp.get(), isSimulated, false));
    }

    AveragingTimer td("deal");
    loginfo << " * Dealing " << numDealIters << " time(s): ";
    for(size_t i = 0; i < numDealIters; i++) {
        td.startLap();
        players[i % numReal]->deal();
        td.endLap();

        std::cout << i + 1 << ", " << std::flush;
    }
    std::cout << "done!" << endl;

    // WARNING: Going through multiple dealers in this fashion will give us a better average, rather than verifying the same
    // dealer multiple times.
    AveragingTimer tv("verify");
    loginfo << " * Verifying dealer(s) (" << numVerIters << " times): ";
    size_t iters = 0;
    while(iters < numVerIters) {
        for(size_t d = 0; d < numReal && iters < numVerIters; d++) {
            auto dealer = players[d];

            for(size_t i = 0; i < numReal && iters < numVerIters; i++) {
                auto p = players[i];

                tv.startLap();
                testAssertTrue(p->verifyOtherPlayers({ dealer }, false));
                tv.endLap();

                std::cout << i << " (p" << p->id << " - d" << dealer->id << "), " << std::flush;
                iters++;
            }
        }
    }
    std::cout << endl;
    
    // benchmark reconstruction
    vector<size_t> subset;
    AveragingTimer trBC("Reconstr. best-case");
    AveragingTimer trWC("Reconstr. worst-case");

    // TODO: should move this into a function, but be careful with verifying the secret
    // because in VSS there's no verification, whereas in DKG, you check if it matches the PK in the fast track
#if defined(WORST_CASE_RECONSTR) || defined(BEST_CASE_RECONSTR)
    loginfo << " * Reconstructing (" << numReconstrIters << " times): ";

    for(size_t i = 0; i < numReconstrIters; i++) {
        auto reconstructor = players[i % numReal];
        assertNotNull(reconstructor);
        Fr secret, actualSecret = reconstructor->getSecret();

# ifdef WORST_CASE_RECONSTR
        // worst case: you have to verify n shares before you find a subset of t correct ones
        subset = random_subset(params.t, params.n);
        std::sort(subset.begin(), subset.end()); // for AMT's reconstructVerify

        trWC.startLap();
        testAssertTrue(reconstructor->reconstructionVerify(subset, false));
        secret = reconstructor->interpolate(subset);
        trWC.endLap();

        // NOTE: since this is the worst case, the computed secret is guaranteed to be correctly reconstructed
        testAssertEqual(secret, actualSecret);

        std::cout << i << " (d" << reconstructor->id << ")" << std::flush;
# endif

# ifdef BEST_CASE_RECONSTR
        // best case: you get all correct shares (you don't know it, but you find out as you verify)
        subset = random_subset(params.t, params.n);
        std::sort(subset.begin(), subset.end()); // for AMT's reconstructVerify

        trBC.startLap();
        testAssertTrue(reconstructor->reconstructionVerify(subset, true));
        secret = reconstructor->interpolate(subset);
        // NOTE: For Feldman, need to exponentiate secret by g1 and check against commitment
        // For Kate/AMT, don't need to do anything.
        if(vssType == "feld")
            testAssertEqual(secret * G1::one(), dynamic_cast<Dkg::FeldmanPlayer*>(reconstructor)->comm[0]);
        testAssertEqual(secret, actualSecret);
        trBC.endLap();

        std::cout << ", " << std::flush;
# endif
    }
    std::cout << endl;
#endif

    logperf << endl;

    logperf << td << endl;
    logperf << tv << endl;
    logperf << trBC << endl;
    logperf << trWC << endl;

    size_t vms, rss;
    getMemUsage(vms, rss);
    printMemUsage(nullptr); //"Memory usage at the end");

    fout
        << params.t << ","
        << params.n << ","
        << vssType << ","
#ifndef NDEBUG
        << "debug,"
#else
        << "release,"
#endif

        << numDealIters << ","
        << numVerIters << ","
        << numReconstrIters << ","

        << avgTimeOrNan(td, false) << ","
        << avgTimeOrNan(tv, false) << ","
        << avgTimeOrNan(trBC, false) << ","
        << avgTimeOrNan(trWC, false) << ","

        << avgTimeOrNan(td,   true) << ","
        << avgTimeOrNan(tv,   true) << ","
        << avgTimeOrNan(trBC, true) << ","
        << avgTimeOrNan(trWC, true) << ","

        << stddevOrNan(td) << ","
        << stddevOrNan(tv) << ","
        << stddevOrNan(trBC) << ","
        << stddevOrNan(trWC) << ","

        << Utils::humanizeBytes(vms) << ","
        << Utils::humanizeBytes(rss) << ","
        << numReal << ","
        << timeToString()
        << endl;

    loginfo << endl;

    // clean up
    for(auto ptr : players)
        delete ptr;
}

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);
    srand(static_cast<unsigned int>(time(NULL)));

    if(argc < 9) {
        cout << "Usage: " << argv[0] << " <public-params-file> <min-f> <max-f> <vss-types> <num-deal-iters> <out-file>" << endl;
        cout << endl;
        cout << "Simulates the specified VSSs with threshold starting at t = <min-f> + 1 up to t = <max-f> + 1, where n is always 2f+1." << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        cout << "   <public-params-file>    the file with q-SDH public params for Kate-based VSS" << endl;
        cout << "   <min-f>                 the min threshold # f of malicious participants" << endl;
        cout << "   <max-f>                 the max threshold # f of malicious participants" << endl;
        cout << "   <vss-types>             comma-separated list of VSSs you want to benchmark: feld, kate, kate-sim, amt or amt-sim" << endl;
        cout << "   <num-deal-iters>        the # of times to measure the dealing sub-phase" << endl;
        cout << "   <num-ver-iters>         the # of times to measure the verification sub-phase" << endl;
        cout << "   <num-reconstr-iters>    the # of times to measure the reconstruction phase" << endl;
        cout << "   <out-file>              write benchmark result to this CSV file" << endl;
        cout << endl;

        return 1;
    }

    std::string ppFile(argv[1]);
    size_t minf = static_cast<size_t>(std::stoi(argv[2]));
    size_t maxf = static_cast<size_t>(std::stoi(argv[3]));
    std::string vssList(argv[4]);
    size_t numDealIters = static_cast<size_t>(std::stoi(argv[5]));
    size_t numVerIters = static_cast<size_t>(std::stoi(argv[6]));
    size_t numReconstrIters = static_cast<size_t>(std::stoi(argv[7]));
    std::string outFile(argv[8]);

    if(minf > maxf) {
        logerror << "<min-f> needs to be <= than <max-f>" << endl;
        return 1;
    }

    std::vector<std::string> vsss;
    std::istringstream iss(vssList);
    std::string vssName;
    loginfo << endl;
    loginfo << "Benchmarking VSSs:" << endl;
    while(std::getline(iss, vssName, ',')) {
        loginfo << " * " << vssName << endl;
        vsss.push_back(vssName);
    }
    loginfo << endl;

    std::vector<size_t> fs;
    loginfo << "Benchmarking thresholds: " << endl;
    size_t maxN = 0, f, maxT = 0;
    for(size_t p = Utils::smallestPowerOfTwoAbove(minf); p <= maxf + 1; p *= 2) {
        f = p - 1;
        maxT = f + 1;
        maxN = 2*f + 1;
        loginfo << " * " << maxT << " out of " << maxN << " VSSs" << endl;
        fs.push_back(f);
    }
    assertNotZero(maxN);
    loginfo << endl;

    std::ofstream fout(outFile);
    if(fout.fail()) {
        logerror << "Could not open file '" << outFile << "' for writing benchmark results" << endl;
        return 1;
    }

    std::unique_ptr<AccumulatorTree> accs;
    std::unique_ptr<AuthAccumulatorTree> authAccs;
    std::unique_ptr<Dkg::KatePublicParameters> kpp;
    if(needsKatePublicParams(vsss)) {
        size_t maxPolyDegree = f;   // recall that poly of degree f has f+1 coefficients, but q = f
        kpp.reset(new Dkg::KatePublicParameters(ppFile, maxPolyDegree));

        loginfo << "Computing accumulators..." << endl;
        accs.reset(new AccumulatorTree(maxN));
        loginfo << "Authenticating accumulators..." << endl;
        authAccs.reset(new AuthAccumulatorTree(*accs, *kpp, maxT));
    }

    size_t vms, rss;
    getMemUsage(vms, rss);
    logperf << "VMS: " << Utils::humanizeBytes(vms) << endl;
    logperf << "RSS: " << Utils::humanizeBytes(rss) << endl;

    fout 
        << "t,n,vss,"
        << "build,"

        << "num_deal_iters,"
        << "num_verify_iters,"
        << "num_reconstr_iters,"

        << "avg_deal_usec,"
        << "avg_verify_usec,"
        << "avg_reconstr_bc_usec,"
        << "avg_reconstr_wc_usec,"

        << "avg_deal_hum,"
        << "avg_verify_hum,"
        << "avg_reconstr_bc_hum,"
        << "avg_reconstr_wc_hum,"

        << "stddev_deal,stddev_verify,"
        << "stddev_reconstr_bc,stddev_reconstr_wc,"

        << "vms_hum,rss_hum,"
        << "num_real_players,"
        << "date" 
        << endl;

#ifdef USE_MULTITHREADING
    logperf << "Using OpenMP multithreading..." << endl << endl;
#else
    logperf << "Multithreading disabled..." << endl << endl;
#endif

    for(auto& f : fs) {
        for(auto& vssType : vsss) {
            benchmarkVss(f, numDealIters, numVerIters, numReconstrIters, vssType, kpp.get(), authAccs.get(), fout);
        }
    }

    loginfo << "Benchmark ended successfully!" << endl;

    return 0;
}
