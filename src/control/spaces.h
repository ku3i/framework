#ifndef SPACES_H_INCLUDED
#define SPACES_H_INCLUDED

#include <robots/joint.h>
#include <control/jointcontrol.h>
#include <control/sensorspace.h>

#include <robots/simloid.h>

#include <learning/action_module.h>
#include <learning/payload.h>
#include <learning/sarsa.h>
#include <learning/gmes.h>
#include <learning/competitive_motor_layer.h>
#include <learning/competitive_motor_layer_graphics.h>
#include <learning/learning_machine_interface.h>

/** TODO:
 * Think of having intermediate 'terminal' states, i.e. that switch to a specific policy and
 * giving a binary like reward of 1 when reaching.
 */

namespace control {

class pendulum_sensor_space : public sensor_vector {
public:
    pendulum_sensor_space(const robots::Jointvector_t& joints)
    : sensor_vector(3)
    {
        assert(joints.size() == 1);
        sensors.emplace_back("[1] sin phi" , [&joints](){ return +sin(M_PI * joints[0].s_ang); });
        sensors.emplace_back("[2] cos phi" , [&joints](){ return -cos(M_PI * joints[0].s_ang); });
        sensors.emplace_back("[3] velocity", [&joints](){ return +joints[0].s_vel;             });
    }
};

class pendulum_reward_space : public reward_base
{
public:
    pendulum_reward_space( const GMES&                  gmes
                         , const robots::Jointvector_t& joints )
    : reward_base(3)
    {
        assert(joints.size() == 1);

        /** Cite: >The reward for swinging up is simply given by a measure of the height of the pole. In order to incorporate exploration due
         *  to optimistic value initialization, we chose to give the negative distance of the pole to the horizontal plane
         *  located on the top position.<
         *  Reward should be normed and have a max. value of zero, so optimistic initialization of Q-values is easily zero.
         */
        rewards.emplace_back("Intrinsic learning", [&gmes  ](){ return gmes.get_learning_progress();                                              });
        rewards.emplace_back("Pendulum swing-up" , [&joints](){ return exp(-100*(std::abs(joints[0].s_ang)-1.0)*(std::abs(joints[0].s_ang)-1.0)); });
        rewards.emplace_back("Resting position"  , [&joints](){ return exp(-100*(std::abs(joints[0].s_ang)-0.0)*(std::abs(joints[0].s_ang)-0.0)); });

        /** in principle the swing-up are two tasks, swing-up and balancing. Think of splitting this up and introduce a task planner.
         *  switch to balancing when swing up is finished, on the other hand this introduces the need for terminal conditions.
         */

        sts_msg("Creating %u reward signals for Pendulum.", rewards.size());
    }
};

/* these rewards are probably all wrong!!!
   try using a distance based reward (distance to a virtual goal, e.g. which can hardly be reached)
   or which can be reached and terminates the trial.
   however distance to goal is not available on real machines... so can we accumulate positions...?

   Relation to HER:
   HER (hindsight experience replay)
   move the goal towards the actual reached position.
   moving the robot in different directions within the plane can be considered as moving to a specific location.
   if the action was taken, select the respective reward function of subpolicy that would maximize learning.
   but we already learn everything simultaneously. so this is a generalization of HER?

   try move over from discrete separate goals to a goal vector.. e.g. for the walking robot...simply the direction in 2D or 3D space to move!


 */
class walking_reward_space : public reward_base
{
public:
    walking_reward_space(robots::Simloid const& robot)
    : reward_base(16)
    {
        /* add dummies for intrinsic learning, until state and motor layers are constructed */
        rewards.emplace_back("Intrinsic Motivation"   , [](){ return .0; } );

        switch(robot.robot_ID){
        case 10: /* Tadpole */
            rewards.emplace_back("walking forwards"       , [&robot](){ return +robot.get_avg_velocity_forward()/* - std::abs(robot.get_avg_velocity_left()) - std::abs(robot.get_avg_rotational_speed()) )/(1. + robot.get_normalized_mechanical_power())*/;   });
            rewards.emplace_back("walking backwards"      , [&robot](){ return -robot.get_avg_velocity_forward()/* - std::abs(robot.get_avg_velocity_left()) - std::abs(robot.get_avg_rotational_speed()) )/(1. + robot.get_normalized_mechanical_power())*/;   });
            rewards.emplace_back("turning left"           , [&robot](){ return +0.1*robot.get_avg_rotational_speed()/*/(1. + robot.get_normalized_mechanical_power())*/;   });
            rewards.emplace_back("turning right"          , [&robot](){ return -0.1*robot.get_avg_rotational_speed()/*/(1. + robot.get_normalized_mechanical_power())*/;   });

            break;
        case 31: /* Fourlegged */
            rewards.emplace_back("walking forwards"       , [&robot](){ return +robot.get_avg_velocity_forward();    });
            //rewards.emplace_back("walking backwards"      , [&robot](){ return -robot.get_avg_velocity_forward()     });
            //rewards.emplace_back("turning left"           , [&robot](){ return +0.1*robot.get_avg_rotational_speed() });
            //rewards.emplace_back("turning right"          , [&robot](){ return -0.1*robot.get_avg_rotational_speed() });
            //rewards.emplace_back("stopping"               , [&robot](){ return -1.0/(1. + robot.get_normalized_mechanical_power())*/;   });
            //rewards.emplace_back("get_up"                 , [&robot](){ return +robot.get_avg_position().z - robot.get_accels()[0].v.y; });
            //rewards.emplace_back("walking left"           , [&robot](){ return +robot.get_avg_velocity_left();       });
            //rewards.emplace_back("walking right"          , [&robot](){ return -robot.get_avg_velocity_left();       });
            break;

        case 38: /* Hannah */
            rewards.emplace_back("walking forwards"       , [&robot](){ return +robot.get_avg_velocity_forward();    });
            sts_msg("Reward for Hannah.");
            break;
        default:
            break;
        }
        /** TODO: consider a penalty for dropping in the reward function.
         *  However this should only apply for the walking robots, not the crawlers.
         *
         *  TODO: make a policy for stopping, use simloid.is_motion_stopped? and minimize ctrl output.
         */
        sts_msg("Creating %u reward signals for walking." , rewards.size());
    }

