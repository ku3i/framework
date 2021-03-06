#ifndef AUTOENCODER_H_INCLUDED
#define AUTOENCODER_H_INCLUDED

#include <common/modules.h>
#include <common/static_vector.h>
#include <control/sensorspace.h>


namespace learning {

class Autoencoder {
    typedef VectorN vector_t;
    typedef copyable_static_vector<copyable_static_vector<double>> matrix_t;

public:
    Autoencoder( const std::size_t input_size
               , const std::size_t hidden_size
               , const double random_weight_range )
    : hidden(hidden_size)
    , outputs(input_size)
    , delta(outputs.size())
    , weights(hidden.size(), input_size)
    {
        dbg_msg("Creating Autoencoder with %u inputs and %u hidden units.", input_size, hidden_size);
        assertion(hidden_size > 0, "Hidden layer must have min. size of 1 (currently=%u)", hidden_size);
        assertion(input_size > hidden_size, "Input size (%u) must be greater than hidden layer size (%u).", input_size, hidden_size);

        assert(weights.size() == hidden_size);
        assert(weights[0].size() == input_size);
        assert(outputs.size() == input_size);

        if (random_weight_range == 0.0)
            wrn_msg("Weights will be all zeros");
        else
            randomize_weight_matrix(random_weight_range);
    }


    template <typename InputVector_t>
    void propagate(const InputVector_t& inputs) {
        /* encoder */
        propagate_forward(inputs);

        /* decoder*/
        propagate_backward(hidden);
    }


    template <typename InputVector_t>
    void propagate_forward(const InputVector_t& inputs)
    {
        assert(inputs.size() == outputs.size());

        /* encoder */
        for (std::size_t i = 0; i < hidden.size(); ++i) {
            double act = 0.;
            for (std::size_t j = 0; j < inputs.size(); ++j)
                act += weights[i][j] * inputs[j];
            hidden[i] = tanh(act);
        }
    }

    template <typename InputVector_t>
    void propagate_backward(const InputVector_t& hidden_input)
    {
        assert(hidden_input.size() == hidden.size());

        /* decoder*/
        for (std::size_t j = 0; j < outputs.size(); ++j) {
            double act = 0.;
            for (std::size_t i = 0; i < hidden_input.size(); ++i)
                act += weights[i][j] * hidden_input[i];
            outputs[j] = tanh(act);
            /** TODO: The decoder should not use the tanh activation function.
                This must also be considered in the weight change. */
        }
    }


    template <typename InputVector_t>
    void adapt(const InputVector_t& inputs, const double learning_rate) {
        assert(inputs.size() == outputs.size());
        assert_in_range(learning_rate, 0.0, 0.5);

        /** x_j : inputs[j]
            y_j : outputs[j]
            h_i : hidden[i]

            eta : learning_rate

            decoder weight change:
            error : e_j = (x_j - y_j)
            delta : d2_j = e_j * (1.0 + y_j) * (1.0 - y_j)
            weight: dw2_ji = eta * d2_j * h_i

            encoder weight change:
            error : eh_i = sum_j d2_j * w2_ji
            delta : d1_i = eh_i * (1.0 + h_i) * (1.0 - h_i)
            weight: dw1_ji = eta * d1_i * x_j

        */

        for (std::size_t j = 0; j < outputs.size(); ++j)
            delta[j] = (inputs[j] - outputs[j]) * tanh_(outputs[j]); /** this tanh_ is not needed, when transfer function of output layer is omitted */

        for (std::size_t i = 0; i < hidden.size(); ++i)
        {
            double error_i = .0;
            for (std::size_t j = 0; j < outputs.size(); ++j)
                error_i += delta[j] * weights[i][j];

            const double delta_i = error_i * tanh_(hidden[i]);
            for (std::size_t j = 0; j < outputs.size(); ++j)
                weights[i][j] += learning_rate * (delta_i * inputs[j] + delta[j] * hidden[i]);
        }
    }

    vector_t const& get_outputs() const { return outputs; }
    matrix_t const& get_weights() const { return weights; }


    void randomize_weight_matrix(const double random_weight_range) {
        assert_in_range(random_weight_range, 0.0, 0.5);
        const double normed_std_dev = random_weight_range / sqrt(weights[0].size());
        assert(normed_std_dev != 0.0);

        for (std::size_t i = 0; i < weights.size(); ++i) {
            for (std::size_t j = 0; j < weights[i].size(); ++j)
                weights[i][j] = rand_norm_zero_mean(normed_std_dev); // normalized by sqrt(N), N:#inputs
        }
    }

    template <typename InputVector_t, typename TargetVector_t>
    void train(const InputVector_t& inputs, const TargetVector_t& targets, const double learning_rate)
    {
        set_hidden(targets);
        adapt(inputs, learning_rate);
    }

    vector_t const& get_forward() const { return hidden; }

    template <typename InputVector_t>
    vector_t const& propagate_forward_and_get(const InputVector_t& inputs) {
        propagate_forward(inputs);
        return hidden;
    }

    template <typename InputVector_t>
    vector_t const& propagate_backward_and_get(const InputVector_t& hidden_input) {
        propagate_backward(hidden_input);
        return outputs;
    }

private:

    template <typename InputVector_t>
    void set_hidden(const InputVector_t& x) {
        assert(x.size() == hidden.size());
        if (is_vector_zero(x))
            wrn_msg("Assigned hidden vector is all zeros.");
        hidden = x;
    }

    vector_t hidden;
    vector_t outputs;
    vector_t delta;
    matrix_t weights;
};


} // learning


#endif // AUTOENCODER_H_INCLUDED
