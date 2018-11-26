#pragma once

#include <polycrypto/PolyCrypto.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <libfqfft/polynomial_arithmetic/basic_operations.hpp>

#include <xutils/Log.h>
#include <xutils/Utils.h>
#include <xassert/XAssert.h>

#include <polycrypto/AccumulatorTree.h>
#include <polycrypto/KatePublicParameters.h>

using namespace std;
using namespace libfqfft;
using Dkg::KatePublicParameters;
using libpolycrypto::multiExp;

namespace libpolycrypto {

/**
 * Polynomials stored in the multipoint evaluation tree: quotient and remainder.
 */
class EvalPolys {
public:
    std::vector<Fr> quo;
    std::vector<Fr> rem;  // the remainders are not part of the proof, but we need them to compute quotients (and to get the final evaluations)
};

/**
 * Class used to evaluate a polynomial at the first n Nth roots of unity.
 * n is given as a parameter and N = 2^k is the smallest number such that n <= N.
 *
 * TODO: can be multithreaded
 */
class RootsOfUnityEvaluation : public BinaryTree<EvalPolys> {
protected:
    const AccumulatorTree& accs;

public:
    /**
     * Executes an O(n \log{n}) multipoint evaluation of a polynomial f on the first n Nth roots-of-unity points (given in 'accs')
     *
     * @param   poly    the polynomial f being evaluated
     * @param   accs    the points to evaluate f at (wrapped as an AccumulatorTree)
     */
    RootsOfUnityEvaluation(const std::vector<Fr>& poly, const AccumulatorTree& accs)
        : accs(accs)
    {
        size_t n = accs.getNumPoints();
        size_t N = accs.getNumLeaves();
        size_t t = poly.size();
        // it could be that t >> n
        size_t rootLevel = std::min(Utils::log2floor(t-1), accs.getNumLevels() - 1);

        assertStrictlyGreaterThan(n, 1);
        allocateTree(N, rootLevel);

        computeQuotients(poly, rootLevel);
    }

protected:
    void computeQuotients(const std::vector<Fr>& f, size_t rootLevel) {
        assertStrictlyGreaterThan(tree.size(), 0);
        size_t n = accs.getNumPoints();
        size_t numBits = getNumBits();

        // compute the root(s) of the multipoint evaluation tree
        size_t numRoots = tree.back().size();
        for(size_t idx = 0; idx < numRoots; idx++) {
            poly_divide_xnc(
                f,
                accs.getPoly(rootLevel, idx),
                tree[rootLevel][idx].quo,
                tree[rootLevel][idx].rem
            );
        }

        // compute the rest of the tree
        traversePreorder([rootLevel, n, numBits, this](size_t k, size_t idx) {
            // the root is a special case and was computed above
            if(k == rootLevel)
                return;

            // no need to compute quotient/remainder for the ith leaf if bitreverse(i) >= n
            // since we are only evaluating at points \omega^i, i = 0,...,n-1
            if(k == 0 && libff::bitreverse(idx, numBits) >= n)
                return;

            // divides the parent remainder poly by the accumulator of the current node
            poly_divide_xnc(
                tree[k+1][idx / 2].rem,
                accs.getPoly(k, idx),
                tree[k][idx].quo,
                tree[k][idx].rem
            );
        });
    }

public:
    size_t getMaxLevel() const {
        return tree.size() - 1;
    }
    
    size_t getNumBits() const {
        return accs.getNumBits();
    }

    size_t getNumPoints() const {
        return accs.getNumPoints();
    }

    std::vector<Fr> getEvaluations() const {
        std::vector<Fr> values;
        getEvaluations(values);
        return values;
    }

    /**
     * Returns a vector of evaluations: values[i] = f(w_N^i).
     */
    void getEvaluations(std::vector<Fr>& values) const {
        size_t numBits = getNumBits();
        values.resize(getNumPoints());

        //logdbg << "Getting " << values.size() << " evaluations..." << endl;

        for(size_t i = 0; i < values.size(); i++) {
            size_t rev = libff::bitreverse(i, numBits);
            assertValidIndex(rev, tree[0]);
            //logdbg << "Rev of " << i << " is " << rev << endl;

            assertEqual(tree[0][rev].rem.size(), 1);
            values[i] = tree[0][rev].rem[0];
        }
    }

