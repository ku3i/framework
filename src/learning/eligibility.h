#ifndef ELIGIBILITY_H_INCLUDED
#define ELIGIBILITY_H_INCLUDED

#include <cassert>
#include <iostream>
#include <learning/sarsa_constants.h>

/** TODO: visualize traces by displaying the max trace per state.
 */
class Eligibility
{
    double value;

public:
    Eligibility()
    : value(0.0)
    {}

    void decay(void)       { value *= sarsa_constants::LAMBDA * sarsa_constants::GAMMA; }
    void reset(void)       { value = 1.0;  }
    double get(void) const { assert(in_range(value, 0.0, 1.0)); return value; }

    friend std::ostream& operator<< (std::ostream& os, const Eligibility& e);
};

inline std::ostream& operator<< (std::ostream& os, const Eligibility& e) {
    os << e.value;
    return os;
}


#endif // ELIGIBILITY_H_INCLUDED
