#include <fstream>
#include <string>

#include <polycrypto/PolyCrypto.h>

#include <xutils/Utils.h>
#include <xutils/Log.h>

using namespace std;
using namespace libpolycrypto;

double dkg_k(size_t lambda, size_t n) {
    return static_cast<double>(lambda + 1) / (log(n)/log(2.0));
}

size_t dkg_ell(double k, size_t n, double epsilon) {
    // TODO: Canny & Sorkin DKG paper doesn't clarify what the log bases are. Assuming e.
    double res = ( (2+k) / ((-1) * log(1.0-2.0*epsilon)) ) * log(n);

    return static_cast<size_t>(ceil(res));
}

double dkg_u(size_t ell, double epsilon) {
    double elld = static_cast<double>(ell);
    double ud = elld / (2*epsilon*epsilon);
    return ud;
}

// the size of a checking group
size_t dkg_group_size(size_t ell, double epsilon) {
    double u = dkg_u(ell, epsilon);
    return static_cast<size_t>(static_cast<double>(ell) + 2.0*(u - 1.0));
}

// max # of malicious parties
size_t dkg_alpha_n(size_t n, double epsilon) {
    return static_cast<size_t>((0.5 - epsilon)*static_cast<double>(n));
}

// min # of parties needed to reconstruct secret
size_t dkg_beta_n(size_t n, double epsilon) {
    return static_cast<size_t>((0.5 + epsilon)*static_cast<double>(n));
}

void printVariables() {
    std::cout << endl;
    std::cout << "lambda  -- the security parameter of the scheme (e.g., 128)" << endl;
    std::cout << "n       -- the total # of players in the DKG protocol" << endl;
    std::cout << "epsilon -- the fraction of players that will be 'moved' from honest to malicious (see 'an' and 'bn' below)" << endl;
    std::cout << endl;
    std::cout << "k   = (lambda + 1) / log_2(n)" << endl;
    std::cout << "ell = [ (2+k) / log(1 - 2*epsilon) ] * log(n)" << endl;
    std::cout << "u   = ell / (2 * epsilon^2)" << endl;
    std::cout << "gs  = ell + 2(u-1)            -- the checking group size" << endl;
    std::cout << "an  = (1/2 - epsilon)n        -- the # of malicious players" << endl;
    std::cout << "bn  = (1/2 + epsilon)n        -- the # of required players to reconstruct" << endl;
    std::cout << "compatible = false if the group size gs > n and true otherwise" << endl;
    std::cout << endl;
}

void printHeader(std::ostream& out) {
    out << "lambda,n,epsilon,k,ell,u,gs,an,bn,compatible" << endl;
}

bool printParams(std::ostream& out, size_t lambda, size_t n, double epsilon) {
    auto k = dkg_k(lambda, n); 
    auto ell = dkg_ell(k, n, epsilon);
    auto u = dkg_u(ell, epsilon);
    auto group_size = dkg_group_size(ell, epsilon);
    auto an = dkg_alpha_n(n, epsilon);
    auto bn = dkg_beta_n(n, epsilon);
    bool compat = group_size <= n;

    // This never happens because (1-(1/2 - epsilon/2) = 1/2 + epsilon/2 is less than the fraction of honest players bn = 1/2 + epsilon.
    // Therefore, even in the worst case when gs = n, it cannot be that there aren't enough honest players in the group.
    //
    //// recall that the probability that a checking group has more than a fraction f = (1/2 - epsilon)/2 of malicious players should be negligible
    //// this means that the other (1-f)*gs players (or more) must be honest with non-negligible probability. 
    //// but this cannot happen unless (1-f)*gs <= bn (where 'bn' = n - 'an' is the total # of honest players).
    //if(compat) {
    //    double hf = 1.0 - (1 - epsilon)/2.0;
    //    auto need_honest = hf * static_cast<double>(group_size);
    //    if(need_honest > bn) {
    //        logwarn << "Group size = " << group_size << " for lambda = " << lambda << ", n = " << n << " and epsilon = " << epsilon << " requires more honest players than available in total." << endl;
    //        logwarn << "i.e., need " << need_honest << " out of " << bn << " total honest" << endl;
    //        compat = false;
    //    }
    //}

    std::ostream* outp = &out;

    if(!compat) {
        outp = &std::cout;
    }

    *outp << lambda << ","
        << n << ","
        << epsilon << ","
        << k << ","
        << ell << ","
        << u << ","
        << group_size << ","
        << an << ","
        << bn << ","
        << (compat ? "true" : "false")
        << endl;

    return compat;
}

bool isEmptyFile(const char * file) {
    std::ifstream fin(file, ios::binary | ios::ate);
    if(fin.fail())
        return true;
    return fin.tellg() == 0;
}

int main(int argc, char *argv[]) {
    libpolycrypto::initialize(nullptr, 0);
    
    if(argc < 3) {
        cout << "Usage: " << argv[0] << " <lambda> <n> [out-file] [epsilon]" << endl;
        cout << endl;
        cout << "Estimates polylog DKG parameters (given the security parameter and # of players)" << endl;
        cout << endl;
        cout << "  <lambda>     desired security parameter" << endl
             << "  <n>          total # of players" << endl
             << endl
             << "OPTIONAL"
             << "  <out-file>   file path to dump (or - for stdout)"
             << "  <epsilon>    1/2 +/- epsilon players needed to reconstruct / can be malicious" << endl
             << endl;
        return 1;
    }

    size_t lambda = static_cast<size_t>(std::stoi(argv[1]));
    size_t n =      static_cast<size_t>(std::stoi(argv[2]));

    std::ostream *out = &std::cout;
    std::ofstream fout;
    bool isEmpty = true;
    if(argc > 3 && std::string(argv[3]) != "-") {
        isEmpty = isEmptyFile(argv[3]);
        loginfo << "File '" << argv[3] << "' is empty: " << isEmpty << endl;
        fout.open(argv[3], std::ofstream::out | std::ofstream::app);
        if(fout.fail()) {
            throw std::runtime_error("Could not open output file for writing");
        }
        out = &fout;
    }

    printVariables();
    if(isEmpty)
        printHeader(*out);

    if(argc > 4) {
        double epsilon = std::stod(argv[4]);
        printParams(*out, lambda, n, epsilon);
    } else {
        std::vector<double> epsilons = { 0.0001, 0.001, 0.01, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.33, 0.40 };
        for(auto epsilon : epsilons) {
            if(!printParams(*out, lambda, n, epsilon)) {
                logerror << " \\-> incompatible parameterization" << endl;
            }
        }
    }

    return 0;
}
