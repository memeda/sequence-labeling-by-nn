#ifndef POS_BASE_MODEL_INPUT1_FEATURE2INPUT_LAYER_MODEL_HPP_
#define POS_BASE_MODEL_INPUT1_FEATURE2INPUT_LAYER_MODEL_HPP_

#include <boost/archive/text_iarchive.hpp>
#include <boost/program_options.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/split_member.hpp>

#include "dynet/dynet.h"
#include "dynet/dict.h"
#include "single_input_with_feature_model.hpp"
namespace slnn{

template<typename RNNDerived>
class Input1F2IModel : public SingleInputWithFeatureModel<RNNDerived>
{
    friend class boost::serialization::access;

public:
    Input1F2IModel();
    virtual ~Input1F2IModel();
    Input1F2IModel(const Input1F2IModel &) = delete;
    Input1F2IModel& operator()(const Input1F2IModel&) = delete;

    virtual void set_model_param(const boost::program_options::variables_map &var_map);
    
    virtual void build_model_structure() = 0 ;
    virtual void print_model_info() = 0 ;

    virtual dynet::expr::Expression  build_loss(dynet::ComputationGraph &cg,
                                              const IndexSeq &input_seq, 
                                              const POSFeature::POSFeatureIndexGroupSeq &features_gp_seq,
                                              const IndexSeq &gold_seq) ;
    virtual void predict(dynet::ComputationGraph &cg ,
                         const IndexSeq &input_seq, 
                         const POSFeature::POSFeatureIndexGroupSeq &features_gp_seq,
                         IndexSeq &pred_seq) ;

    template <typename Archive>
    void save(Archive &ar, const unsigned versoin) const; 
    template <typename Archive>
    void load(Archive &ar, const unsigned version);
    template <typename Archive>
    void serialize(Archive & ar, const unsigned version);

protected:
    
    POSFeatureLayer * pos_feature_layer;
    Input1WithFeature *input_layer;
    BIRNNLayer<RNNDerived> *birnn_layer;
    OutputBase *output_layer;

public:
    unsigned word_embedding_dim,
        word_dict_size,
        nr_rnn_stacked_layer,
        rnn_x_dim,
        rnn_h_dim,
        hidden_dim,
        output_dim ;

