#include <fstream>
#include <string>

#include <polycrypto/PolyCrypto.h>

#include <xutils/Utils.h>
#include <xutils/Log.h>

#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;
using namespace std;
using namespace libpolycrypto;

/**
 * How do we reason about bandwidth in (t, n) DKG? (i.e., need t shares to reconstruct)
 *  - c   = the size of a poly commitment
 *  - s   = the size of a share
 *  - p   = the size of a proof for a share
 *  - f0c = the size of a Feldman commitment g^f(0) to f(0)
 *  - f0p = the size of the proof for g^f(0) (includes NIZKPoK for Kate et al)
 */

/**
 *  DOWNLOAD
 *  --------
 *  Each player i\in [n] needs to obtain from every other player j \ne i: a poly commitment + share + proof + f(0) comm + f(0) proof
 *   - per-player: (n-1)*(c + s + p + f0c + f0p) commitments + shares + proofs need to be downloaded
 *   - total: n * per-player
 */
template<class T>
T calcDownload(T n, T c, T s, T p, T f0c, T f0p) {
    T perPlayer = (n-1)*(c + s + p + f0c + f0p);

    return perPlayer;
}

/**
 *  UPLOAD
 *  ------
 *  Each player i needs to broadcast their poly commitment + f(0) comm + f(0) proof (same in eJF-DKG and in AMT DKG)
 *  Each player i sends n-1 shares to the n-1 other players (same in eJF-DKG and in AMT DKG)
 *  Each player i publishes the AMT (O(n) best case) or sends proofs individually (O(n log t) worst case)
 *  Assume n = 2^k for some k
 *
 *   - Best case per-player:  (c + f0c + f0p) + (n-1)*s + (2n-1)*c (for the AMT quotient commitments)
 *      - In eJF-DKG, last sum term would have been (n-1)*c (so we are only a bit more expensive)
 *
 *   - Worst case per-player: (c + f0c + f0p) + (n-1)*(s + (log(t-1) + 1)c)
 */
template<class T>
T calcUpload_WorstCase(T n, T c, T s, T p, T f0c, T f0p) {
    T perPlayer = c + f0c + f0p + (n-1)*(s + p);

    return perPlayer;
}

template<class T>
T calcUpload(T n, T c, T s, T p, T f0c, T f0p) {
    return calcUpload_WorstCase(n, c, s, p, f0c, f0p);
}

// NOTE: t is the reconstruction threshold so the poly degrees are t-1
template<class T>
std::tuple<T,T> feldman(T n, T t) {
    const T polyDeg = (t - 1);
    const T commSize = 32 * (polyDeg + 1); // need to send a group element for each coeff of the poly
    const T shareSize = 32;        // share is just a field element
    const T proofSize = 0;         // can verify share against commitment directly (not true for Pedersen though: needs 32-byte r(i))
    const T f0commSize = 0;        // g^f(0) is already part of the Feldman commitment to the first coeff c_0 = f(0)
    const T f0proofSize = 0;       // can verify f(0) directly against comm

    return std::make_tuple(
        calcDownload(n, commSize, shareSize, proofSize, f0commSize, f0proofSize),
        calcUpload(n, commSize, shareSize, proofSize, f0commSize, f0proofSize));
}

template<class T>
std::tuple<T,T> kate(T n) {
    const T commSize = 32;          // g^p(s) is one group element
    const T shareSize = 32;         // share is a field element
    const T proofSize = 32;         // proof is g^{p(s) - p(i) / (x - i)}: one group element
    const T f0commSize = 32;        // one group element
    // a Schnorr signature (e = H(g || g^k || g^x || proverID || bla), s = k + ex)
    // Reference: https://tools.ietf.org/html/rfc8235#page-8
    const T nizkPok = 64;           // basically a Schnorr signature
    const T f0proofSize = 32 + nizkPok; // g^f(0) proof: normal Kate proof + a NIZKPoK of f(0) w.r.t. g

    return std::make_tuple(
        calcDownload(n, commSize, shareSize, proofSize, f0commSize, f0proofSize),
        calcUpload(n, commSize, shareSize, proofSize, f0commSize, f0proofSize));
}

