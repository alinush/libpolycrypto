#include <fstream>
#include <algorithm>
#include <thread>

#include <boost/functional/hash.hpp>

#include <polycrypto/NtlLib.h>
#include <polycrypto/Configuration.h>
#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>

#include <libff/common/profiling.hpp>
#include <libff/algebra/fields/field_utils.hpp> // get_root_of_unity

#ifdef USE_MULTITHREADING
# include <omp.h>
#endif

using namespace std;
using namespace NTL;

namespace libpolycrypto {

void initialize(unsigned char * randSeed, int size) {
    (void)randSeed; // TODO: initialize entropy source
    (void)size;     // TODO: initialize entropy source

    // Apparently, libff logs some extra info when computing pairings
    libff::inhibit_profiling_info = true;

    // Initializes the default EC curve, so as to avoid "surprises"
    libff::default_ec_pp::init_public_params();

    // Initializes the NTL finite field
    ZZ p = conv<ZZ> ("21888242871839275222246405745257275088548364400416034343698204186575808495617");
    ZZ_p::init(p);

    NTL::SetSeed(randSeed, size);

#ifdef USE_MULTITHREADING
    // NOTE: See https://stackoverflow.com/questions/11095309/openmp-set-num-threads-is-not-working
    loginfo << "Using " << getNumCores() << " threads" << endl;
    omp_set_dynamic(0);     // Explicitly disable dynamic teams
    omp_set_num_threads(static_cast<int>(getNumCores())); // Use 4 threads for all consecutive parallel regions
#else
    //loginfo << "NOT using multithreading" << endl;
#endif
}

vector<Fr> random_field_elems(size_t num) {
    vector<Fr> p(num);
    for (size_t i = 0; i < p.size(); i++) {
        p[i] = Fr::random_element();
    }
    return p;
}

size_t getNumCores() {
    static size_t numCores = std::thread::hardware_concurrency();
    if(numCores == 0)
        throw std::runtime_error("Could not get number of cores");
    return numCores;
}

vector<Fr> get_all_roots_of_unity(size_t n) {
    if(n < 1)
        throw std::runtime_error("Cannot get 0th root-of-unity");

    size_t N = Utils::smallestPowerOfTwoAbove(n);

    // initialize array of roots of unity
    Fr omega = libff::get_root_of_unity<Fr>(N);
    std::vector<Fr> omegas(n);
    omegas[0] = Fr::one();

    if(n > 1) {
        omegas[1] = omega;
        for(size_t i = 2; i < n; i++) {
            omegas[i] = omega * omegas[i-1];
        }
    }

    return omegas;
}

//vector<Fr> random_poly(size_t deg) {
//    return random_field_elems(deg+1);
//}

} // end of namespace libpolycrypto

namespace boost {

std::size_t hash_value(const libpolycrypto::Fr& f)
{
	size_t size;
    mpz_t rop;
    mpz_init(rop);
    f.as_bigint().to_mpz(rop);

    //char *s = mpz_get_str(NULL, 10, rop);
    //size = strlen(s);
    //auto h = boost::hash_range<char*>(s, s + size);

    //void (*freefunc)(void *, size_t);
    //mp_get_memory_functions(NULL, NULL, &freefunc);
    //freefunc(s, size);


    mpz_export(NULL, &size, 1, 1, 1, 0, rop);
	AutoBuf<unsigned char> buf(static_cast<long>(size));

    mpz_export(buf, &size, 1, 1, 1, 0, rop);
    auto h = boost::hash_range<unsigned char*>(buf, buf + buf.size());

    mpz_clear(rop);
	return h;
}

} // end of boost namespace
