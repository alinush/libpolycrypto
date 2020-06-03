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

void logMemUsage() {
    size_t vms, rss;
    getMemUsage(vms, rss);
    logperf << "VMS: " << Utils::humanizeBytes(vms) << endl;
    logperf << "RSS: " << Utils::humanizeBytes(rss) << endl;
    logperf << endl;
}

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);

    if(argc < 5) {
        cout << "Usage: " << argv[0] << "<deg> <n> <r>" << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        cout << "   <pp-file>  the Kate public parameters file" << endl;
        cout << "   <deg>      the degree of the evaluated polynomial" << endl;
        cout << "   <n>        the # of points to evaluate at (i.e., # of AMT leaves)" << endl;  
        cout << "   <r>        the # of times to repeat the AMT auth + verif" << endl;
        cout << endl;

        return 1;
    }
    
    std::string ppFile = argv[1];
    size_t deg = static_cast<size_t>(std::stoi(argv[2]));
    size_t n = static_cast<size_t>(std::stoi(argv[3]));
    size_t r = static_cast<size_t>(std::stoi(argv[4]));

    std::unique_ptr<Dkg::KatePublicParameters> kpp(
        new Dkg::KatePublicParameters(ppFile, deg));

    loginfo << endl;
    loginfo << "Degree " << deg << " poly, evaluated at n = " << n << " points, iters = " << r << endl;

    AveragingTimer at("Accum tree");
    at.startLap();
    AccumulatorTree accs(n);
    auto mus = at.endLap();

    logperf << " - AccumulatorTree: " << Utils::humanizeMicroseconds(mus, 2) << endl;
    
    // NOTE: Uncomment this to see beautiful roots-of-unity structure
    //std::cout << "Accumulator tree for n = " << n << endl;
    //std::cout << accs.toString() << endl;

    std::vector<Fr> f = random_field_elems(deg + 1);

    // Step 1: Fast multipoint eval
    AveragingTimer et("Roots-of-unity eval ");
    et.startLap();
    RootsOfUnityEvaluation eval(f, accs);
    std::vector<Fr> evals = eval.getEvaluations();
    mus = et.endLap();

    logperf << " - Roots of unity eval: " << Utils::humanizeMicroseconds(mus, 2) << endl;

    // Step 2: Authenticate AccumulatorTree
    AveragingTimer aat("Auth accum tree");
    aat.startLap();
    AuthAccumulatorTree authAccs(accs, *kpp, deg+1);
    mus = aat.endLap();
    
    logperf << " - AuthAccumulatorTree: " << Utils::humanizeMicroseconds(mus, 2) << endl;
    
    // Step 3: Authenticate Multipoint Evaluation
    AveragingTimer ars("Auth roots-of-unity eval (simulated)");
    for(size_t i = 0; i < r; i++) {
        ars.startLap();
        AuthRootsOfUnityEvaluation authEval(eval, *kpp, true);
        mus = ars.endLap();

        logperf << " - AuthRootsOfUnityEval simulated (iter " << i << "): " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    AveragingTimer ar("Auth roots-of-unity eval");
    for(size_t i = 0; i < r; i++) {
        ar.startLap();
        AuthRootsOfUnityEvaluation authEval(eval, *kpp, false);
        mus = ar.endLap();

        logperf << " - AuthRootsOfUnityEval (iter " << i << "): " << Utils::humanizeMicroseconds(mus, 2) << endl;
    }

    logperf << endl;
    logperf << at << endl;
    logperf << aat << endl;
    logperf << et << endl;
    logperf << ar << endl;
    logperf << ars << endl;
    logperf << endl;
     
    // Step 4: Verify AMT proofs (TODO: implement)
    
    loginfo << "Exited succsessfully!" << endl;

    return 0;
}