    RootsOfUnityEvaluation& operator+=(const RootsOfUnityEvaluation& rhs) {
        if(accs != rhs.accs) {
            throw std::runtime_error("Can only merge multipoint evaluations if they were done at the same set of points");
        }

        assertEqual(tree.size(), rhs.tree.size());


        for(size_t k = 0; k < tree.size(); k++) {
            for (size_t i = 0; i < tree[k].size(); i++) {
                libfqfft::_polynomial_addition(tree[k][i].quo, tree[k][i].quo, rhs.tree[k][i].quo);
                libfqfft::_polynomial_addition(tree[k][i].rem, tree[k][i].rem, rhs.tree[k][i].rem);
            }
        }

        return *this;
    }

// used for testing purposes only
public:
    /**
     * Tests if the properties between the parent and the children polynomials hold.
     * The child's remainder should be its parent's remainder modulo the child's accumulator.
     * This property is verified by checking if the parent remainder = child accumulator * child quotient + child remainder.
     * (i.e., the polynomial remainder theorem)
     */
    bool _testIsConsistent() const {
        //logdbg << "Checking consistency of multipoint evaluation of " << getNumPoints() << " points (and " << tree.size() << " levels)" << endl;
        
        size_t numRoots = tree.back().size();
        for(size_t idx = 0; idx < numRoots; idx++)
            if( _testIsConsistent(tree.size() - 1, idx) == false)
                return false;

        return true;
    }

    bool _testIsConsistent(size_t k, size_t idx) const {
        //logdbg << "Checking level " << k << ", idx " << idx << endl;
        // reached the bottom level (checked in parent recursive call)
        if(k == 0) {
            return true;
        }

        size_t numBits = getNumBits();
        size_t n = getNumPoints();

        // Check polynomials are correctly computed in the multipoint evaluation tree
        // (e.g., for left child check if acc[k-1][2i]*quo[k-1][2i] + rem[k-1][2i] =?= rem[k][i])
        std::vector<Fr> parent;

        // Check left child, if it's a leaf for a \omega^i point we care about
        if(k - 1 != 0 || libff::bitreverse(2*idx, numBits) < n) {
            libfqfft::_polynomial_multiplication_on_fft(parent,
                accs.getPoly(k - 1, 2*idx).toLibff(),
                tree[k-1][2*idx].quo);

            libfqfft::_polynomial_addition(parent,
                parent,
                tree[k-1][2*idx].rem);

            if(parent != tree[k][idx].rem)
                return false;

            //logdbg << "Left child is consistent at level=" << k-1 << ", idx=" << 2*idx << endl;
        }

        // Check right child, if it's a leaf for a \omega^i point we care about
        if(k - 1 != 0 || libff::bitreverse(2*idx + 1, numBits) < n) {
            libfqfft::_polynomial_multiplication_on_fft(parent,
                accs.getPoly(k - 1, 2*idx + 1).toLibff(),
                tree[k-1][2*idx + 1].quo);

            libfqfft::_polynomial_addition(parent,
                parent,
                tree[k-1][2*idx + 1].rem);

            if(parent != tree[k][idx].rem)
                return false;
            
            //logdbg << "Right child is consistent at level=" << k-1 << ", idx=" << 2*idx+1 << endl;
        }

        bool rightCheck = _testIsConsistent(k-1, 2*idx + 1),
            leftCheck = _testIsConsistent(k-1, 2*idx);

        return leftCheck && rightCheck;
    }
};

/**
 * Authenticated version of the roots-of-unity evaluation tree.
 * All quotient polynomials are committed to.
 */ 
class AuthRootsOfUnityEvaluation : public BinaryTree<G1> {
protected:
    const KatePublicParameters& kpp;

public:
    AuthRootsOfUnityEvaluation(const RootsOfUnityEvaluation& eval, const KatePublicParameters& kpp, bool simulate)
        : kpp(kpp)
    {
        allocateTree(eval.getNumLeaves(), eval.getMaxLevel());
        authenticate(eval, simulate);
    }

protected:
    void authenticate(const RootsOfUnityEvaluation& eval, bool simulate) {
        traversePreorder([eval, simulate, this](size_t k, size_t idx) {
            size_t numBits = eval.getNumBits();
            size_t n = eval.getNumPoints();

            // no need to commit to quotient in the ith leaf if bitreverse(i) >= n
            // since we are only evaluating at points \omega^i, i = 0, ..., n-1
            if(k == 0 && libff::bitreverse(idx, numBits) >= n)
                return;

            auto& quo = eval.tree[k][idx].quo;
            if(simulate) {
                Fr qOfS = libfqfft::evaluate_polynomial(quo.size(), quo, kpp.getTrapdoor());
                tree[k][idx] = qOfS * G1::one();
            } else {
                tree[k][idx] = multiExp<G1>(
                    kpp.g1si.begin(),
                    kpp.g1si.begin() + static_cast<long>(quo.size()),
                    quo.begin(),
                    quo.end());
            }
        });
    }
};

// TODO: abstract the proof itself, if that helps simplify code
//class AuthEvalProof {
//public:
//    const AuthRootsOfUnityEvaluation& authEval;
//    size_t leafIdx;
//
//public:
//    AuthEvalProof(const AuthRootsOfUnityEvaluation& authEval, size_t leafIdx)
//        : authEval(authEval), leafIdx(leafIdx)
//    {
//    }
//
//public:
//    bool verify(const AuthAccumulatorTree& authAccs) {
//        // NOTE: The auth accs might have bigger height than this tree
//        auto numLevels = authEval.height// which is now correct 
//    }
//};

} // end of namespace libpolycrypto
