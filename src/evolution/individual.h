#ifndef INDIVIDUAL_H_INCLUDED
#define INDIVIDUAL_H_INCLUDED

#include <vector>
#include <cassert>
#include <float.h>
#include <common/modules.h>
#include <common/log_messages.h>

class Individual;

class Fitness_Value
{
public:

    Fitness_Value(void)     : fitness(-DBL_MAX), number_of_evaluations(0) {}
    Fitness_Value(double f) : fitness(f)       , number_of_evaluations(1) {}

    double get_value(void)   const { return fitness; }
    void   set_value(double value) {
        /* average fitness (leaky) */
        if (number_of_evaluations > 0) {
            //dbg_msg("averaging fitness: %1.2f = 0.1 * %1.2f + 0.9 * %1.2f", 0.1*value + 0.9*fitness, value, fitness);
            sts_msg(" averaging fitness");

            fitness = 0.1 * value + 0.9 * fitness;// TODO make a constant
        } else
            fitness = value;
        ++number_of_evaluations;
    }

    void reset(void) {
        fitness = -DBL_MAX;
        number_of_evaluations = 0;
    }
    std::size_t get_number_of_evaluations(void) const { return number_of_evaluations; }

private:
    double      fitness;
    std::size_t number_of_evaluations;

    friend bool operator==(const Fitness_Value& lhs, const Fitness_Value& rhs);
    friend bool operator!=(const Fitness_Value& lhs, const Fitness_Value& rhs);
    friend bool operator< (const Fitness_Value& lhs, const Fitness_Value& rhs);
    friend bool operator> (const Fitness_Value& lhs, const Fitness_Value& rhs);
    friend bool operator<=(const Fitness_Value& lhs, const Fitness_Value& rhs);
    friend bool operator>=(const Fitness_Value& lhs, const Fitness_Value& rhs);
    friend void crossover (const Individual& mother, const Individual& father, Individual& child);
};

inline bool operator==(const Fitness_Value& lhs, const Fitness_Value& rhs) { return lhs.fitness == rhs.fitness; }
inline bool operator!=(const Fitness_Value& lhs, const Fitness_Value& rhs) { return !operator==(lhs,rhs);       }
inline bool operator< (const Fitness_Value& lhs, const Fitness_Value& rhs) { return lhs.fitness < rhs.fitness;  }
inline bool operator> (const Fitness_Value& lhs, const Fitness_Value& rhs) { return  operator< (rhs,lhs);       }
inline bool operator<=(const Fitness_Value& lhs, const Fitness_Value& rhs) { return !operator> (lhs,rhs);       }
inline bool operator>=(const Fitness_Value& lhs, const Fitness_Value& rhs) { return !operator< (lhs,rhs);       }

void crossover(const Individual& mother, const Individual& father, Individual& child);

class Individual
{
public:
    Individual(unsigned int individual_size, double init_mutation_rate, double meta_mutation_rate)
    : genome(individual_size)
    , fitness()
    , mutation_rate(init_mutation_rate)
    , meta_mutation_rate(meta_mutation_rate)
    {
        assert(individual_size    >  0);
        assert(init_mutation_rate > .0);
        assert(meta_mutation_rate > .0);
    }

    ~Individual() {}


    Individual(const Individual& mother, const Individual& father)
    : genome(mother.genome.size())
    , fitness()
    , mutation_rate()
    , meta_mutation_rate()
    {
        crossover(mother, father, *this);
    }

    Individual& operator=(const Individual& other) {
        if (this != &other) // avoid invalid self-assignment
        {
            assert(other.genome.size() == genome.size());
            genome = other.genome;
            fitness = other.fitness;
            mutation_rate = other.mutation_rate;
            meta_mutation_rate = other.meta_mutation_rate;
        }
        return *this;
    }

    void mutate(void);
    void initialize_from_seed(const std::vector<double>& seed); // TODO make a constructor of that
    std::size_t get_size(void) const { return genome.size(); }

    std::vector<double> genome;
    Fitness_Value       fitness;

    double      mutation_rate;
    double meta_mutation_rate;
};

void crossover(const Individual& mother, const Individual& father, Individual& child);

#endif // INDIVIDUAL_H_INCLUDED
