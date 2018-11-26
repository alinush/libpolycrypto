#pragma once

#include <polycrypto/PolyCrypto.h>
#include <vector>

#include <xassert/XAssert.h>

using namespace std;

namespace libpolycrypto {

/**
 * Generates public key pk and signer keys s.
 *
 * @param pkSigners will store the PKs of each signer here, if not null
 */
void generate_keys(size_t n, size_t k, G2& pk, vector<Fr>& s, vector<G2>* pkSigners);

/**
 * Signs a message hash with a signer key.
 */
G1 shareSign(const Fr& shareKey, const G1& msgHash);

/**
 * Takes in signature shares and lagrange coefficients to return the signature.
 */
G1 aggregate(const vector<G1>& sigShare, const vector<Fr>& coeffs);

/**
 * Returns signature shares with indices corresponding to the subset signers.
 */
vector<G1> align_shares(const vector<G1>& sigShare, const vector<size_t>& signers);

/**
 * Returns boolean array storing whether the signature share at each index is valid.
 */
vector<bool> batch_ver(const vector<G1>& sigShare, const vector<G2>& pkSigners, const G1& H);

/**
 * Builds binary trees for signature shares and public keys for batch verification.
 */
void build_batch_trees(
    const vector<G1>& sigShare,
    const vector<G2>& pkSigners,
    vector<vector<G1>>& sigShareTree,
    vector<vector<G2>>& pkTree);

/**
 * Checks if the combined signature share at height k and index is valid. 
 * If not, checks the combined signature shares of its children.
 */
void descend_batch_trees(
    vector<bool>& validShares,
    const vector<vector<G1>>& sigShareTree,
    const vector<vector<G2>>& pkTree,
    const G1& H, size_t k, size_t index);
}
