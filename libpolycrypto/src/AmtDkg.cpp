#include <polycrypto/Configuration.h>

#include <polycrypto/AmtDkg.h>

namespace Dkg {

    std::ostream& operator<<(std::ostream& out, const AmtProof& pi) {
        out << "[ ";
        for(auto q : pi.quoComms) {
            out << q << ", ";
        }
        out << "]";

        return out;
    }
}
