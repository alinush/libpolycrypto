#pragma once

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/DkgCommon.h>
#include <polycrypto/Lagrange.h>
#include <polycrypto/AccumulatorTree.h>
#include <polycrypto/RootsOfUnityEval.h>

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::AccumulatorTree;
using libpolycrypto::AuthAccumulatorTree;
using libpolycrypto::RootsOfUnityEvaluation;

class AbstractPlayer {
public:
    size_t id;              // this player's ID (from 0 to n-1)

    const DkgParams& params;

    /**
     * Random degree t-1 poly f_id(x).
     * In DKG, the final polynomial f(x) will be \sum_{j\in IDs} f_j(x)
     */
    std::vector<Fr> f_id;

    /**
     * Shares of f_id(x) for other players.
     * i.e., shares[j] = f_id(j).
     * Player IDs start at j = 0.
     */
    std::vector<Fr> shares;

    Fr share;   // share of the final DKG secret: f(id) = \sum_{j\in IDs} f_j(id)
    G1 pk;      // the corresponding PK of the final DKG secret

    /**
     * True when this player will simulate dealing rather than actually deal
     * (to make benchmarks faster).
     *
     * For Feldman, simulation is the same as normal dealing.
     * For Kate, the trapdoor is used to compute proofs in O(1) rather than O(n) time.
     * Same for AMT.
     */
    bool simulated;

    /**
     * If this is a VSS (not a DKG) player, then the NIZKPoK and f0comm are not needed in Kate/AMT subclasses
     */
    bool isDkgPlayer;

public:
    AbstractPlayer(const DkgParams& params, size_t id, bool isSimulated, bool isDkgPlayer)
        : id(id), params(params), share(0), pk(G1::zero()), simulated(isSimulated), isDkgPlayer(isDkgPlayer)
    {}

    virtual ~AbstractPlayer() {}

public:
    bool isSimulated() const { return simulated; }
    void deal() {
        // picks a degree t-1 polynomial that can be recovered by t people
        f_id = libpolycrypto::random_field_elems(params.t);
        // evaluate f_id(.) to compute shares for each player 
        // NOTE: KatePlayer will set this to do nothing, since evaluation happens "for free" as proofs are computed
        evaluate();

        if(simulated) {
            simulatedDealImpl();
        } else {
            // implementation-specific DKG dealing code
            dealImpl();
        }
    }

    /**
     * Returns the secret f_id(0)
     */
    const Fr& getSecret() const {
        return f_id[0];
    }

    /**
     * Evaluates f_id() to compute shares for all players.
     *
     * Feldman does this via an FFT.
     * Kate does this while computing proofs.
     * AMT does this using the roots-of-unity evaluation tree.
     */
    virtual void evaluate() = 0;

    /**
     * Implements the per-player verification in a DKG protocol.
     * Verifies this player's share of f_j(x) received from player j != id.
     * Assembles this player's final share of f(x) = \sum_{j \in IDs} f_j(x).
     */
    virtual bool verifyOtherPlayers(const std::vector<AbstractPlayer*>& allPlayers, bool agg) = 0;

    /** 
     * See subclasses for how reconstruction uses subset/fast-track.
     *
     * @param subset        a subset of player IDs used to interpolate
     * @param fastTrack     true if should simulate a fast-track reconstruction, false otherwise
     */
    virtual bool reconstructionVerify(const std::vector<size_t>& subset, bool fastTrack) = 0;

    /**
     * Interpolates the secret from a subset of size t of the players.
     *
     * For DKG this should be done from on the aggregated shares, but it would give the same performance, so we just do it on one of the DKG players.
     */
    Fr interpolate(const std::vector<size_t>& subset) {
        assertEqual(subset.size(), params.t);

        // get all N Nth roots of unity
        const std::vector<Fr>& allOmegas = params.omegas;

        // get interpolation points
        vector<Fr> someOmegas;
        for(auto pid : subset) {
            testAssertStrictlyLessThan(pid, allOmegas.size());
            someOmegas.push_back(allOmegas[pid]);
        }

        // interpolate
        std::vector<Fr> lagr;
        // TODO: parameterize threshold where fast Lagrange beats naive Lagrange
        if(subset.size() < 64) {
            lagrange_coefficients_naive(lagr, allOmegas, someOmegas, subset);
        } else {
            lagrange_coefficients(lagr, allOmegas, someOmegas, subset);
        }
        Fr s = Fr::zero();
        assertEqual(lagr.size(), subset.size());
        for(size_t i = 0; i < lagr.size(); i++) {
            s += lagr[i] * shares[subset[i]];
        }

        return s;
    }

    /**
     * After receiving shares from all players in the DKG protocol, this function
     * should verify the final share of this player against the final commitment.
     */
    virtual bool verifyFinalShareProof() = 0;

    virtual void simulatedDealImpl() = 0;

    virtual void dealImpl() = 0;
};

}
