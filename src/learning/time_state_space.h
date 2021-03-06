#ifndef TIME_STATE_SPACE_H
#define TIME_STATE_SPACE_H

#include <robots/robot.h>
#include <control/sensorspace.h>

namespace learning {

template <std::size_t NumTaps>
class Time_State_Space : public time_embedded_sensors<NumTaps> {
public:
    Time_State_Space(const robots::Robot_Interface& robot)
    : time_embedded_sensors<NumTaps>(robot.get_joints().size())
    {
        auto const& joints = robot.get_joints();
        auto const& accels = robot.get_accels();

        for (robots::Joint_Model const& j : joints) {
            time_embedded_sensors<NumTaps>::sensors.emplace_back(j.name + "_ang", [&j](){ return j.s_ang; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back(j.name + "_vel", [&j](){ return j.s_vel; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back(j.name + "_vol", [&j](){ return j.motor.get_backed(); });
        }
        //for (robots::Joint_Model const& j : joints)
          //  time_embedded_sensors<NumTaps>::sensors.emplace_back(j.name + "_cur", [&j](){ return j.motor.get_backed();/*j.s_cur;*/ });

        for (robots::Accel_Sensor const& a : accels) {
            time_embedded_sensors<NumTaps>::sensors.emplace_back("acc_x", [&a](){ return a.a.x; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back("acc_y", [&a](){ return a.a.y; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back("acc_z", [&a](){ return a.a.z; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back("vel_x", [&a](){ return a.v.x; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back("vel_y", [&a](){ return a.v.y; });
            time_embedded_sensors<NumTaps>::sensors.emplace_back("vel_z", [&a](){ return a.v.z; });
        }

        // don't know if that helps much.
        //time_embedded_sensors<NumTaps>::sensors.emplace_back("noise", [](){ return 0.1 + rand_norm_zero_mean(0.1); });

        //IDEA: consider avg rotational speed... as the gyroscope

        /*
        for (robots::Joint_Model const& j : joints)
            time_embedded_sensors<NumTaps>::sensors.emplace_back(j.name + "_mot", [&j](){ return j.motor.get(); });*/

        /**Added bias internally */
    }
};

} /* namespace learning */

#endif /* TIME_STATE_SPACE_H */

