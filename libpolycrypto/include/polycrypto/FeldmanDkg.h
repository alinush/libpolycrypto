#pragma once

#include <memory>

#include <polycrypto/PolyCrypto.h>
#include <polycrypto/DkgCommon.h>
#include <polycrypto/AbstractPlayer.h>

#include <xutils/Log.h>
#include <xutils/Timer.h>

namespace Dkg {

using libpolycrypto::Fr;
using libpolycrypto::G1;
using libpolycrypto::multiExp;

class FeldmanPublicParameters {
public:
    const DkgParams& params;

public:
    FeldmanPublicParameters(const DkgParams& params)
        : params(params)
    {
    }

public:
    void getPlayerExps(size_t id, std::vector<Fr>& exps) const {
        assertStrictlyLessThan(id, params.n);

        exps.clear();
        exps.push_back(Fr::one());
        size_t k = id;
        const auto& omegas = params.omegas;

        // the degree of the polynomial is t-1
        for(size_t j = 1; j < params.t; j++) {
            exps.push_back(omegas[k]);

            k = (k + id) % params.N;
        }

        assertEqual(exps.size(), params.t);

        //logdbg << "w_n^i's for player " << id << ": ";
        //for(auto w : exps) {
        //    // identify if the exp is a root of unity
        //    auto it = std::find(omegas.begin(), omegas.end(), w);
        //    if(it == omegas.end()) {
        //        throw std::runtime_error("This is not what I expected");
        //    }

        //    size_t pos = static_cast<size_t>(it - omegas.begin());
        //    std::cout << pos << ", ";
        //}
        //std::cout << endl;
    }
};

class FeldmanPlayer : public AbstractPlayer {
public:
    const FeldmanPublicParameters& fpp;
    std::vector<G1> comm;       // commitment to f_id(.) of degree t-1 and t coefficients
    std::vector<G1> commFinal;  // commitment to the final polynomial f(x) = \sum_{j \in IDs} f_j(x)
    size_t numBits;             // the number of bits of all (w_n^id)^k exponents

public:
    /**
     * A player in a Feldman DKG.
     */
    FeldmanPlayer(const DkgParams& params, const FeldmanPublicParameters& fpp, size_t id, bool isSimulated, bool isDkgPlayer)
        : AbstractPlayer(params, id, isSimulated, isDkgPlayer), fpp(fpp), commFinal(params.t), numBits(0)
    {
        assertInclusiveRange(0, id, params.n - 1);

        // get ((w_N^id)^k)_{k=0}^{t-1} exponents to verify the shares against the Feldman commitment
        std::vector<Fr> exps;
        fpp.getPlayerExps(id, exps);
        for(size_t i = 0; i <= params.t - 1; i++) {
            numBits += exps[i].as_bigint().num_bits();
        }
    }

public:
    /**
     * Evaluates f_id(x) at the first n Nth roots of unity, computing the shares
     * for each player j.
     */
    virtual void evaluate() {
        libpolycrypto::poly_fft(f_id, params.N, shares);
        shares.resize(params.n);
    }

    /**
     * Feldman commits to the polynomial f_id(x).
     */
    void dealImpl() {
        // NOTE: shares[i] are already precomputed in AbstractPlayer by now

        // commit to polynomial using Feldman commitments
        // i.e., given c_i's, return g^{c_i}'s
        comm.resize(f_id.size());
#ifdef USE_MULTITHREADING
#pragma omp parallel for
#endif
        for(size_t i = 0; i < f_id.size(); i++) {
            comm[i] = f_id[i] * G1::one();
        }
    }
    
    /**
     * Picks Feldman commitment randomly.
     */
    virtual void simulatedDealImpl() {
        dealImpl();
    }

