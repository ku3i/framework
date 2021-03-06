#ifndef STATE_LAYER_H_INCLUDED
#define STATE_LAYER_H_INCLUDED

#include <common/integrator.h>
#include <robots/robot.h>
#include <robots/joint.h>
#include <control/jointcontrol.h>
#include <control/control_vector.h>
#include <control/sensorspace.h>
#include <learning/expert.h>
#include <learning/gmes.h>
#include <learning/payload.h>
#include <learning/state_predictor.h>
#include <learning/learning_machine_interface.h>

/* graphics */
#include <draw/draw.h>
#include <draw/axes.h>
#include <draw/axes3D.h>
#include <draw/plot1D.h>
#include <draw/plot2D.h>
#include <draw/plot3D.h>
#include <draw/network3D.h>
#include <draw/graphics.h>
#include <draw/color_table.h>


namespace learning {


namespace state_layer_constants {
    const unsigned subspace_num_datapoints = 200; // 2s of data at 100Hz
}

class State_Space : public sensor_vector {
public:
    State_Space(const robots::Jointvector_t& joints)
    : sensor_vector(joints.size())
    {
        for (robots::Joint_Model const& j : joints)
            sensors.emplace_back(j.name + "_ang", [&j](){ return j.s_ang; });

        for (robots::Joint_Model const& j : joints)
            sensors.emplace_back(j.name + "_vel", [&j](){ return j.s_vel; });


        /* TODO: why is there no accel data included*/

        sensors.emplace_back("bias", [](){ return 0.1; });
    }
};

class State_Layer : public control::Statemachine_Interface, public learning::Learning_Machine_Interface {
public:
    State_Layer( robots::Robot_Interface const& robot
               , static_vector_interface& payloads
               , std::size_t max_num_state_experts
               , double learning_rate
               , double growth_rate
               , std::size_t experience_size
               , std::size_t hidden_layer_size
               , std::size_t time_delay_size )
    : max_num_state_experts(max_num_state_experts)
    , payloads(payloads)
    , statespace(robot.get_joints())
    , experts(max_num_state_experts, payloads, statespace, learning_rate, experience_size, hidden_layer_size, time_delay_size)
    , gmes(experts, growth_rate, /* one shot learning = */false)
    {
        dbg_msg("Creating new competitive state layer.");
        assert(payloads.size() == max_num_state_experts); /**TODO: re-factor that, move payloads<template> to state_layer*/
        assert(learning_rate > 0.);
        assert(growth_rate > 0.);
    }

    void execute_cycle(void) {
        statespace.execute_cycle();
        gmes.execute_cycle();
    }

    bool has_state_changed(void) const override { return gmes.has_state_changed(); };
    std::size_t  get_state(void) const override { return gmes.get_state(); }

    double get_learning_progress(void) const override { return gmes.get_learning_progress(); }

    void enable_learning(bool b) override { gmes.enable_learning(b); }
    void toggle_learning(void) { gmes.enable_learning(not gmes.is_learning_enabled()); }

    void save(const std::string& foldername) const {
        const std::size_t num_experts = gmes.get_number_of_experts();
        sts_msg("Saving %lu state expert%s to folder %s", num_experts, (num_experts>1?"s":""), foldername.c_str());
        const std::string folder = basic::make_directory((foldername + "/state").c_str());
        for (std::size_t i = 0; i < num_experts; ++i)
        {
            //auto const& ctrl = get_controller_weights(i);
            //ctrl.save_to_file(folder + "/motor_expert_" + std::to_string(i) + ".dat", i);
        }
    }

    std::size_t                  max_num_state_experts;
    static_vector_interface&     payloads;
    State_Space                  statespace;
    Expert_Vector                experts;
    GMES                         gmes;

    friend class State_Layer_Graphics;
};




struct sensor_subspace_graphics : public Graphics_Interface
{
    sensor_subspace_graphics( robots::Joint_Model const& j0
                            , robots::Joint_Model const& j1
                            , Vector3 pos, float size
                            , ColorTable const& colortable)
    : j0(j0)
    , j1(j1)
    , axis_xy(pos.x + size/2        , pos.y + size/2, pos.z,     size, size, 0, std::string("xy"))
    , axis_dt(pos.x + (2.0 + size)/2, pos.y + size/2, pos.z, 2.0-size, size, 1, std::string(j0.name + "/" + j1.name))
    , plot_xy(state_layer_constants::subspace_num_datapoints, axis_xy, colors::magenta )
    , plot_j0(state_layer_constants::subspace_num_datapoints, axis_dt, colors::light0, "j0")
    , plot_j1(state_layer_constants::subspace_num_datapoints, axis_dt, colors::light1, "j1")
    , plot_p0(state_layer_constants::subspace_num_datapoints, axis_dt, colortable    , "p0")
    , plot_p1(state_layer_constants::subspace_num_datapoints, axis_dt, colortable    , "p1")
    {
        dbg_msg("Initialize subspace for joints:\n\t%2u: %s\n\t%2u: %s", j0.joint_id, j0.name.c_str()
                                                                       , j1.joint_id, j1.name.c_str() );
    }

