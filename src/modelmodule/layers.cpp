#include "layers.h" 

using namespace dynet;
using namespace std;
namespace slnn {

// DenseLayer

DenseLayer::DenseLayer(Model *m , unsigned input_dim , unsigned output_dim)
    :w(m->add_parameters({output_dim , input_dim})) ,
    b(m->add_parameters({output_dim}))
{}

DenseLayer::~DenseLayer(){}

// Merge 2 Layer

Merge2Layer::Merge2Layer(Model *m, unsigned input1_dim, unsigned input2_dim,unsigned output_dim)
    :w1(m->add_parameters({ output_dim , input1_dim })),
    w2(m->add_parameters({ output_dim , input2_dim })),
    b(m->add_parameters({ output_dim}))
{}

Merge2Layer::~Merge2Layer() {}


// Merge 3 Layer

Merge3Layer::Merge3Layer(Model *m ,unsigned input1_dim , unsigned input2_dim , unsigned input3_dim , unsigned output_dim )
    :w1(m->add_parameters({output_dim , input1_dim})) ,
    w2(m->add_parameters({output_dim , input2_dim})) ,
    w3(m->add_parameters({output_dim , input3_dim})) ,
    b(m->add_parameters({output_dim}))
{}

Merge3Layer::~Merge3Layer(){}


// Merge 4 Layer
Merge4Layer::Merge4Layer(Model *m ,unsigned input1_dim , unsigned input2_dim , unsigned input3_dim ,
    unsigned input4_dim, unsigned output_dim )
    :w1(m->add_parameters({output_dim , input1_dim})) ,
    w2(m->add_parameters({output_dim , input2_dim})) ,
    w3(m->add_parameters({output_dim , input3_dim})) ,
    w4(m->add_parameters({output_dim, input4_dim})),
    b(m->add_parameters({output_dim}))
{}

Merge4Layer::~Merge4Layer(){}

// MLPHiddenLayer

MLPHiddenLayer::MLPHiddenLayer(Model *m, unsigned input_dim, const vector<unsigned> &layers_dim, 
    dynet::real dropout_rate,
    NonLinearFunc *nonlinear_func)
    : nr_hidden_layer(layers_dim.size()),
    output_dim(nr_hidden_layer > 0 ? layers_dim.back() : input_dim),
    w_list(nr_hidden_layer),
    b_list(nr_hidden_layer),
    w_expr_list(nr_hidden_layer),
    b_expr_list(nr_hidden_layer),
    dropout_rate(dropout_rate),
    is_enable_dropout(true),
    nonlinear_func(nonlinear_func)
{
    assert(nr_hidden_layer > 0);
    w_list[0] = m->add_parameters({ layers_dim.at(0), input_dim });
    b_list[0] = m->add_parameters({ layers_dim.at(0) });
    for( unsigned i = 1 ; i < nr_hidden_layer ; ++i )
    {
        w_list.at(i) = m->add_parameters({ layers_dim.at(i), layers_dim.at(i - 1) });
        b_list.at(i) = m->add_parameters({ layers_dim.at(i) });
    }
}



} // end namespace slnn