    /**
     * Verifies this player j's f_i(j) share against f_i(x), for all i
     */
    bool verifySharesIndividually(const std::vector<AbstractPlayer*>& allPlayers) {
        std::vector<Fr> exps;
        for(size_t i = 0; i < allPlayers.size(); i++) {
            const FeldmanPlayer& p = *dynamic_cast<FeldmanPlayer*>(allPlayers[i]);

            // do not verify your own share, when used as DKG
            if(isDkgPlayer && p.id == id) {
                continue;
            }

            // verify your share of f_i(x) against player i's commitment of f_i(x)
            assertNotEqual(p.shares[id], Fr::zero());

            G1 shareVk = p.shares[id] * G1::one();
            fpp.getPlayerExps(id, exps);
            G1 result = multiExp<G1>(p.comm, exps);

            if(shareVk != result) {
                logerror << "The share from player #" << p.id << " did not verify against the Feldman commitment" << endl;
                return false;
            }
        }

        return true;
    }

    /**
     * Verifies each player i's share against the final polynomial f(x). 
     */
    bool verifySharesWorstCaseReconstruction() {
        std::vector<Fr> exps;
        for(size_t pid = 0; pid < params.n; pid++) {
            // verify player i's share against the final commitment of f(x)
            G1 shareVk = shares[pid] * G1::one();
            fpp.getPlayerExps(pid, exps);
            G1 result = multiExp<G1>(comm, exps);

            if(shareVk != result) {
                logerror << "The share from player #" << pid << " did not verify against the Feldman commitment" << endl;
                return false;
            }
        }
        
        return true;
    }

    /**
     * PERF: The performance of this call might depend on the signer ID of the current player.
     * This is because the exponents used to verify the share might be smaller when the signer ID
     * is smaller. When using roots of unity, this will only be true for player 0 with ID w_N^0 = 1.
     * Even so, we still notice that exponentiation by roots of unity is much faster than by random exponents.
     * See bench/BenchMultiexp.cpp
     */
    virtual bool verifyOtherPlayers(const std::vector<AbstractPlayer*>& allPlayers, bool aggregate) {
        pk = G1::zero();
        share = Fr::zero();
        commFinal.clear();
        commFinal.resize(params.t, G1::zero());

        // compute your final share f(id) of f(x) = \sum_{i \in IDs} f_i(x), the final public key g^{f(0)} and final commitment
        if(isDkgPlayer) {
            for(size_t i = 0; i < allPlayers.size(); i++) {
                const FeldmanPlayer& p = *dynamic_cast<FeldmanPlayer*>(allPlayers[i]);
                share = share + p.shares[id];
                pk = pk + p.comm[0];
                for(size_t j = 0; j < f_id.size(); j++) {
                    commFinal[j] = commFinal[j] + p.comm[j];
                }
            }
        } else {
            assertEqual(allPlayers.size(), 1);  // VSS players only verify the dealer
        }

        // verify share from each player i individually, or verify aggregate shares
        if(aggregate) {
            if(isDkgPlayer) {
                return verifyFinalShareProof();
            } else {
                throw std::runtime_error("What are you aggregating for if you're doing VSS?");
            }
        } else {
            return verifySharesIndividually(allPlayers);
        }
    }

    virtual bool reconstructionVerify(const std::vector<size_t>& subset, bool fastTrack) {
        (void)subset;
        assertEqual(subset.size(), params.t);

        if(!fastTrack) {
            // verify all $n$ shares manually to simulate finding good subset of shares
            return verifySharesWorstCaseReconstruction();
        }

        // Feldman fast-track requires no verification
        return true;
    }

    /**
     * NOTE: Should only be called after verifyOtherPlayers has been called on all players.
     *
     * Verifies this player's share of the final agreed-upon secret f(0) against the final commitment to the polynomial f(x) \sum_{j \in IDs} f_j(x)
     */
    virtual bool verifyFinalShareProof() {
        assertNotEqual(share, Fr::zero());

        std::vector<Fr> exps;
        G1 shareVk = share * G1::one();
        fpp.getPlayerExps(id, exps);
        G1 result = multiExp<G1>(commFinal, exps);

        if(shareVk != result) { 
            logerror << "Feldman player " << id << "'s aggregated share did not verify against the aggregated Feldman commitment" << endl;
            return false;
        }
        return true;
    }
};

}