    dynet::real dropout_rate ; 

};


template<typename RNNDerived>
Input1F2IModel<RNNDerived>::Input1F2IModel() 
    :SingleInputWithFeatureModel<RNNDerived>(),
    pos_feature_layer(nullptr),
    input_layer(nullptr),
    birnn_layer(nullptr),
    output_layer(nullptr)
{}

template <typename RNNDerived>
Input1F2IModel<RNNDerived>::~Input1F2IModel()
{
    delete pos_feature_layer;
    delete input_layer;
    delete birnn_layer;
    delete output_layer;
}




template <typename RNNDerived>
void Input1F2IModel<RNNDerived>::set_model_param(const boost::program_options::variables_map &var_map)
{
    assert(this->word_dict.is_frozen() && this->postag_dict.is_frozen()  && this->pos_feature.is_dict_frozen()) ;

    unsigned replace_freq_threshold = var_map["replace_freq_threshold"].as<unsigned>();
    float replace_prob_threshold = var_map["replace_prob_threshold"].as<float>();
    this->set_replace_threshold(replace_freq_threshold, replace_prob_threshold);

    word_embedding_dim = var_map["word_embedding_dim"].as<unsigned>() ;
    nr_rnn_stacked_layer = var_map["nr_rnn_stacked_layer"].as<unsigned>() ;
    rnn_x_dim = var_map["rnn_x_dim"].as<unsigned>();
    rnn_h_dim = var_map["rnn_h_dim"].as<unsigned>() ;
    hidden_dim = var_map["tag_layer_hidden_dim"].as<unsigned>() ;

    dropout_rate = var_map["dropout_rate"].as<dynet::real>() ;

    unsigned prefix_suffix_len1_embedding_dim = var_map["prefix_suffix_len1_embedding_dim"].as<unsigned>();
    unsigned prefix_suffix_len2_embedding_dim = var_map["prefix_suffix_len2_embedding_dim"].as<unsigned>();
    unsigned prefix_suffix_len3_embedding_dim = var_map["prefix_suffix_len3_embedding_dim"].as<unsigned>();
    unsigned char_length_embedding_dim = var_map["char_length_embedding_dim"].as<unsigned>();
    this->pos_feature.init_embedding_dim(prefix_suffix_len1_embedding_dim, prefix_suffix_len2_embedding_dim,
                                   prefix_suffix_len3_embedding_dim, char_length_embedding_dim);
    
    word_dict_size = this->word_dict.size() ;
    output_dim = this->postag_dict.size() ;
}


template<typename RNNDerived>
dynet::expr::Expression Input1F2IModel<RNNDerived>::build_loss(dynet::ComputationGraph &cg,
                                                                           const IndexSeq &input_seq, 
                                                                           const POSFeature::POSFeatureIndexGroupSeq &features_gp_seq,
                                                                           const IndexSeq &gold_seq)
{
    pos_feature_layer->new_graph(cg);
    input_layer->new_graph(cg) ;
    birnn_layer->new_graph(cg) ;
    output_layer->new_graph(cg) ;

    birnn_layer->set_dropout() ;
    birnn_layer->start_new_sequence() ;

    std::vector<dynet::expr::Expression> features_exprs;
    pos_feature_layer->build_feature_exprs(features_gp_seq, features_exprs);

    std::vector<dynet::expr::Expression> inputs_exprs ;
    input_layer->build_inputs(input_seq, features_exprs, inputs_exprs) ;

    std::vector<dynet::expr::Expression> l2r_exprs,
        r2l_exprs ;
    birnn_layer->build_graph(inputs_exprs, l2r_exprs, r2l_exprs) ;
    return output_layer->build_output_loss(l2r_exprs, r2l_exprs, gold_seq) ;
}

template<typename RNNDerived>
void Input1F2IModel<RNNDerived>::predict(dynet::ComputationGraph &cg,
                                                      const IndexSeq &input_seq,
                                                      const POSFeature::POSFeatureIndexGroupSeq &features_gp_seq,
                                                      IndexSeq &pred_seq)
{
    pos_feature_layer->new_graph(cg);
    input_layer->new_graph(cg) ;
    birnn_layer->new_graph(cg) ;
    output_layer->new_graph(cg) ;

    birnn_layer->disable_dropout() ;
    birnn_layer->start_new_sequence();

    std::vector<dynet::expr::Expression> feature_exprs;
    pos_feature_layer->build_feature_exprs(features_gp_seq, feature_exprs);

    std::vector<dynet::expr::Expression> inputs_exprs ;
    input_layer->build_inputs(input_seq, feature_exprs, inputs_exprs) ;
    std::vector<dynet::expr::Expression> l2r_exprs,
        r2l_exprs ;
    birnn_layer->build_graph(inputs_exprs, l2r_exprs, r2l_exprs) ;
    output_layer->build_output(l2r_exprs, r2l_exprs , pred_seq) ;
}

template <typename RNNDerived>template< typename Archive>
void Input1F2IModel<RNNDerived>::save(Archive &ar, const unsigned version) const
{
    ar & word_dict_size & word_embedding_dim
        & rnn_x_dim & rnn_h_dim & nr_rnn_stacked_layer
        & hidden_dim & output_dim
        & dropout_rate ;
    ar & this->word_dict & this->postag_dict & this->pos_feature ;
    ar & *this->m ;
}

template <typename RNNDerived>template< typename Archive>
void Input1F2IModel<RNNDerived>::load(Archive &ar, const unsigned version)
{
    ar & word_dict_size & word_embedding_dim
        & rnn_x_dim & rnn_h_dim & nr_rnn_stacked_layer
        & hidden_dim & output_dim
        & dropout_rate ;
    ar & this->word_dict & this->postag_dict & this->pos_feature;
    assert(this->word_dict.size() == word_dict_size && this->postag_dict.size() == output_dim) ;
    build_model_structure() ;
    ar & *this->m ;
}

template <typename RNNDerived>
template<typename Archive>
void Input1F2IModel<RNNDerived>::serialize(Archive & ar, const unsigned version)
{
    boost::serialization::split_member(ar, *this, version);
}

} // end of namespace slnn
#endif
