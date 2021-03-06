#ifndef POOL_STRATEGY_H_INCLUDED
#define POOL_STRATEGY_H_INCLUDED

#include <vector>
#include <string>
#include <algorithm>

#include <evolution/evolution_strategy.h>
#include <evolution/evaluation_interface.h>
#include <common/log_messages.h>
#include <common/modules.h>

inline double biased_random_value(double xrand, double bias)
{
    return tanh(bias*xrand)/tanh(bias);
}

inline std::size_t biased_random_index(std::size_t N, double bias)
{
    return floor(biased_random_value(random_value(), bias) * N);
}

inline std::size_t biased_random_index_inv(std::size_t N, double bias)
{
    return N - biased_random_index(N, bias) -1; //N - (0..N-1) - 1
}

/* search the population (sorted-by-fitness) bottom-up and replace
 * the best individual you can get with lower fitness than yours.
 */
std::size_t get_replacement_candidate_for(Individual& opponent, Population& population);

/* generation free */
class Pool_Evolution: public Evolution_Strategy
{
public:
    Pool_Evolution(Population& population,
                   Evaluation_Interface& evaluation,
                   config& configuration,
                   std::size_t max_trials,
                   std::size_t current_trial,
                   double moving_rate,
                   double selection_bias,
                   const std::string& project_folder_path,
                   const bool verbose = true )
    : Evolution_Strategy(population, evaluation, configuration, project_folder_path, verbose)
    , population(population)
    , max_trials(max_trials)
    , current_trial(current_trial)
    , moving_rate(moving_rate)
    , selection_bias(selection_bias)
    , best_individual_has_changed(false)
    , current_playback_idx()
    {
        sts_msg("Created pool evolution strategy.");
        assert(current_trial <= max_trials);
        assert(max_trials > 1);
    }

    ~Pool_Evolution() { sts_msg("Destroyed pool evolution strategy."); }

    bool evaluate(Individual& individual)
    {
        evaluation.constrain(individual.genome);
        if (evaluation.evaluate(individual.fitness, individual.genome, random_value(0.0, 1.0))) {
            sts_add("F=%+1.3f", individual.fitness.get_value_or_default());
            return true;
        } else {
            sts_msg("Evaluation aborted without result.");
            return false;
        }
    }

    bool crossover_trial(void)
    {
        /* select parents from pool and crossover */
        std::size_t parent_1 = biased_random_index_inv(population.get_size(), selection_bias);
        std::size_t parent_2 = biased_random_index_inv(population.get_size(), selection_bias);

        Individual child(population[parent_1], population[parent_2]);
        sts_add("[cross %2u + %2u]", parent_1, parent_2);

        child.mutate();

        if (not evaluate(child)) return false;

        std::size_t replace_idx = get_replacement_candidate_for(child, population);

        if (child.fitness > population[replace_idx].fitness)
        {// if better than the replacement candidate, replace it.
            sts_add("[> %+1.3f] replace %2u", population[replace_idx].fitness.get_value(), replace_idx);
            population[replace_idx] = child;

            best_individual_has_changed |= (replace_idx == 0);

            if (population[replace_idx].fitness.get_number_of_evaluations() == 0)
                err_msg(__FILE__, __LINE__, "overriding a not yet tested one: %u", replace_idx);
        }
        else sts_add("[< %+1.3f] discard", population[replace_idx].fitness.get_value_or_default());

        return true;
    }

    bool refreshing_trial(void)
    {
        std::size_t candidate_idx = random_index(population.get_size());
        Individual candidate(population[candidate_idx]); // select candidate from pool
        sts_add("[refr. %2u (%2u)]", candidate_idx, candidate.fitness.get_number_of_evaluations());
        bool result = evaluate(candidate);

        /* push_back p to population, replace the old one */
        population[candidate_idx] = candidate;

        return result;
    }

    bool initial_trial(void)
    {
        sts_add("[initial trial]");
        assert(current_trial < population.get_size());
        std::size_t candidate_idx = current_trial;
        Individual& candidate(population[candidate_idx]); // select candidates successively from pool
        return evaluate(candidate);
    }