    void draw(const pref&) const {
        axis_xy.draw();
        plot_xy.draw();

        axis_dt.draw();
        plot_j0.draw(); // signals
        plot_j1.draw();
        plot_p0.draw_colored(); // predictions
        plot_p1.draw_colored();
    }

    void execute_cycle(float s0, float s1, unsigned expert_id) {
        plot_xy.add_sample(j0.s_ang, j1.s_ang); // TODO: add velocities
        plot_j0.add_sample(j0.s_ang);
        plot_j1.add_sample(j1.s_ang);
        plot_p0.add_colored_sample(s0, expert_id);
        plot_p1.add_colored_sample(s1, expert_id);
    }

    const robots::Joint_Model& j0;
    const robots::Joint_Model& j1;

    axes   axis_xy;
    axes   axis_dt;

    plot2D plot_xy;
    plot1D plot_j0, plot_j1; // joint sensor values
    colored_plot1D plot_p0, plot_p1; // predictions

};

class State_Layer_Graphics : public Graphics_Interface {
public:

    State_Layer_Graphics( State_Layer const& state_layer
                        , robots::Robot_Interface const& robot )
    : state_layer(state_layer)
    , num_experts()
    , max_experts(state_layer.gmes.get_max_number_of_experts())
    , winner()
    , subspace()
    , colortable(5, /*randomized*/true)
    , integrator()
    , axis_dt(-0.5,-1.50, 0.0, 1.0, 1.0, 1, "PredErr")
    , axis_ne(-0.5,-1.75, 0.0, 1.0, 0.5, 1, "#Experts")
    , pred_err(2000, axis_dt, colors::gray0)
    , pred_err_avg(2000, axis_dt, colors::white)
    , num_exp(2000, axis_ne, colors::cyan)
    {
        /** TODO
            + also for the rest of the joints
            + consider grouping the graphs in legs (4x 3D) instead of 6x 2D
        */
        const unsigned N = robot.get_number_of_symmetric_joints();
        const float size = 2.0/N;
        subspace.reserve(N);
        Vector3 pos(-1.0, 1.0, 0.);
        for (auto const& j0: robot.get_joints()) {
            if (j0.is_symmetric()) {
                robots::Joint_Model const& j1 = robot.get_joints()[j0.symmetric_joint];
                pos.y -= size;
                subspace.emplace_back(j1, j0, pos, size, colortable);
            }
        }
    }

    void draw(const pref& p) const {
        glColor3f(1.0, 1.0, 1.0);
        glprintf(-0.95, 0.95, 0., 0.025, "%03u (%03u/%03u)", winner, num_experts, max_experts);

        for (auto& s: subspace)
            s.draw(p);

        axis_dt.draw();
        axis_ne.draw();
        pred_err.draw();
        pred_err_avg.draw();
        num_exp.draw();
    };

    void execute_cycle() {

        num_experts = state_layer.gmes.get_number_of_experts(); // update number of experts
        winner = state_layer.gmes.get_winner();
        auto const& predictions = state_layer.experts[winner].get_predictor().get_prediction();

        for (auto& s: subspace) {
            auto s0 = predictions.at(s.j0.joint_id); /**TODO: how to access the velocities*/
            auto s1 = predictions.at(s.j1.joint_id);
            s.execute_cycle(s0, s1, winner);
        }

        integrator.add(state_layer.gmes.get_min_prediction_error());
        if (integrator.get_number_of_samples()==1000) {
            const float val = 100.0*integrator.get_avg_value_and_reset();
            avg_err = 0.99*avg_err + 0.01*val;
            pred_err.add_sample(val);
            pred_err_avg.add_sample(avg_err);
            num_exp.add_sample(state_layer.gmes.get_number_of_experts());
        }
    };

    State_Layer const& state_layer;
    std::size_t        num_experts;
    std::size_t        max_experts;
    std::size_t        winner;

    std::vector<sensor_subspace_graphics> subspace;
    ColorTable                            colortable;

    Integrator integrator;
    float      avg_err = 0.0;
    axes       axis_dt;
    axes       axis_ne;
    plot1D     pred_err,pred_err_avg;
    plot1D     num_exp;
};

} // namespace learning

#endif // STATE_LAYER_H_INCLUDED
