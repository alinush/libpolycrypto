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
#include <sstream>

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

#include "Utils.h"

using namespace std;
using namespace Dkg;

/**
 * When n is really large, we don't want to spend all that time dealing in Kate/AMT/Feldman.
 * So we only have a subset of 'numDealIters' players deal.
 * We replicate these "real" players in allPlayers until it reaches size n, so we can call verifyOtherPlayers with it.
 */
void benchmarkDkg(
    size_t f,
    size_t numDealIters,
    size_t numVerifyBcIters,
    size_t numVerifyWcIters,
    size_t numReconstrBcIters,
    size_t numReconstrWcIters,
    const std::string& dkgType,
    const KatePublicParameters* kpp,
    const AuthAccumulatorTree* authAccs,
    std::ofstream& fout)
{
    DkgParams params(f+1, 2*f+1, dkgType != "feld");
    testAssertStrictlyGreaterThan(params.n, 1);

    std::unique_ptr<FeldmanPublicParameters> fpp;

    if(needsFeldmanPublicParams({ dkgType })) 
        fpp.reset(new FeldmanPublicParameters(params));
    if(needsKatePublicParams({ dkgType }))
        params.setAuthAccumulators(authAccs);

    std::vector<AbstractPlayer*> realPlayers;

    loginfo << "Benchmarking " << f + 1 << " out of " << 2*f+1 << " '" << dkgType << "' DKG ..." << endl;

    // numReal is the number of real players we create
    size_t numReal = std::min(params.n, numDealIters);
    bool isSimulated = boost::contains(dkgType, "-sim");

    size_t lastId = params.n - 1;
    for(size_t i = 0; i < numReal; i++) {
        // WARNING: We set the active players to be those with larger IDs, because otherwise Feldman DKG share verification
        // is much faster for ID w_N^0 = 1, since they have to compute \sum_{i=0}^{t-1} c_i^{1^i}, so no exponentiations will happen
        realPlayers.push_back(
            createPlayer(
                dkgType, params, lastId, kpp, fpp.get(), isSimulated, true));
        lastId--;
    }
    
    loginfo << numDealIters << " players are dealing" << endl;
    loginfo << numVerifyBcIters << " best-case verification iters" << endl;
    loginfo << numVerifyWcIters << " worst-case verification iters" << endl;
    loginfo << numReconstrBcIters << " best-case reconstruction iters" << endl;
    loginfo << numReconstrWcIters << " worst-case reconstruction iters" << endl;

    AveragingTimer td("1 player deal");
    loginfo << endl;
    loginfo << "Dealing players (" << numDealIters << " iters): " << endl;
    testAssertStrictlyGreaterThan(numDealIters, 0);
    for(size_t i = 0; i < numDealIters; i++) {
        auto p = realPlayers[i % realPlayers.size()];
        assertNotNull(p);

        td.startLap();
        p->deal();
        auto mus = td.endLap();

        loginfo << " - " << p->id << ": " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    // We need to create an array of all players for estimating the verification time.
    // We do this by repeatedly including players from realPlayers, but we cannot repeatedly include
    // the verifying player itself more than once, because that would bring the verification
    // time down, since the verifying player skips itself during verification.
    auto refillAllPlayers = [&params, &realPlayers](
        std::vector<AbstractPlayer*>& allPlayers,
        AbstractPlayer* verifyingPlayer) 
    {
        // fill an array of size n with all players
        allPlayers.clear();
        allPlayers.reserve(params.n);

        size_t i = 0;
        allPlayers.push_back(verifyingPlayer);
        while(allPlayers.size() < params.n) {
            auto p = realPlayers[i % realPlayers.size()];
            i++;

            if(p != verifyingPlayer)
                allPlayers.push_back(p);
        }
        testAssertEqual(allPlayers.size(), params.n);

        return allPlayers;
    };

    // If only one player was created, then we need to create a 2nd one. 
    // (Because the 1st player will verify himself too fast, by skipping over the share and NIZKPoK verification.)
    // Now the 2 players can verify each other.
    //
    // Better way? Have a AbstractPlayer::dkgVerify(size_t id, bool fastTrack) that simulates player 'id' verifying his share from this AbstractPlayer?
    // Well, then dkgVerify() would also have to simulate the aggregation of comm/pk/share/proof (in the best-case and worst-case)
    // So it would still need access to all n players.
    bool needsOneMorePlayer = realPlayers.size() == 1;
    bool needsVerification = numVerifyBcIters > 0 || numVerifyWcIters > 0;
    if(needsVerification) {
        if(needsOneMorePlayer) {
            loginfo << "Need more than 1 player for verification subphase, creating new simulated player..." << endl;
            // if we only have one player (with id 1), then we create another player (with id 2)
            // and refillAllPlayers will return [2, 1, 1, ..., 1] or [1, 2, 2, ..., 2]
            realPlayers.push_back(
                // this player always simulates dealing, to make it faster
                createPlayer(
                    dkgType, params, lastId, kpp, fpp.get(), true, true
                )
            );
            lastId--;

            logperf << "Simulated deal (extra):" << endl;
            ManualTimer t;
            realPlayers.back()->deal();
            auto mus = t.stop().count();
            logperf << " - " << realPlayers.back()->id << ": " << Utils::humanizeMicroseconds(mus, 2) << endl;
        } else {
            // if we have more than 2 players that dealt, they can verify each other
            // (i.e., refillAllPlayers will return [1, 2, 2, ..., 2] or [2, 1, 1, ..., 1]
        }
    }

    std::vector<AbstractPlayer*> allPlayers;

    AveragingTimer tvBC("1 player verify (best-case) ");
    loginfo << "Verifying players, best-case (" << numVerifyBcIters << " iters): " << endl;
    for(size_t i = 0; i < numVerifyBcIters; i++) {
        auto& p = realPlayers.at(i % realPlayers.size());
        refillAllPlayers(allPlayers, p);

        tvBC.startLap();
        testAssertTrue(p->verifyOtherPlayers(allPlayers, true));
        auto mus = tvBC.endLap();

        loginfo << " - " << p->id << ": " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    AveragingTimer tvWC("1 player verify (worst-case) ");
    loginfo << "Verifying players, worst-case (" << numVerifyWcIters << " iters): " << endl;

    for(size_t i = 0; i < numVerifyWcIters; i++) {
        auto& p = realPlayers.at(i % realPlayers.size());
        refillAllPlayers(allPlayers, p);

        tvWC.startLap();
        testAssertTrue(p->verifyOtherPlayers(allPlayers, false));
        auto mus = tvWC.endLap();

        loginfo << " - " << p->id << ": " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    // measure the time to compute Lagrange coefficients \ell_i for interpolation
    // we ignore the time to compute s = \sum_i \ell_i s_i (e.g., a few ms compared to 
    vector<size_t> subset;
    AveragingTimer trBC("Reconstr. best-case");
    AveragingTimer trWC("Reconstr. worst-case");

    loginfo << "Reconstructing, best-case (" << numReconstrBcIters << " iters): " << endl;
    for(size_t i = 0; i < numReconstrBcIters; i++) {
        auto reconstructor = realPlayers.at(i % realPlayers.size());
        assertNotNull(reconstructor);
        G1 pk = reconstructor->getSecret() * G1::one(); // PK = g^secret (if all went well)
        Fr secret;

        // best case: you get all correct shares (you don't know it, but you find out as you verify)
        subset = random_subset(params.t, params.n);
        std::sort(subset.begin(), subset.end()); // for AMT's reconstructVerify

        trBC.startLap();
        testAssertTrue(reconstructor->reconstructionVerify(subset, true));
        secret = reconstructor->interpolate(subset);
        // in the DKG fast track, we just interpolate directly and check against the PK
        testAssertEqual(secret*G1::one(), pk);
        auto mus = trBC.endLap();

        loginfo << " - " << reconstructor->id << ": " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    // TODO: should move this into a function, but be careful with verifying the secret
    // because in VSS there's no verification, whereas in DKG, you check if it matches the PK in the fast track
    loginfo << "Reconstructing, worst-case (" << numReconstrWcIters << " iters): " << endl;
    for(size_t i = 0; i < numReconstrWcIters; i++) {
        auto reconstructor = realPlayers.at(i % realPlayers.size());
        assertNotNull(reconstructor);
        Fr secret;

        // worst case: you have to verify n shares before you find a subset of t correct ones
        subset = random_subset(params.t, params.n);
        std::sort(subset.begin(), subset.end()); // for AMT's reconstructVerify

        trWC.startLap();
        testAssertTrue(reconstructor->reconstructionVerify(subset, false));
        secret = reconstructor->interpolate(subset);
        auto mus = trWC.endLap();

        // NOTE: since this is the worst-case, all shares are verified, so recovered secret is guaranteed to be correct 
        testAssertEqual(secret, reconstructor->getSecret());

        loginfo << " - " << reconstructor->id << ": " << Utils::humanizeMicroseconds(mus, 2) << endl;

    }

    logperf << endl;
    logperf << td << endl;
    logperf << tvBC << endl;
    logperf << tvWC << endl;
    logperf << trBC << endl;
    logperf << trWC << endl;
    logperf << endl;

    size_t vms, rss;
    getMemUsage(vms, rss);
    printMemUsage(nullptr); //"Memory usage at the end");

    // clean up
    for(auto ptr : realPlayers)
        delete ptr;
    
    fout
        << params.t << "," 
        << params.n << "," 
#ifndef NDEBUG
        << "debug,"
#else
        << "release,"
#endif
        << dkgType << ","
        << numDealIters << ","
        << numVerifyBcIters << ","
        << numVerifyWcIters << ","
        << numReconstrBcIters << ","
        << numReconstrWcIters << ",";

    fout << avgTimeOrNan(td, false) << ",";
    fout << avgTimeOrNan(tvBC, false) << ",";
    fout << avgTimeOrNan(tvWC, false) << ",";
    fout << avgTimeOrNan(trBC, false) << ",";
    // NOTE: Worst-case reconstruction is triggered after best-case fails, so need to account for the
    // wasted work.
    fout << sumAvgTimeOrNan({trBC, trWC}, true) << ",";

    fout << avgTimeOrNan(td, true) << ",";
    fout << avgTimeOrNan(tvBC, true) << ",";
    fout << avgTimeOrNan(tvWC, true) << ",";
    fout << avgTimeOrNan(trBC, true) << ",";
    fout << sumAvgTimeOrNan({trBC, trWC}, true) << ",";

    fout << stddevOrNan(td) << ",";
    fout << stddevOrNan(tvBC) << ",";
    fout << stddevOrNan(tvWC) << ",";
    fout << stddevOrNan(trBC) << ",";
    // TODO: the worst-case time is actually trBC + trWC, so we need to update the stddev here
    fout << stddevOrNan(trWC) << ",";

    fout
        << Utils::humanizeBytes(vms) << ","
        << Utils::humanizeBytes(rss) << ","
        << timeToString()
        << endl;

    loginfo << endl;
}

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);
    srand(static_cast<unsigned int>(time(NULL)));
    
#ifndef NDEBUG
    //assertFalse("Don't benchmark in debug mode!");
#endif

    if(argc < 11) {
        cout << "Usage: " << argv[0] << " <public-params-file> <min-f> <max-f> <dkgs> <num-deal> <num-verify-bc> <num-verify-wc> <num-reconstr-bc> <num-reconstr-wc> <out-file>" << endl;
        cout << endl;

        cout << "Simulates the specified DKGs with threshold starting at t = <min-f> + 1 up to t = <max-f> + 1, where n is always 2f+1." << endl;
        cout << endl;

        cout << "OPTIONS: " << endl;
        cout << "   <public-params-file>    the file with q-SDH public params for Kate-based DKG" << endl;
        cout << "   <min-f>                 the min threshold # f of malicious participants" << endl;
        cout << "   <max-f>                 the max threshold # f of malicious participants" << endl;
        cout << "   <dkgs>                  comma-separated list of DKGs you want to benchmark: feld, kate, or amt" << endl;
        cout << "   <num-deal>              <num-deal> out of total n players deal (when > n, just have players re-deal)" << endl;
        cout << "   <num-verify-bc>         # of iters for best-case verification" << endl;
        cout << "   <num-verify-wc>         # of iters for worst-case verification" << endl;
        cout << "   <num-reconstr-bc>       # of iters for best-case reconstruction phase" << endl;
        cout << "   <num-reconstr-wc>       # of iters for worst-case reconstruction phase" << endl;
        cout << "   <out-file>              write benchmark result to this CSV file" << endl;
        cout << endl;

        return 1;
    }

    std::string ppFile(argv[1]);
    size_t minf = static_cast<size_t>(std::stoi(argv[2]));
    size_t maxf = static_cast<size_t>(std::stoi(argv[3]));
    std::string dkgList(argv[4]);
    size_t numDealIters     = static_cast<size_t>(std::stoi(argv[5]));
    size_t numVerifyBcIters = static_cast<size_t>(std::stoi(argv[6]));
    size_t numVerifyWcIters = static_cast<size_t>(std::stoi(argv[7]));
    size_t numReconstrBcIters = static_cast<size_t>(std::stoi(argv[8]));
    size_t numReconstrWcIters = static_cast<size_t>(std::stoi(argv[9]));
    std::string outFile(argv[10]);

    if(minf > maxf) {
        logerror << "<min-f> needs to be <= than <max-f>" << endl;
        return 1;
    }

    std::vector<std::string> dkgs;
    std::istringstream iss(dkgList);
    std::string dkgName;
    loginfo << endl;
    loginfo << "Benchmarking DKGs:" << endl;
    while(std::getline(iss, dkgName, ',')) {
        loginfo << " * " << dkgName << endl;
        dkgs.push_back(dkgName);
    }
    loginfo << endl;

    std::vector<size_t> fs;
    loginfo << "Benchmarking thresholds: " << endl;
    size_t maxN = 0, f, maxT = 0;
    for(size_t p = Utils::smallestPowerOfTwoAbove(minf); p <= maxf + 1; p *= 2) {
        f = p - 1;
        maxT = f + 1;
        maxN = 2*f + 1;
        loginfo << " * " << maxT << " out of " << maxN << " DKGs" << endl;
        fs.push_back(f);
    }
    testAssertNotZero(fs.size());
    loginfo << endl;

    std::ofstream fout(outFile);
    if(fout.fail()) {
        logerror << "Could not open file '" << outFile << "' for writing benchmark results" << endl;
        return 1;
    }

    std::unique_ptr<AccumulatorTree> accs;
    std::unique_ptr<AuthAccumulatorTree> authAccs;
    std::unique_ptr<KatePublicParameters> kpp;
    if(needsKatePublicParams(dkgs)) {
        size_t maxPolyDegree = f;   // recall that poly of degree f has f+1 coefficients, but q = f
        kpp.reset(new KatePublicParameters(ppFile, maxPolyDegree));

        loginfo << "Computing accumulators..." << endl;
        accs.reset(new AccumulatorTree(maxN));
        loginfo << "Authenticating accumulators..." << endl;
        authAccs.reset(new AuthAccumulatorTree(*accs, *kpp, maxT));
    }

    fout 
        << "t,"
        << "n,"
        << "build,"
        << "dkg,"

        << "num_deal_iters,"
        << "num_verify_bc_iters,"
        << "num_verify_wc_iters,"
        << "num_reconstr_bc_iters,"
        << "num_reconstr_wc_iters,"

        << "avg_deal_usec,"
        << "avg_verify_best_case_usec,"
        << "avg_verify_worst_case_usec,"
        << "avg_reconstr_bc_usec," 
        << "avg_reconstr_wc_usec," 

        << "avg_deal_hum,"
        << "avg_verify_bc_hum,"
        << "avg_verify_wc_hum,"
        << "avg_reconstr_bc_hum,"
        << "avg_reconstr_wc_hum," 

        << "stddev_deal,"
        << "stddev_verify_bc,"
        << "stddev_verify_wc,"
        << "stddev_reconstr_bc,"
        << "stddev_reconstr_wc,"

        << "vms_hum,"
        << "rss_hum,"
        << "date"
        << endl;

#ifdef USE_MULTITHREADING
    logperf << "Using OpenMP multithreading..." << endl << endl;
#else
    logperf << "Multithreading disabled..." << endl << endl;
#endif

    for(auto& f : fs) {
        for(auto& dkgType : dkgs) {
            benchmarkDkg(f,
                numDealIters,
                numVerifyBcIters,
                numVerifyWcIters,
                numReconstrBcIters,
                numReconstrWcIters,
                dkgType,
                kpp.get(),
                authAccs.get(),
                fout);
        }
    }

    loginfo << "Benchmark ended successfully!" << endl;

    return 0;
}
