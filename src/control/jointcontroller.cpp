
#include "jointcontroller.h"

Jointcontroller::Jointcontroller( Robot_Configuration& configuration
                                , bool symmetric_controller
                                , double param_p
                                , double param_d
                                , double param_m
                                , const std::string& seed_filename
                                )
: robot(configuration)
, num_inputs(3 * robot.number_of_joints + 3 * robot.number_of_accels + 1) /* angle, velocity, motor output, + xyz-Accel + bias, Idea: add 1 for antisymmetric bias? */
, total_num_params(0) // will be assigned later
, activation(robot.number_of_joints)
, weights(robot.number_of_joints, std::vector<double>(num_inputs, 0.0))
, X(num_inputs)
, Y(num_inputs)
, seed_from_file()
{
    sts_msg("Creating joint controller.");
    if (robot.number_of_joints < 1) err_msg(__FILE__, __LINE__, "No motor outputs.");
    if (0 == robot.number_of_accels) wrn_msg("No use of acceleration sensors in controller.");

    sts_msg("Controller type is %s", symmetric_controller? "symmetric":"asymmetric");
    if (not symmetric_controller)
        robot.delete_symmetry_information();

    unsigned int num_sym_joints = robot.get_number_of_symmetric_joints();
    sts_msg("Number of symmetric joints is %u.", num_sym_joints);

    if (num_sym_joints * 2 > robot.number_of_joints) err_msg(__FILE__, __LINE__, "Invalid number of symmetric joints.\n");

    total_num_params = num_inputs * (robot.number_of_joints - num_sym_joints);
    seed_from_file.reserve(total_num_params);

    assert(weights.size() == robot.number_of_joints);
    assert(weights[0].size() == num_inputs);

    set_initial_parameter(param_p, param_d, param_m); // load defaults
    if (seed_filename != "")
        load_seed(seed_filename); // load seed

    print_parameter();
    sts_msg("Created controller with: \n   %u inputs\n   %u outputs\n   %u params.", num_inputs, robot.number_of_joints, total_num_params);
}

const std::vector<double>
Jointcontroller::get_control_parameter(void) const
{
    std::vector<double> transmission_params(total_num_params);
    assert(weights.size() == robot.number_of_joints);
    assert(weights[0].size() == num_inputs);

    unsigned int p = 0;
    for (unsigned int i = 0; i < robot.number_of_joints; ++i)
        if (robot.joints[i].type == robots::Joint_Type_Normal)
            for (unsigned int k = 0; k < num_inputs; ++k)
            {
                if ((p < total_num_params) && (weights[i][k] == weights[robot.joints[i].symmetric_joint][k]))
                    transmission_params[p++] = weights[i][k];
                else
                    err_msg(__FILE__, __LINE__, "Inconsistent parameter transmission.");
            }

    assert(p == total_num_params);
    return transmission_params;
}

void /* load parameters from file */
Jointcontroller::load_seed(const std::string& filename)
{
    sts_msg("Load controller weights from csv file '%s'", filename.c_str());
    assert(seed_from_file.size() == 0);
    assert(not filename.empty());

    file_io::CSV_File<double> seed_csv(filename, 1, total_num_params);
    seed_csv.read();
    seed_from_file.assign(total_num_params, 0.0);
    seed_csv.get_line(0, seed_from_file);

    assert(seed_from_file.size() == total_num_params);
    set_control_parameter(seed_from_file); // apply weights
}

void
Jointcontroller::set_seed_parameter(void)
{
    assert(seed_from_file.size() == total_num_params);
    set_control_parameter(seed_from_file);
}

void
Jointcontroller::set_control_parameter(const std::vector<double>& params)
{
    assert(params.size() == total_num_params);
    assert(weights.size() == robot.number_of_joints);
    assert(weights[0].size() == num_inputs);

    unsigned int p = 0;
    for (unsigned int i = 0; i < robot.number_of_joints; ++i)
    {
        if (robot.joints[i].type == robots::Joint_Type_Normal)
        {
            assert(robot.joints[i].symmetric_joint < robot.number_of_joints);
            for (unsigned int k = 0; k < num_inputs; ++k)
            {
                weights[i][k] = params[p++]; // apply weights
                weights[robot.joints[i].symmetric_joint][k] = weights[i][k];
            }
        }
    }
    assert(p == params.size());
}

