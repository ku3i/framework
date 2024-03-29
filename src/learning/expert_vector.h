#ifndef EXPERT_VECTOR_H_INCLUDED
#define EXPERT_VECTOR_H_INCLUDED

#include <memory>
#include <common/static_vector.h>
#include <common/save_load.h>
#include <control/sensorspace.h>
#include <control/control_vector.h>
#include <robots/robot.h>
#include <learning/expert.h>
#include <learning/predictor.h>
#include <learning/state_predictor.h>
#include <learning/motor_predictor.h>
#include <learning/state_action_predictor.h>
#include <learning/homeokinetic_predictor.h>
#include <learning/bimodel_predictor.h>

/* The Expert Vector merely work as a container
 * and should neither carry any information nor functionality
 * regarding the expert modules in it. However this is theory. :)
 */

class Expert_Vector : public common::Save_Load {

    std::vector<Expert> experts;
    static_vector_interface& payloads;

    Expert_Vector( const std::size_t max_number_of_experts
                 , static_vector_interface& payloads )
    : experts()
    , payloads(payloads)
    {
        assert(payloads.size() == max_number_of_experts);
        assert(max_number_of_experts > 0);
        experts.reserve(max_number_of_experts);
    }

public:
    Expert_Vector(Expert_Vector&& other) = default;
    Expert_Vector& operator=(Expert_Vector&& other) = default;

          Expert& operator[] (const std::size_t index)       { return experts.at(index); }
    const Expert& operator[] (const std::size_t index) const { return experts.at(index); }

    std::size_t size(void) const { return experts.size(); }

    void save(std::string f)
    {
        auto const cols = experts.at(0).get_predictor().get_weights().size();
        csv_file_t csv(f+"experts.dat", experts.size(), cols);
        for (std::size_t i = 0, line = 0; i < experts.size(); ++i) {
            if (experts[i].does_exists())
                csv.set_line(line++, experts[i].get_predictor().get_weights());
        }
        csv.write();
    }

    void load(std::string f)
    {
        auto const cols = experts.at(0).get_predictor().get_weights().size();
        csv_file_t csv(f+"experts.dat", experts.size(), cols);
        csv.read();
        for (std::size_t i = 0; i < experts.size(); ++i) {
            csv.get_line(i, experts[i].set_predictor().set_weights());
            experts[i].exists = !(i > 0 && is_vector_zero(experts[i].get_predictor().get_weights()));
        }
    }


    void copy(std::size_t to, std::size_t from, bool one_shot_learning) {

        experts.at(to).exists = true; // create

        if (one_shot_learning) experts.at(to).reinit_predictor_weights();
        else
            experts.at(to).predictor->copy( *(experts.at(from).predictor) );

        payloads.copy(to, from); /* take a flawed copy of the payload */
    }

    /* simple sensor state space constructor */
    Expert_Vector( const std::size_t         max_number_of_experts
                 , static_vector_interface&  payloads
                 , const sensor_vector&      input
                 , const double              local_learning_rate
                 , const double              random_weight_range
                 , const std::size_t         experience_size )
    : Expert_Vector(max_number_of_experts, payloads)
    {
        assert(local_learning_rate > 0.);
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back( Predictor_ptr( new Predictor(input, local_learning_rate, random_weight_range, experience_size) )
                                , max_number_of_experts );
    }

    /* time-delay network sensor state space constructor */
    Expert_Vector( const std::size_t         max_number_of_experts
                 , static_vector_interface&  payloads
                 , const sensor_vector&      input
                 , const double              local_learning_rate
                 , const std::size_t         experience_size
                 , const std::size_t         hidden_layer_size
                 , const std::size_t         time_delay_size )
    : Expert_Vector(max_number_of_experts, payloads)
    {
        assert(local_learning_rate > 0.);
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back( Predictor_ptr( new learning::State_Predictor(input, local_learning_rate, gmes_constants::random_weight_range, experience_size, hidden_layer_size, time_delay_size) )
                                , max_number_of_experts );
    }

