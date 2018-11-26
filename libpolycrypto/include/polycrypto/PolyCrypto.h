#pragma once

#include <libff/algebra/curves/public_params.hpp>
#include <libff/common/default_types/ec_pp.hpp>
#include <libff/algebra/scalar_multiplication/multiexp.hpp>

#include <xassert/XAssert.h>

using namespace std;

namespace libpolycrypto {

// Type of group G1
using G1 = typename libff::default_ec_pp::G1_type;
// Type of group G2
using G2 = typename libff::default_ec_pp::G2_type;
// Type of group GT (recall pairing e : G1 x G2 -> GT)
using GT = typename libff::default_ec_pp::GT_type;
// Type of the finite field "in the exponent" of the EC group elements
using Fr = typename libff::default_ec_pp::Fp_type;

// Pairing function, takes an element in G1, another in G2 and returns the one in GT
//using libff::default_ec_pp::reduced_pairing;
//using ECPP::reduced_pairing;
// Found this solution on SO: https://stackoverflow.com/questions/9864125/c11-how-to-alias-a-function
// ('using ECPP::reduced_pairing;' doesn't work, even if you expand ECPP)
template<typename ... Args>
auto ReducedPairing(
        Args&&... args) -> decltype(libff::default_ec_pp::reduced_pairing(std::forward<Args>(args)...)) {
    return libff::default_ec_pp::reduced_pairing(std::forward<Args>(args)...);
}

/**
 * Initializes the library, including its randomness.
 */
void initialize(unsigned char * randSeed, int size = 0);

/**
 * Picks num random field elements
 */
vector<Fr> random_field_elems(size_t num);

template<class Group>
vector<Group> random_group_elems(size_t n) {
    vector<Group> bases;
    for(size_t j = 0; j < n; j++) {
        bases.push_back(Group::random_element());
    }
    return bases;
}

size_t getNumCores();

/**
 * Returns the first n Nth roots of unity where N = 2^k is the smallest number such that n <= N.
 */
vector<Fr> get_all_roots_of_unity(size_t n);

/**
 * Picks a random polynomial of degree deg.
 */
//vector<Fr> random_poly(size_t degree);

/**
 * Performs a multi-exponentiation using libfqfft
 */
template<class Group>
Group multiExp(
    typename std::vector<Group>::const_iterator base_begin,
    typename std::vector<Group>::const_iterator base_end,
    typename std::vector<Fr>::const_iterator exp_begin,
    typename std::vector<Fr>::const_iterator exp_end
    )
{
    long sz = base_end - base_begin;
    long expsz = exp_end - exp_begin;
    if(sz != expsz)
        throw std::runtime_error("multiExp needs the same number of bases as exponents");
    //size_t numCores = getNumCores();

    if(sz > 4) {
        if(sz > 16384) {
            return libff::multi_exp<Group, Fr, libff::multi_exp_method_BDLO12>(base_begin, base_end,
                exp_begin, exp_end, 1);//numCores);
        } else {
            return libff::multi_exp<Group, Fr, libff::multi_exp_method_bos_coster>(base_begin, base_end,
                exp_begin, exp_end, 1);//numCores);
        }
    } else {
        return libff::multi_exp<Group, Fr, libff::multi_exp_method_naive>(base_begin, base_end,
            exp_begin, exp_end, 1);
    }
}

/**
 * Performs a multi-exponentiation using libfqfft
 */
template<class Group>
Group multiExp(
    const std::vector<Group>& bases,
    const std::vector<Fr>& exps)
{
    assertEqual(bases.size(), exps.size());
    return multiExp<Group>(bases.cbegin(), bases.cend(),
        exps.cbegin(), exps.cend());
}

} // end of namespace libpolycrypto

namespace boost {

std::size_t hash_value(const libpolycrypto::Fr& f);

} // end of boost namespace
