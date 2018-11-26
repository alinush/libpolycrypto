#include <fstream>
#include <algorithm>

#include <polycrypto/Configuration.h>
#include <polycrypto/FFThresh.h>
#include <polycrypto/PolyOps.h>

#include <xutils/Utils.h>

using namespace std;

namespace libpolycrypto {

void generate_keys(size_t n, size_t k, G2& pk, vector<Fr>& sk, vector<G2>* pkSigners) {
    // set random polynomial p of degree k - 1
    std::vector<Fr> p = random_field_elems(k);

    // set signer id's to w_N^0, w_N^1, w_N^2, ... w_N^{n-1} where N = 2^i is the smallest N such that n <= N
    // (and no signer id should be 0 since the secret key s = p(0) )
    size_t N = Utils::smallestPowerOfTwoAbove(n);

    // pk = g2^s = g2^p(0)
    pk = p[0]  * G2::one();

    // sk[i] = p evaluated at w_N^i
    poly_fft(p, N, sk);
    // we only need the first n <= N roots of unity
    sk.resize(n);

    if(pkSigners != nullptr) {
        // pkSigners[i] = g2^s_i
        pkSigners->resize(n); 
        for (size_t i = 0; i < n; i++) {
            (*pkSigners)[i] = sk[i] * G2::one();
        }
    }
}

G1 shareSign(const Fr& shareKey, const G1& msgHash) {
    // TODO: assuming message is really H(message)
    return shareKey * msgHash;
}

G1 aggregate(const vector<G1>& sigShare, const vector<Fr>& coeffs) {
    return multiExp(sigShare, coeffs);
}

vector<G1> align_shares(const vector<G1>& sigShare, const vector<size_t>& signers) {
    vector<G1> sigSharesSubset(signers.size());

    for (size_t i = 0; i < sigSharesSubset.size(); i++) {
        sigSharesSubset[i] = sigShare[signers[i]];
    }

    return sigSharesSubset;
}

vector<bool> batch_ver(const vector<G1>& sigShare, const vector<G2>& pkSigners, const G1& H) {
    vector<bool> validShares(sigShare.size(), true);
    vector<vector<G1>> sigShareTree;
    vector<vector<G2>> pkTree;

    size_t height = static_cast<size_t> (ceil(log(sigShare.size()) / log(2)) + 1);

    build_batch_trees(sigShare, pkSigners, sigShareTree, pkTree);
    descend_batch_trees(validShares, sigShareTree, pkTree, H, height - 1, 0);
    return validShares;
}

void build_batch_trees(
    const vector<G1>& sigShare,
    const vector<G2>& pkSigners,
    vector<vector<G1>>& sigShareTree,
    vector<vector<G2>>& pkTree)
{
    size_t height = static_cast<size_t> (ceil(log(sigShare.size()) / log(2)) + 1);
    sigShareTree.resize(height);
    pkTree.resize(height);
    sigShareTree[0] = sigShare;
    pkTree[0] = pkSigners;

    for (size_t i = 1; i < height; i++) {
        sigShareTree[i].resize((sigShareTree[i-1].size() + 1) / 2);
        pkTree[i].resize((pkTree[i-1].size() + 1) / 2);
        for (size_t j = 0; j < sigShareTree[i].size(); j++) {
            // if last node cannot be paired, set parent to itself
            if (2 * j + 1 >= sigShareTree[i-1].size()) {
                sigShareTree[i][j] = sigShareTree[i - 1][2 * j];
                pkTree[i][j] = pkTree[i - 1][2 * j];
            } else {
                sigShareTree[i][j] = sigShareTree[i - 1][2 * j] + sigShareTree[i - 1][2 * j + 1];
                pkTree[i][j] = pkTree[i - 1][2 * j] + pkTree[i - 1][2 * j + 1];
            }
        }
    }
}

void descend_batch_trees(
    vector<bool>& validShares,
    const vector<vector<G1>>& sigShareTree,
    const vector<vector<G2>>& pkTree,
    const G1& H, size_t k, size_t index)
{
    // return if verifies correctly
    if (ReducedPairing(sigShareTree[k][index], G2::one()) == ReducedPairing(H, pkTree[k][index])) return;

    // if on bottom row, set share to false
    if (k == 0) {
        validShares[index] = false;
        return;
    }

    // check children
    descend_batch_trees(validShares, sigShareTree, pkTree, H, k - 1, index * 2);
    if (index * 2 + 1 < sigShareTree[k - 1].size()) {
        descend_batch_trees(validShares, sigShareTree, pkTree, H, k - 1, index * 2 + 1);
    }
}

}