    /* motor action space constructor */
    Expert_Vector( std::size_t              max_number_of_experts
                 , static_vector_interface& payloads
                 , sensor_vector const&     motor_targets
                 , double                   local_learning_rate
                 , std::size_t              experience_size
                 , double                   noise_level
                 , control::Control_Vector const& ctrl_params
                 , robots::Robot_Interface const& robot )
    : Expert_Vector(max_number_of_experts, payloads)
    {
        sts_msg("Creating motor expert vector with %u elements in control parameter vector.", ctrl_params.size());
        assert(local_learning_rate > 0.);
        assert(ctrl_params.size() == max_number_of_experts);
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back( Predictor_ptr( new learning::Motor_Predictor(robot, motor_targets, local_learning_rate, gmes_constants::random_weight_range, experience_size, ctrl_params.get(i), noise_level))
                                , max_number_of_experts );
    }

    /* state action space constructor */
    Expert_Vector( const std::size_t         max_number_of_experts
                 , static_vector_interface&  payloads
                 , const time_embedded_sensors<16>&      input
                 , const double              local_learning_rate
                 , const std::size_t         experience_size
                 , const std::size_t         hidden_layer_size // ergibt sich aus num joints
                 , const double              random_weight_range
                 )
    : Expert_Vector(max_number_of_experts, payloads)
    {
        assert(local_learning_rate > 0.);
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back( Predictor_ptr( new learning::State_Action_Predictor( input
                                                                                     , local_learning_rate
                                                                                     , random_weight_range
                                                                                     , experience_size
                                                                                     , hidden_layer_size
                                                                                     ) )
                                , max_number_of_experts );
    }



    /* homeokinetic expert constructor */
    Expert_Vector( std::size_t                   max_number_of_experts
                 , static_vector_interface&      payloads
                 , sensor_input_interface const& input
                 , std::size_t                   number_of_motor_outputs
                 , std::size_t                   number_of_context_units
                 , double                        local_learning_rate
                 , double                        random_weight_range
                 )
    : Expert_Vector(max_number_of_experts, payloads)
    {
        assert(local_learning_rate > 0.);
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back( Predictor_ptr( new learning::Homeokinetic_Core( input
                                                                                , number_of_motor_outputs
                                                                                , local_learning_rate
                                                                                , random_weight_range
                                                                                , number_of_context_units
                                                                                ) )
                                , max_number_of_experts );
    }


    /* bimodel expert constructor */
    Expert_Vector( std::size_t                   max_number_of_experts
                 , static_vector_interface&      payloads
                 , sensor_input_interface const& input
                 , sensor_input_interface const& gateway
                 , learning::model::vector_t&    gradient
                 , double                        local_learning_rate
                 , double                        random_weight_range
                 , double                        regularization_rate
                 )
    : Expert_Vector(max_number_of_experts, payloads)
    {
        assert(local_learning_rate > 0.);
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back(
                                 Predictor_ptr( new learning::BiModel_Predictor( input
                                                                               , gateway
                                                                               , gradient
                                                                               , local_learning_rate
                                                                               , random_weight_range
                                                                               , regularization_rate
                                                                               ) )
                                , max_number_of_experts );
    }

};

/* possible template constructor

template <typename PredictorType>
class Expert_Vector : public Expert_Vector_Base {
public:

    template<typename... Args>
    Expert_Vector( const std::size_t         max_number_of_experts
                 , static_vector_interface&  payloads
                 , const Args&...            predictor_args)
    : Expert_Vector_Base(max_number_of_experts, payloads)
    {
        for (std::size_t i = 0; i < max_number_of_experts; ++i)
            experts.emplace_back( Predictor_ptr(new PredictorType(predictor_args...)), max_number_of_experts );
    }
};

*/

#endif // EXPERT_VECTOR_H_INCLUDED