    void add_intrinsic_rewards( learning::Learning_Machine_Interface const& state_learner
                              , learning::Learning_Machine_Interface const& motor_learner )
    {
        assert(rewards.size() >= 2);
        rewards[0] = { "Intrinsic Motivation"
                     , [&state_learner ,&motor_learner]() { return state_learner.get_learning_progress()
                                                                 + motor_learner.get_learning_progress(); }
                     };

    }
};


/**TODO remove*/
class self_adjusting_motor_space : public Action_Module_Interface
{
    control::Jointcontrol              control;

    std::size_t                        applied_policy;
    std::size_t                        applied_action;
    std::size_t                        applied_state;

    const learning::RL_Interface&      learner;
    const bool                         self_adjusting;
    CompetitiveMotorLayer              motor_layer;

public:

    self_adjusting_motor_space( robots::Robot_Interface&       robot
                              , const Control_Vector&          parameter_set
                              , const std::size_t              max_actions
                              , const std::size_t              num_actions_begin
                              , static_vector<State_Payload>&  state_payload
                              , const learning::RL_Interface&  learner
                              , bool                           self_adjusting
                              , const double                   mutation_rate
                              , const double                   learning_rate
                              , const control::Minimal_Seed_t& seed )
    : control(robot)
    , applied_policy(0)
    , applied_action(0)
    , applied_state(0)
    , learner(learner)
    , self_adjusting(self_adjusting)
    , motor_layer(robot, state_payload, parameter_set, max_actions, num_actions_begin, mutation_rate, learning_rate, self_adjusting, seed)
    {
        dbg_msg("Creating self adjusting motor space.");
        control.set_control_parameter(motor_layer.get_unit(0).weights); // initialize non-mutated start controller
        control.print_parameter();
        control.reset();
    }

    void execute_cycle(bool state_has_changed)
    {
        if (state_has_changed) { /* apply action learning on state change only */
            /* adjust previously selected weights*/
            motor_layer.adapt(learner.positive_current_delta(applied_policy));

            /* update and check state + action from learner */
            applied_policy = learner.get_current_policy();
            applied_action = learner.get_current_action();
            applied_state  = learner.get_current_state();

            if (self_adjusting)
                motor_layer.enable_adaption(applied_policy == 0); /**TODO use enums for policies*/
            motor_layer.create_mutated_weights(applied_action);

            /* apply new weights */
            control.set_control_parameter(motor_layer.get_mutated_weights());
        }
        control.execute_cycle();
    }

    std::size_t get_number_of_actions          (void) const { return motor_layer.get_number_of_motor_units();           }
    std::size_t get_number_of_actions_available(void) const { return motor_layer.get_number_of_available_motor_units(); }

    bool exists(const std::size_t action_index) const { return motor_layer.exists(action_index); }

    friend class self_adjusting_motor_space_graphics;
};

#include <draw/draw.h>

/**TODO refactor and move to gmes_action_module*/
class self_adjusting_motor_space_graphics : public Graphics_Interface {
    const self_adjusting_motor_space& space;
    CompetitiveMotorLayer_Graphics    motor_layer_graphics;
public:
    self_adjusting_motor_space_graphics(const self_adjusting_motor_space& space)
    : space(space)
    , motor_layer_graphics(space.motor_layer, -1.0, 0.0, 0.5) {}

    void execute_cycle(bool state_has_changed) {
        if (state_has_changed)
            motor_layer_graphics.execute_cycle();
    }
    void draw(const pref& p) const
    {
        motor_layer_graphics.draw(p);

        const MotorUnit& motor = space.motor_layer.get_unit(space.applied_action);
        glColor3f(1.0, 1.0, 1.0);
        glprintf(-0.9, 0.65, 0.0, 0.03, "%3u:%2u"
                                      , space.applied_state
                                      , space.applied_action );

        draw_vector2(-0.9, 0.6, 0.05, 1.0, motor.weights.get_parameter(), 3.0);
    }
};

/**GMES spaces */
class GMES_joint_space : public sensor_vector
{
public:
    GMES_joint_space(const robots::Joint_Model& joint)
    : sensor_vector(3)
    {
        sensors.emplace_back("[1] angle"    , [&joint](){ return joint.s_ang;       });
        sensors.emplace_back("[2] velocity" , [&joint](){ return joint.s_vel;       });
        sensors.emplace_back("[3] torque"   , [&joint](){ return joint.motor.get(); }); //don't even think of removing that
        /**TODO: Think about: the introduction of the motor signal in the sensor space makes that inherently instable.
         * The adaption can skip the world in the loop and can directly influence the sensor space without actually moving the robot.
         * Also, reducing sensor space dimension reduces the overall cost. */
    }
};

} // namespace control


#endif // SPACES_H_INCLUDED