/**
 * This is the bandwidth if we send a constant-sized proof for g^f(0) and a log n-sized proof for f(i)
 */
template<class T>
std::tuple<T,T> amt(T n, T t) {
    // a tree of n = 2^i nodes has i + 1 nodes along any path and that's how many quotient commitments will be in our proof
    // when n is not a power of two, we round up using log2ceil
    const T commSize = 32;
    const T shareSize = 32;
    // we are only evaluating at n points this time, and proving g^f(0) using a normal Kate proof
    //const T numLevels = Utils::log2ceil(n) + 1;
    const T proofSize = (Utils::log2floor(t-1) + 1) * 32;
    const T f0commSize = 32;
    const T nizkPok = 64;           // see kate() description
    const T f0proofSize = 32 + nizkPok;    // proof for g^f(0) is now a normal constant-sized Kate proof (+ NIZKPoK)

    //loginfo << " * numLevels = " << numLevels << " (for n = " << n << ")" << endl;

    return std::make_tuple(
        calcDownload(n, commSize, shareSize, proofSize, f0commSize, f0proofSize),
        calcUpload(n, commSize, shareSize, proofSize, f0commSize, f0proofSize));
}

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);
    
    if(argc < 3) {
        cout << "Usage: " << argv[0] << " <out-file> <max-f>" << endl;
        cout << endl;
        cout << "Estimates bandwidth for Feldman, Kate and AMT DKGs" << endl;
        cout << endl;
        cout << " <out-file>     writes numbers in this file" << endl;
        cout << " <max-f>        computes numbers for (k=f+1, n=2f+1) thresholds for f up to <max-f>" << endl;
        cout << endl;
        return 1;
    }

    string outFile(argv[1]);
    size_t max_f = static_cast<size_t>(std::stoi(argv[2]));

    ofstream fout(outFile);

    if(fout.fail()) {
        throw std::runtime_error("Could not write to output file");
    }

    using NumBytesType = int128_t;
    loginfo << "sizeof(NumBytesType): " << sizeof(NumBytesType) << endl;

    fout << "t,n,dkg,download_bw_bytes,download_bw_hum,upload_bw_bytes,upload_bw_hum,comm_bw_bytes,comm_bw_hum,total_bw_bytes,total_bw_hum" << endl;

    for(size_t p = 2; p <= max_f + 1; p *= 2) {
        size_t f = p - 1;
        size_t t = f + 1;
        size_t n = 2*f + 1;
        loginfo << t << " out of " << n  << " DKG" << endl;

        auto writeRow = [&fout, &n, &t](std::tuple<NumBytesType, NumBytesType> bytes, const char * scheme) {
            NumBytesType perPlayerDown = std::get<0>(bytes);
            NumBytesType perPlayerUp = std::get<1>(bytes);
            fout
                 << t << ","
                 << n << ","
                 << scheme << ","
                 << perPlayerDown << ","
                 << Utils::humanizeBytes(perPlayerDown) << ","
                 << perPlayerUp << ","
                 << Utils::humanizeBytes(perPlayerUp) << ","
                 << perPlayerDown + perPlayerUp << ","
                 << Utils::humanizeBytes(perPlayerDown + perPlayerUp) << ","
                 << n*(perPlayerDown + perPlayerUp) << ","
                 << Utils::humanizeBytes(n*(perPlayerDown + perPlayerUp))
                 << endl;
        };

        writeRow(feldman(n, t), "JF-DKG");
        writeRow(kate(n), "eJF-DKG");
        writeRow(amt(n, t), "AMT DKG");
    }
    
    loginfo << "All done!" << endl;

    return 0;
}
