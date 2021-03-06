
#include <control/controlparameter.h>

namespace control {


    Control_Parameter::Control_Parameter( const std::string& filename
                                        , std::size_t number_of_params
                                        , bool symmetric
                                        , bool mirrored
                                        , unsigned robot_id
                                        , uint64_t rnd_init )
    : parameter()
    , symmetric(symmetric)
    , mirrored (mirrored )
    , robot_id (robot_id )
    , rnd_init (rnd_init )
    {
        sts_msg("Loading controller weights from CSV file:\n   '%s'", filename.c_str());
        file_io::CSV_File<double> csv_file(filename, 1, number_of_params);
        if (csv_file.read()) {
            parameter.assign(number_of_params, 0.0);
            csv_file.get_line(0, parameter);
        } else
            wrn_msg("Could not read from file: %s", filename.c_str());
    }


    Control_Parameter::Control_Parameter(const std::string& filename)
    : parameter()
    , symmetric()
    , mirrored ()
    , robot_id ()
    , rnd_init ()
    {
        sts_msg("Loading controller file: '%s'", filename.c_str());
        file_io::Data_Reader dat_file(filename, /*verbose=*/false);
        assert(dat_file.read("parameter", parameter)); /** TODO: remove assert dependence */

        symmetric = ("symmetric" == dat_file.read_string("symmetry"   )) ? true : false;
        mirrored  = ("original"  == dat_file.read_string("propagation")) ? false : true;
        robot_id  = dat_file.read_unsigned("robot_id", 0);
        rnd_init  = dat_file.read_unsigned("random_init", 0);
    }


    Control_Parameter::Control_Parameter(const std::vector<double>& parameter, bool symmetric, bool mirrored, unsigned robot_id, uint64_t rnd_init)
    : parameter(parameter)
    , symmetric(symmetric)
    , mirrored (mirrored )
    , robot_id (robot_id )
    , rnd_init (rnd_init )
    {}

    void Control_Parameter::set_from_matrix(matrix_t const& weights)
    {
        assert(symmetric == false and mirrored == false); /**TODO accept mirrored weights */
        assert(weights.size() > 0 and weights[0].size() > 0);
        assert(parameter.size() == weights.size() * weights[0].size());

        std::size_t p = 0;
        for (auto const& wi : weights)
            for (auto const& wij : wi)
                parameter[p++] = wij;
        assert( p == parameter.size());
    }

    Control_Parameter::Control_Parameter(const Control_Parameter& other)
    : noncopyable()
    , parameter(other.parameter)
    , symmetric(other.symmetric)
    , mirrored (other.mirrored )
    , robot_id (other.robot_id )
    , rnd_init (other.rnd_init )
    { /*dbg_msg("Copying control parameter.");*/ }


    Control_Parameter& Control_Parameter::operator=(const Control_Parameter& other)
    {
        if (this != &other) // avoid invalid self-assignment
        {
            parameter = other.parameter;
            symmetric = other.symmetric;
            mirrored  = other.mirrored;
            robot_id  = other.robot_id;
            rnd_init  = other.rnd_init;
        }
        return *this;
    }

    void Control_Parameter::add_gaussian_noise(double sigma) {
        if (sigma == 0.0) return;
        const double s = sigma/sqrt(parameter.size());
        for (auto &u : parameter) {
            u += rand_norm_zero_mean(s);
        }
    }

    void Control_Parameter::print() const {
        for ( auto const& p : parameter ) printf("% 5.2f ", p);
        printf("\n");
    }

    void Control_Parameter::save_to_file(const std::string& filename, std::size_t id) const {
        sts_msg("Saving control parameter to file: %s", filename.c_str());

        FILE* ctrl = open_file("w", filename.c_str());

        fprintf(ctrl, "name = \"motor-expert-%zu\"\n", id);
        if (robot_id > 0) fprintf(ctrl, "robot_id = %u\n", robot_id);
        if (rnd_init > 0) fprintf(ctrl, "random_init = %zu\n", rnd_init);
        fprintf(ctrl, "symmetry = \"%s\"\n", symmetric ? "symmetric" : "asymmetric");
        fprintf(ctrl, "propagation = \"%s\"\n", mirrored ? "mirrored" : "original");
        fprintf(ctrl, "parameter = { ");
        for ( auto const& p : parameter ) fprintf(ctrl, "%e ", p);
        fprintf(ctrl, "}\n\n");
        fclose(ctrl);
    }

} /* namespace control */