    Evolution_State execute_trial(void)
    {
        bool result = false;
        bool is_initial = current_trial < population.get_size();

        /* prepare */
        if (current_trial % population.get_size() == 0)
            evaluation.prepare_evaluation(current_trial, max_trials);

        sts_add("T: %u", current_trial);
        if (is_initial) {
            result = initial_trial();
        } else if (random_value(0.0, 1.0) > moving_rate) {
            result = crossover_trial();
        } else {
            result = refreshing_trial();
        }
        printf("\n");

        if (not result) return Evolution_State::aborted;

        if (!is_initial)
            update_population_statistics();

        if (++current_trial < max_trials) {
            if (current_trial > 0 and (current_trial % population.get_size() == 0))
                save_state();
            return Evolution_State::running;
        }
        else {
            save_state();
            return Evolution_State::finished;
        }

    }

    Evolution_State playback(void)
    {
        sts_msg("playback individual: %u", current_playback_idx);
        if (not evaluate(population[current_playback_idx]))
            return Evolution_State::aborted;

        update_population_statistics();

        if (++current_playback_idx < population.get_size()) return Evolution_State::playback;
        else return Evolution_State::stopped;
    }

    void resume(void)
    {
        if (current_trial > 0) {
            std::size_t max_trials_old = max_trials;
            max_trials += (current_trial - (current_trial % max_trials));
            if (max_trials > max_trials_old)
                wrn_msg("Max. trials increased from %u to %u.", max_trials_old, max_trials);
            load_state();                 /**TODO consider to move that out of the if clause */
            population.sort_by_fitness(); /**TODO consider to move that out of the if clause */
            sts_msg("Strategy ready to resume.");
        } else
            wrn_msg("Nothing to resume. Skip.");
    }

    void save_config(config& configuration)
    {
        sts_msg("Saving pool strategy settings.");
        configuration.writeUINT("MAX_TRIALS"    , max_trials);
        configuration.writeUINT("CURRENT_TRIAL" , current_trial);
        configuration.writeDBL ("MOVING_RATE"   , moving_rate);
        configuration.writeDBL ("SELECTION_BIAS", selection_bias);
    }

    std::size_t get_max_trials   (void) const { return max_trials;    }
    std::size_t get_current_trial(void) const { return current_trial; }

    const Individual& get_best_individual(void) const
    {
        sts_msg("Get best individual (%1.4f)", population.get_best_individual().fitness.get_value_or_default());
        return population.get_best_individual();
    }

    bool is_there_a_new_best_individual(void) {
        bool result = best_individual_has_changed;
        best_individual_has_changed = false;
        return result;
    }

    /* think about to use 'insertion sort' and keeping this list up-to-date all the time */
    void update_population_statistics(void)
    {
        double last_best_fitness = population.get_best_individual().fitness.get_value();
        population.sort_by_fitness();
        best_individual_has_changed |= (last_best_fitness != population.get_best_individual().fitness.get_value());

        fitness_stats .reset();
        mutation_stats.reset();

        for (std::size_t i = 0; i < population.get_size(); ++i) {

            if (population[i].fitness.get_number_of_evaluations() > 0) {
                fitness_stats .add_sample(population[i].fitness.get_value());
                mutation_stats.add_sample(population[i].mutation_rate);
            } else dbg_msg("not evaluated, skipping %u", i);
        }

        fitness_stats .update_average();
        mutation_stats.update_average();

        if ((current_trial+1) % population.get_size() == 0)
            sts_msg("max:%+1.4f, avg:%+1.4f, min:%+1.4f",
                    fitness_stats.max,
                    fitness_stats.avg,
                    fitness_stats.min);
    }

    Population&  population;
    std::size_t  max_trials;
    std::size_t  current_trial;
    const double moving_rate;
    const double selection_bias;

    bool         best_individual_has_changed;
    std::size_t  current_playback_idx;
};

#endif // POOL_STRATEGY_H_INCLUDED
