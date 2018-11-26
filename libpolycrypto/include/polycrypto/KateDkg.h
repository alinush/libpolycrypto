#pragma once

#include <memory>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/DkgCommon.h>
#include <polycrypto/AbstractKatePlayer.h>
#include <polycrypto/KatePublicParameters.h>

#include <xutils/Log.h>
#include <xutils/Timer.h>
#include <xassert/XAssert.h>

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::G2;
using libpolycrypto::GT;
using libpolycrypto::ReducedPairing;
using libpolycrypto::multiExp;

/**
 * Let n denote the number of players in the DKG/VSS.
 * Let N = 2^k be the smallest N such that n <= N.
 * Let p(x) denote the degree t-1 polynomial picked by this player.
 * Let s denote the trapdoor used to commit to p as g^{p(s)} where g is a generator of a group G.
 *
 * Stores this player's Kate et al proofs for p(w_N^id), i.e., shares for all players with id \in {0, ..., n-1}
 */
class AllConstantSizedProofs {
public:
    const KatePublicParameters& kpp;
    std::vector<G1> playerProof;    // playerProof[i] stores the proof for p(w_N^i)
    G1 zeroProof;                   // proof for p(0), i.e., the secret
    G2 g2toId;                      // g2^(s - w_N^id); needed for verifying this player's shares on other player's polynomials

public:
    AllConstantSizedProofs(const KatePublicParameters& kpp, size_t n)
        : kpp(kpp)
    {
        playerProof.resize(n);
    }

public:
    /**
     * Sets the proof for p(w_N^id), id \in {0, ..., n-1}. 
     */
    void setPlayerProof(size_t id, const G1& proof) {
        assertStrictlyLessThan(id, playerProof.size());
        playerProof[id] = proof;
    }

    const G1& getPlayerProof(size_t id) const {
        assertStrictlyLessThan(id, playerProof.size());
        return playerProof[id];
    }

    /**
     * Sets the proof for the secret stored at p(0).
     */
    void setZeroProof(const G1& proof) {
        zeroProof = proof;
    }

    const G1& getZeroProof() const {
        return zeroProof;
    }

    /**
     * Stores g^{s - w_N^id}, which is needed to verify proofs for p_j(w_N^id) for player j's polynomial p_j(x)
     */
    void addVerificationHelpers(const G2& g2toId) {
        //loginfo << "VK: " << g2toId << endl;
        this->g2toId = g2toId;
    }

    /**
     * Verifies that p_j(w_N^id) = val, where p_j is committed in polyComm.
     */
    bool verifyAtId(const G1& polyComm, const G1& proof, const Fr& val) const {
        return verifyKateProof(polyComm, proof, val * G1::one(), g2toId);
    }

    /**
     * Verifies that valComm = g^p_j(0), where p_j is committed in polyComm.
     */
    bool verifyAtZero(const G1& polyComm, const G1& proof, const G1& valComm) const {
        return verifyKateProof(polyComm, proof, valComm, kpp.getG2toS()); 
    }

    /**
     * Verifies that valComm = g^p_j(z), where p_j is committed in polyComm and z is committed in acc = g^{s - z}.
     */
    bool verifyKateProof(const G1& polyComm, const G1& proof, const G1& valComm, const G2& acc) const {
        return ReducedPairing(polyComm - valComm, G2::one()) ==
               ReducedPairing(proof, acc);
    }
};

/**
 * Implements a player for Kate et al's DKG.
 */
