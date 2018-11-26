#include <polycrypto/PolyCrypto.h>
#include <polycrypto/NizkPok.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

using namespace std;

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::NizkPok;

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);

    if(argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        cout << "Usage: " << argv[0] << " [<r>]" << endl;
        cout << endl;
        cout << "OPTIONS: " << endl;
        cout << "   <r>    the number of times to repeat the measurements" << endl;  
        cout << endl;

        return 1;
    }

    int r = 500;
    if(argc > 1)
        r = std::stoi(argv[1]);

    AveragingTimer tp("NIZK prove (diff x)  "), tv("NIZK verify (diff pi) ");
    for(int i = 0; i < r; i++) {
        Fr x = Fr::random_element();
        G1 gToX = x*G1::one();

        tp.startLap();
        auto pi = NizkPok::prove(G1::one(), x, gToX);
        tp.endLap();

        tv.startLap();
        testAssertTrue(NizkPok::verify(pi, G1::one(), gToX));
        tv.endLap();
    }

    logperf << tp << endl;
    logperf << tv << endl;
    
    AveragingTimer tps("NIZK prove (same x)  "), tvs("NIZK verify (same pi)");
    Fr x = Fr::random_element();
    G1 gToX = x*G1::one();
    NizkPok pi;
    for(int i = 0; i < r; i++) {
        tps.startLap();
        pi = NizkPok::prove(G1::one(), x, gToX);
        tps.endLap();
    }

    for(int i = 0; i < r; i++) {
        tvs.startLap();
        testAssertTrue(NizkPok::verify(pi, G1::one(), gToX));
        tvs.endLap();
    }

    logperf << tps << endl;
    logperf << tvs << endl;

    return 0;
}
