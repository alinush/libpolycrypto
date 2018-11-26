#include <polycrypto/PolyOps.h>
#include <polycrypto/PolyCrypto.h>
#include <polycrypto/RootsOfUnityEval.h>

#include <vector>
#include <cmath>
#include <iostream>
#include <ctime>
#include <fstream>

#include <libfqfft/polynomial_arithmetic/basic_operations.hpp>
#include <libfqfft/polynomial_arithmetic/naive_evaluate.hpp>
#include <libff/common/double.hpp>

#include <xutils/Log.h>
#include <xassert/XAssert.h>

using namespace std;
using namespace libfqfft;
using namespace libpolycrypto;

void assertEvals(const std::vector<Fr>& f, const std::vector<Fr>& evals, size_t numPoints);

void testRootsOfUnityEvaluation(size_t deg, const AccumulatorTree& accs) {
    size_t numPoints = accs.getNumPoints();

    std::vector<Fr> f = random_field_elems(deg+1);

    RootsOfUnityEvaluation eval(f, accs);

    testAssertTrue(eval._testIsConsistent());

    std::vector<Fr> evals = eval.getEvaluations();
    assertEvals(f, evals, numPoints);
}

void testMultipointMerging(size_t deg, const AccumulatorTree& accs) {
    size_t numPoints = accs.getNumPoints();

    std::vector<Fr> f1, f2, f3;
    f1 = random_field_elems(deg+1);
    f2 = random_field_elems(deg+1);

    libfqfft::_polynomial_addition(f3, f1, f2);

    // evaluate f1 and f2
    RootsOfUnityEvaluation eval1(f1, accs);
    RootsOfUnityEvaluation eval2(f2, accs);

    testAssertTrue(eval1._testIsConsistent());
    testAssertTrue(eval2._testIsConsistent());
    assertEvals(f1, eval1.getEvaluations(), numPoints);
    assertEvals(f2, eval2.getEvaluations(), numPoints);

    // merge the evaluation of f1(x) and f2(x) to get the evaluation tree of their sum (f1+f2)(x)
    eval1 += eval2;

    testAssertTrue(eval1._testIsConsistent());
    assertEvals(f3, eval1.getEvaluations(), numPoints);
}

int main() {
    libpolycrypto::initialize(nullptr, 0);

    for (size_t numPoints = 2; numPoints <= 32; numPoints++) {
        for (size_t deg = 1; deg <= 32; deg++) {
            loginfo << "Evaluating degree " << deg << " polynomials at the first " << numPoints << " roots of unity" << endl;
            AccumulatorTree accs(numPoints);
            testRootsOfUnityEvaluation(deg, accs);
            testMultipointMerging(deg, accs);
        }
    }

    loginfo << "Test passed!" << endl;

    return 0;
}
    
void assertEvals(const std::vector<Fr>& f, const std::vector<Fr>& evals, size_t numPoints) {
    std::vector<Fr> actual;

    auto omegas = get_all_roots_of_unity(numPoints);
    for(size_t i = 0; i < numPoints; i++) {
        Fr v = libfqfft::evaluate_polynomial(f.size(), f, omegas[i]);
        actual.push_back(v);
    }

    for(size_t i = 0; i < evals.size(); i++) {
        if(evals[i] != actual[i]) {
            logdbg << "The multipoint evaluation function returned a different f(\\omega^"
                   << i << ")!" << endl;
            logdbg << " * Eval:   " << evals[i] << endl;
            logdbg << " * Actual: " << actual[i] << endl;
            throw std::runtime_error("The multipoint evaluation function is wrong!");
        } else {
            //logdbg << "At point w_n^" << i << " = " << evals[i] << endl;
        }
    }
}