class KatePlayer : public AbstractKatePlayer<AllConstantSizedProofs, G1, KatePlayer> {
protected:
public:
    KatePlayer(const DkgParams& params, const KatePublicParameters& kpp, size_t id, bool isSimulated, bool isDkgPlayer)
        : AbstractKatePlayer<AllConstantSizedProofs, G1, KatePlayer>(params, kpp, id, isSimulated, isDkgPlayer)
    {
        assertInclusiveRange(0, id, params.n - 1);

        allProofs.reset(new AllConstantSizedProofs(kpp, params.n));

        assertEqual(
            params.getMonomialCommitment(id),
            kpp.getG2toS() - params.omegas[id] * G2::one());
        allProofs->addVerificationHelpers(params.getMonomialCommitment(id));

        proofFinal = G1::zero();
    }
    
public:
    virtual void evaluate() {
        /**
         * We don't need to manually evaluate the polynomial in Kate et al's scheme
         * since we spend O(n^2) computing the proofs anyway, and we get the evals
         * for "free" from there.
         */
    }
    virtual void computeRealProofs() {
        // TODO: all proofs can be multithreaded, but there's no point for now, unless we can multithread the verification too, which we cannot due to libff parallel pairing bug
        // compute proof for f(0)
        allProofs->setZeroProof(std::get<0>(kateProve(Fr::zero())));

        // compute f_id(w_N^j) and its proof for all players j
        shares.resize(params.n);
        for(size_t i = 0; i < params.n; i++) {
            // NOTE: Although this player doesn't verify his own f_id(id) proof, we still need 
            // to compute this proof so we can aggregate it into a proof for the final f(id).
            G1 pi;
            std::tie(pi, shares[i]) = kateProve(params.omegas[i]);
            allProofs->setPlayerProof(i, pi);
        }
    }

    virtual void computeSimulatedProofs() {
        const Fr& s = kpp.getTrapdoor();
        //logdbg << "s = " << s << endl;
        
        // compute p(s)
        Fr pOfS = libfqfft::evaluate_polynomial(f_id.size(), f_id, s);
        assertEqual(comm, pOfS * G1::one());

        // simulate p(0) proof
        allProofs->setZeroProof(
            ( (pOfS - f_id[0])*s.inverse() ) * G1::one());

        // evaluate at all points, so we can simulate proofs
        libpolycrypto::poly_fft(f_id, params.N, shares);
        shares.resize(params.n);

        // simulate proof for player i as g^{(p(s) - p(i))/(s - w_N^i)}
        for(size_t i = 0; i < params.n; i++) {
            allProofs->setPlayerProof(i, 
                ( (pOfS - shares[i])*(s - params.omegas[i]).inverse() ) * G1::one());
        }
    }

    /**
     * NOTE: We verify as e(g^p(s), g) = e(g^q(s), g^(s-i)) e(g,g)^p(i)
     * because computing e(g,g)^p(i) is faster than computing e(g^p(s)/g^p(i), g)
     */
    virtual bool verifySharesReconstruction(const std::vector<size_t>& subset, bool fastTrack) {
        // compute e(g^p(s),g) LHS
        GT lhs = ReducedPairing(comm, G2::one());

        // ...and compute e(g^q(s), g^(s-i)) e(g,g)^p(i) RHS
        //loginfo << "Fast track: " << fastTrack << endl;
        for(size_t i = 0; i < params.n; i++) {
            // if fast track, iterates through 0, ..., n-1
            // otherwise, through subset[0], ..., subset[t-1]
            size_t pid;
            if(fastTrack) {
                if(i >= subset.size())
                    break;

                pid = subset[i];
            } else {
                pid = i;
            }

            //loginfo << "Player " << pid << " is included!" << endl;

            const auto& vk = params.getMonomialCommitment(pid);
            const auto& pi = allProofs->getPlayerProof(pid);
            GT rhs = ReducedPairing(
                pi,
                vk);

            //{
            //    ScopedTimer<microseconds> t(std::cout, "gtmul+exp: ");
            rhs = rhs * (params.gt ^ shares[pid]);
            //}

            if(lhs != rhs) {
                logerror << "KZG proof of player " << pid << " did not verify during reconstruction" << endl;
                loginfo << " - proof: " << pi << endl;
                loginfo << " - VK:    " << vk << endl;
                auto actualVk = kpp.getG2toS() - params.omegas[pid] * G2::one();
                loginfo << " - g2^{s - w_N^" << pid << "}: " << actualVk << endl;
                testAssertEqual(vk, actualVk);
                return false;
            }
        }

        return true;
    }

};

}