void
Jointcontroller::set_initial_parameter(double p, double d, double m)
{
    /* set default weights for asymmetric joints */
    for (unsigned int i = 0; i < robot.number_of_joints; ++i)
        if (robot.joints[i].type == robots::Joint_Type_Normal)
        {
            weights[i][i*3 + 0] = -p; // spring
            weights[i][i*3 + 1] =  d; // positive friction
            weights[i][i*3 + 2] =  m; // motor neuron's self coupling

            /* divide by initial bias, because INITIAL BIAS < 1 */
            weights[i][num_inputs-1] = -weights[i][i*3 + 0] * robot.joints[i].default_pos * 1.0/INITIAL_BIAS;
            /* if p is zero this bias is also zero */
        }

    /* copy weights for symmetric joints */
    for (unsigned int i = 0; i < robot.number_of_joints; ++i)
        if (robot.joints[i].type == robots::Joint_Type_Symmetric)
            for (unsigned int k = 0; k < num_inputs; ++k)
                weights[i][k] = weights[robot.joints[i].symmetric_joint][k];
}

void
Jointcontroller::print_parameter(void) const
{
    sts_msg("Printing controller parameter:");

    /*print header*/
    printf(" #  |");
    for (std::size_t i = 0; i < robot.number_of_joints; ++i)
        if (robot.joints[i].type == robots::Joint_Type_Normal)
            printf("%4lu  |", i);
    printf("\n");

    for (std::size_t k = 0; k < num_inputs; ++k)
    {
        printf("%2lu: |", k);
        for (std::size_t i = 0; i < robot.number_of_joints; ++i)
        {
            if (robot.joints[i].type == robots::Joint_Type_Normal)
                printf("% 1.3f|", weights[i][k]);
        }
        printf("\n");
    }
    printf("\n");
}

double
Jointcontroller::get_normalized_mechanical_power(void) const
{
    double power = .0;
    for (auto& j : robot.joints)
        power += square(j.motor.get());
    return power/robot.number_of_joints;
}

void
Jointcontroller::reset(void)
{
    for (auto& j : robot.joints)
        j.motor = random_value(-0.01, 0.01);

    /* reset integrated velocities from acceleration sensors */
    for (auto& a : robot.accels) a.reset();
}

void
Jointcontroller::loop(void)
{
    unsigned int index = 0;
    for (auto& jx : robot.joints)
    {
        //virt_ang[i] = clip(virt_ang[i] + x[i][0]);
        auto& jy = robot.joints[jx.symmetric_joint];

        X[index]   = jx.s_ang;
        Y[index++] = jy.s_ang;

        X[index]   = jx.s_vel;
        Y[index++] = jy.s_vel;

        X[index]   = jx.motor.get_backed();
        Y[index++] = jy.motor.get_backed(); //ganz wichtig!, bringt irre viel dynamic für den anfang
    }

    for (auto& a : robot.accels)
    {
        /* integrate velocities from acceleration sensors */
        a.integrate();

        X[index]   = a.v.x;
        Y[index++] = -a.v.x; // mirror the x-axes

        X[index]   = a.v.y;
        Y[index++] = a.v.y;

        X[index]   = a.v.z;
        Y[index++] = a.v.z;

    }

    X[index]   = INITIAL_BIAS; // bias
    Y[index++] = INITIAL_BIAS; // bias

    assert(index == num_inputs);
    assert(activation.size() == robot.number_of_joints);

    for (unsigned int i = 0; i < robot.number_of_joints; ++i)
    {
        activation[i] = 0;
        if (robot.joints[i].type == robots::Joint_Type_Symmetric)
        {
            for (unsigned int k = 0; k < num_inputs; ++k)
                activation[i] += weights[i][k] * Y[k];
        }
        else
        {
            for (unsigned int k = 0; k < num_inputs; ++k)
                activation[i] += weights[i][k] * X[k];
        }
        //TODO robot.joints[i].motor = tanh(activation[i]);
        robot.joints[i].motor = clip(activation[i], 1.0);
    }
}
