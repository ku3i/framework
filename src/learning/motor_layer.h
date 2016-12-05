#ifndef MOTOR_LAYER_H_INCLUDED
#define MOTOR_LAYER_H_INCLUDED

#include <robots/robot.h>
#include <robots/joint.h>
#include <control/jointcontrol.h>
#include <control/control_vector.h>
#include <control/sensorspace.h>
#include <learning/expert.h>
#include <learning/gmes.h>
#include <learning/payload.h>

/** WHAT DO WE NEED?

predictor_base
expert_base
gmes_base


robot
joint_control


*/

namespace learning {

namespace constants {
    const double number_of_experts    = 20;
    const double local_learning_rate  = 0.01;
    const double gmes_learning_rate   = 10.0;
    const std::size_t experience_size = 100;
}


class Motor_Space : public sensor_vector {
public:
    Motor_Space(const robots::Jointvector_t& joints)
    : sensor_vector(joints.size())
    {
        for (robots::Joint_Model const& j : joints)
            sensors.emplace_back(j.name, [&j](){ return j.motor; });
    }
};

/** set up motor space as "sensor_space&" for experts */

class Motor_Layer {
public:
    Motor_Layer( robots::Robot_Interface& robot, std::size_t max_num_motor_experts = constants::number_of_experts )
    : max_num_motor_experts(max_num_motor_experts)
    , control(robot)
    , params(max_num_motor_experts)
    , payloads(max_num_motor_experts)
    , motorspace(robot.get_joints())
    , experts(max_num_motor_experts, motorspace, payloads, constants::local_learning_rate, constants::experience_size)
    , gmes(experts, constants::gmes_learning_rate)
    {
        dbg_msg("Creating new competitive motor layer.");
    }



    std::size_t                  max_num_motor_experts;
    control::Jointcontrol        control;
    control::Control_Vector      params; /** IDEA: this object is only responsible for loading controller params from FS, it copies the weights into the experts and the ctrl.loop must be passed a ref to the experts weights.*/
    /** where to put the control parameters, inside the predictor in the expert? */
    /** the predictor currently uses only the bias/average weight variant (1), use all inputs from (1, s0..sN-1, m0..mN-1) */
    static_vector<Empty_Payload> payloads;
    Motor_Space                  motorspace;
    Expert_Vector                experts;
    GMES                         gmes;

};



} // namespace learning


#endif // MOTOR_LAYER_H_INCLUDED

