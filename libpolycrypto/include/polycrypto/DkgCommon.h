#pragma once

#include <vector>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/AccumulatorTree.h>

#include <xutils/Utils.h>

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::G2;
using libpolycrypto::GT;
using libpolycrypto::ReducedPairing;
using libpolycrypto::AccumulatorTree;
using libpolycrypto::AuthAccumulatorTree;

/**
 * TODO: Both Kate and Feldman PublicParams should extend this class.
 * TODO: AmtPublicParams should extend this class and have the accumulators.
 * TODO: eh, actually, i think we need to differentiate between a thresholdconfig and the actual public params
 * since in our experiments we want pp's for the "largest" thresholdconfig, and we want to create them only once
 * whereas we'll be creating thresholdconfig's for each experiment
 *
 * Not to be confused with public params, no?
 * Although the authAccs could be public params.
 */
class DkgParams {
public:
    size_t t;               // the threshold t to reconstruct the secret (=> degree t-1 polynomial)
    size_t n;               // the total # of players
    size_t N;               // the smallest N = 2^k such that n <= N
    size_t numBits;         // the number of bits in a player ID: i.e., log2(N)
    size_t maxLevel;        // the max level in a accumulator/roots-of-unity eval tree (leaves are level 0)

    std::unique_ptr<AccumulatorTree> accs;          // needed for AMT VSS/DKG
    const AuthAccumulatorTree * authAccs;  // needed for KZG/AMT VSS/DKG

    std::vector<Fr> omegas; // the first n Nth roots of unity
    GT gt;                  // the generator of GT

public:
    DkgParams(size_t t, size_t n, bool needsAccs)
        : t(t), 
          n(n),
          N(Utils::smallestPowerOfTwoAbove(n)),
          numBits(Utils::log2ceil(n)),
          maxLevel(Utils::log2floor(t-1)),
          accs(needsAccs ? new AccumulatorTree(n) : nullptr),
          authAccs(nullptr),
          omegas(needsAccs ? accs->getAllNthRootsOfUnity() : libpolycrypto::get_all_roots_of_unity(N)),
          gt(ReducedPairing(G1::one(), G2::one()))
    {
    }

    void setAuthAccumulators(const AuthAccumulatorTree* aa) {
        authAccs = aa;
    }

    /**
     * Returns the evaluation point for the player with the specified id \in {0, ..., n-1}.
     * For us, it's w_N^{id}.
     */
    const Fr& getEvaluationPoint(size_t id) const {
        assertInclusiveRange(0, id, n - 1);
        return omegas[id];
    }

    /**
     * Returns the KZG commitment to (x-\omega_N^id) needed to verify KZG proofs for f(id)
     */
    const G2& getMonomialCommitment(size_t playerId) const {
        assertNotNull(authAccs);
        assertStrictlyLessThan(playerId, n);

        auto leafIdx = libff::bitreverse(playerId, numBits);
        return authAccs->getLeaf(leafIdx);
    }
};


}
