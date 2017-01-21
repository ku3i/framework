#ifndef MOTOR_PREDICTOR_H_INCLUDED
#define MOTOR_PREDICTOR_H_INCLUDED

#include <common/log_messages.h>
#include <robots/robot.h>
#include <control/controlparameter.h>
#include <control/jointcontrol.h>
#include <control/control_core.h>
#include <learning/predictor.h>

class Motor_Predictor : public Predictor_Base {
public:
    Motor_Predictor( const robots::Robot_Interface& robot
                   , const sensor_vector& motor_targets
                   , const double learning_rate
                   , const double random_weight_range
                   , const std::size_t experience_size
                   , const control::Control_Parameter& parameter )
    : Predictor_Base(motor_targets, learning_rate, random_weight_range, experience_size)
    , robot(robot)
    , core(robot)
    , params(control::make_asymmetric(robot, parameter))
    , motor_targets(motor_targets)
    {
        //dbg_msg("Creating motor predictor.");
        core.apply_weights(robot, params.get_parameter());
        assert(experience_size > 0 && "not allowed.");

        if (experience_size != 1)
            wrn_msg("Experience replay not implemented yet: %lu", experience_size);
        /** TODO how to initialize experience? */
    }

    void copy(Predictor_Base const& other) override {
        Predictor_Base::operator=(other); // copy base members
        Motor_Predictor const& rhs = dynamic_cast<Motor_Predictor const&>(other); /**TODO definitely write a test for that crap :) */
        core = rhs.core;
    }

    double predict(void) {
        //dbg_msg("Motor predictor: predicts.");
        core.prepare_inputs(robot);
        core.update_outputs(robot, params.is_symmetric(), params.is_mirrored());
        return calculate_prediction_error(core.activation);
    }

    void adapt(void) {
        /**TODO regarding the gradient descent on the motor controller weights:
         * + do we handle the non-smooth transfer function (clip), or do we assume linearity?
         * + do we handle the 'recurrent' motor connections as just ordinary inputs?
         */

        if (experience.size() == 1) {
            learn_from_sample();
        } else {
            assert(false && "Not implemented yet.");
        }
    }


    void initialize_randomized(void) override { /**TODO implement*/

        for (auto& w_k : core.weights)
            for (auto& w_ik : w_k)
                w_ik = random_value(-random_weight_range,+random_weight_range);

        /** TODO how to initialize experience?
         *  + empty?
         *  + random?
         */
        //experience.assign(experience.size(), weights);

        prediction_error = predictor_constants::error_min;

    }


    void initialize_from_input(void) override { assert(false && "one shot learning not implemented yet."); }

    VectorN const& get_weights   (void) const override { /**TODO use weights*/ return params.get_parameter(); }
    VectorN const& get_prediction(void) const override { return core.activation; }

private:
    robots::Robot_Interface const&          robot;
    control::Fully_Connected_Symmetric_Core core;
    control::Control_Parameter              params; // currently just for loading

    sensor_vector const& motor_targets;


    void learn_from_sample(void) {
        auto const& inputs = core.input;
        VectorN const& predictions = core.activation;
        std::vector<VectorN>& weights = core.weights; // non const ref
        assert(motor_targets.size() == predictions.size());
        assert(motor_targets.size() == weights.size());
        assert(inputs.size() == weights.at(0).size());

        for (std::size_t k = 0; k < motor_targets.size(); ++k) { // for num of motor outputs
            double err = motor_targets[k] - predictions[k];
            for (std::size_t i = 0; i < weights[k].size(); ++i) { // for num of inputs
                weights[k][i] += learning_rate * err * inputs[i].x;
            }
        }
    }
};


#endif // MOTOR_PREDICTOR_H_INCLUDED