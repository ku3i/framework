#ifndef JOINT_H
#define JOINT_H

#include <string>
#include <common/log_messages.h>

namespace robots {

enum Joint_Type {Joint_Type_Normal, Joint_Type_Symmetric};

class Joint_Model
{
public:
    Joint_Model( unsigned int joint_id
               , Joint_Type type
               , unsigned int symmetric_joint
               , const std::string& name
               , double limit_lo
               , double limit_hi
               , double default_pos)
    : joint_id(joint_id)
    , s_ang(.0)
    , s_vel(.0)
    , motor(.0)
    , type(type)
    , symmetric_joint(symmetric_joint)
    , name(name)
    , limit_lo(limit_lo)
    , limit_hi(limit_hi)
    , default_pos(default_pos)
    {
        sts_msg("Created joint %u '%s'", joint_id, name.c_str());
        sts_msg("  Limits (%+1.2f, %+1.2f, %+1.2f) (%+3.1f, %+3.1f, %+3.1f)", limit_lo    , limit_hi    , default_pos
                                                                            , limit_lo*180, limit_hi*180, default_pos*180);
        sts_msg("  Type: %u, symmetric: %u \n", type, symmetric_joint);
    }

    Joint_Model()
    : Joint_Model(0, Joint_Type_Normal, 0, "joint0", -1.0, +1.0, 0.0) {}

    const unsigned int joint_id;
    double s_ang;
    double s_vel;
    double motor;

    Joint_Type type;
    unsigned int symmetric_joint;
    const std::string name;

    const double limit_lo;
    const double limit_hi;
    const double default_pos;
};

typedef std::vector<Joint_Model> Jointvector_t;

} // namespace robots

#endif // JOINT_H
