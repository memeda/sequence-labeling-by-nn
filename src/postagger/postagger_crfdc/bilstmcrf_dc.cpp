#include <fstream>
#include <vector>
#include <string>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "bilstmcrf_dc.h"

using namespace std;
using namespace dynet;
namespace slnn{

const std::string BILSTMCRFDCModel4POSTAG::UNK_STR = "<UNK_REPR>";

BILSTMCRFDCModel4POSTAG::BILSTMCRFDCModel4POSTAG()
    : m(nullptr),
    merge_doublechannel_layer(nullptr) ,
    bilstm_layer(nullptr),
    merge_hidden_layer(nullptr),
    emit_layer(nullptr),
    dynamic_dict_wrapper(dynamic_dict)
{}

BILSTMCRFDCModel4POSTAG::~BILSTMCRFDCModel4POSTAG()
{
    if (m) delete m;
    if (merge_doublechannel_layer) delete merge_doublechannel_layer;
    if (bilstm_layer) delete bilstm_layer;
    if (merge_hidden_layer) delete merge_hidden_layer;
    if (emit_layer) delete emit_layer;
}

void BILSTMCRFDCModel4POSTAG::build_model_structure()
{
    assert(dynamic_dict.is_frozen() && fixed_dict.is_frozen() && postag_dict.is_frozen()); // Assert all frozen
    m = new Model();
    merge_doublechannel_layer = new Merge2Layer(m, dynamic_embedding_dim, fixed_embedding_dim, lstm_x_dim);
    bilstm_layer = new BILSTMLayer(m, nr_lstm_stacked_layer, lstm_x_dim, lstm_h_dim);
    merge_hidden_layer = new Merge3Layer(m, lstm_h_dim, lstm_h_dim, postag_embedding_dim, merge_hidden_dim);
    emit_layer = new DenseLayer(m, merge_hidden_dim, 1);

    dynamic_words_lookup_param = m->add_lookup_parameters(dynamic_embedding_dict_size, { dynamic_embedding_dim });
    fixed_words_lookup_param = m->add_lookup_parameters(fixed_embedding_dict_size, { fixed_embedding_dim });
    postags_lookup_param = m->add_lookup_parameters(postag_dict_size , { postag_embedding_dim });

    trans_score_lookup_param = m->add_lookup_parameters(postag_dict_size * postag_dict_size, { 1 });
    init_score_lookup_param = m->add_lookup_parameters(postag_dict_size, { 1 });

}

void BILSTMCRFDCModel4POSTAG::print_model_info()
{
    BOOST_LOG_TRIVIAL(info) << "---------------- Model Structure Info ----------------\n"
        << "dynamic vocabulary size : " << dynamic_embedding_dict_size << " with dimention : " << dynamic_embedding_dim << "\n"
        << "fixed vocabulary size : " << fixed_embedding_dict_size << " with dimension : " << fixed_embedding_dim << "\n"
        << "bilstm stacked layer num : " << nr_lstm_stacked_layer << " , x dim  : " << lstm_x_dim << " , h dim : " << lstm_h_dim << "\n"
        << "postag num : " << postag_dict_size << " with dimension : " << postag_embedding_dim << "\n" 
        << "merge hidden dimension : " << merge_hidden_dim ;
}

Expression BILSTMCRFDCModel4POSTAG::viterbi_train(ComputationGraph *p_cg, 
    const IndexSeq *p_dynamic_sent, const IndexSeq *p_fixed_sent, const IndexSeq *p_tag_seq,
    Stat *p_stat)
{
    const unsigned sent_len = p_dynamic_sent->size();
    ComputationGraph &cg = *p_cg;
    // New graph , ready for new sentence
    merge_doublechannel_layer->new_graph(cg);
    bilstm_layer->new_graph(cg);
    merge_hidden_layer->new_graph(cg);
    emit_layer->new_graph(cg);

    bilstm_layer->start_new_sequence();

    // Some container
    vector<Expression> merge_dc_exp_cont(sent_len);
    vector<Expression> l2r_lstm_output_exp_cont; // for storing left to right lstm output(deepest hidden layer) expression for every timestep
    vector<Expression> r2l_lstm_output_exp_cont; // right to left 

    // 1. build input , and merge
    for (unsigned i = 0; i < sent_len; ++i)
    {
        Expression dynamic_word_lookup_exp = lookup(cg, dynamic_words_lookup_param, p_dynamic_sent->at(i));
        Expression fixed_word_lookup_exp = const_lookup(cg, fixed_words_lookup_param, p_fixed_sent->at(i)); // const look up
        Expression merge_dc_exp = merge_doublechannel_layer->build_graph(dynamic_word_lookup_exp, fixed_word_lookup_exp);
        merge_dc_exp_cont[i] = rectify(merge_dc_exp); // rectify for merged expression
    }

    // 2. Build bi-lstm
    bilstm_layer->build_graph(merge_dc_exp_cont, l2r_lstm_output_exp_cont, r2l_lstm_output_exp_cont);

    // viterbi data preparation
    vector<Expression> all_postag_exp_cont(postag_dict_size);
    vector<Expression> init_score(postag_dict_size);
    vector<Expression> trans_score(postag_dict_size * postag_dict_size);
    vector<vector<Expression>> emit_score(sent_len, vector<Expression>(postag_dict_size));
    vector<Expression> cur_score_exp_cont(postag_dict_size)  ,
                       pre_score_exp_cont(postag_dict_size) ;
    vector<vector<size_t>> *p_path_matrix = nullptr ;
        // allocate memory only when using p_stat
    if(p_stat) p_path_matrix = new vector<vector<size_t>>(sent_len , vector<size_t>(postag_dict_size)) ;
    Expression gold_score_exp ;
    
    // init all_postag_exp_cont , init_score together
    for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
    {
        all_postag_exp_cont[postag_idx] = lookup(cg, postags_lookup_param, postag_idx);
        init_score[postag_idx] = lookup(cg, init_score_lookup_param, postag_idx);
    }
    // init translation score
    for (size_t flat_idx = 0; flat_idx < postag_dict_size * postag_dict_size; ++flat_idx)
    {
        trans_score[flat_idx] = lookup(cg, trans_score_lookup_param, flat_idx);
    }
    // init emit score
    for (size_t time_step = 0; time_step < sent_len; ++time_step)
    {
        for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
        {
            Expression hidden_out_exp = merge_hidden_layer->build_graph(l2r_lstm_output_exp_cont[time_step],
                r2l_lstm_output_exp_cont[time_step], all_postag_exp_cont[postag_idx]);
            emit_score[time_step][postag_idx] = emit_layer->build_graph( rectify(hidden_out_exp) );
        }
    }
    // viterbi docoding
    // 1. the time 0
    for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
    {
        // init_score + emit_score
        cur_score_exp_cont[postag_idx] = init_score[postag_idx] + emit_score[0][postag_idx];
    }
    gold_score_exp = cur_score_exp_cont[p_tag_seq->at(0)];
    // 2. the continues time
    for(size_t time_step = 1; time_step < sent_len; ++time_step)
    {
        // for every tag(to-tag)
        swap(cur_score_exp_cont , pre_score_exp_cont) ;
        for (size_t cur_postag_idx = 0; cur_postag_idx < postag_dict_size; ++cur_postag_idx)
        {
            // for every possible trans
            vector<Expression> partial_score_exp_cont(postag_dict_size);
            for (size_t pre_postag_idx = 0; pre_postag_idx < postag_dict_size; ++pre_postag_idx)
            {
                size_t flatten_idx = pre_postag_idx * postag_dict_size + cur_postag_idx ;
                // from-tag score + trans_score
                partial_score_exp_cont[pre_postag_idx] = pre_score_exp_cont[pre_postag_idx] +
                    trans_score[flatten_idx];
            }
            cur_score_exp_cont[cur_postag_idx] = logsumexp(partial_score_exp_cont) + 
                                                 emit_score[time_step][cur_postag_idx];
            if (p_stat)
            {
                size_t pre_tag_value = 0;
                dynet::real max_score_value = as_scalar(cg.get_value(partial_score_exp_cont[pre_tag_value]));
                for (size_t pre_tag_idx = 1; pre_tag_idx < postag_dict_size; ++pre_tag_idx)
                {
                    dynet::real score_value = as_scalar(cg.get_value(partial_score_exp_cont[pre_tag_idx]));
                    if (score_value > max_score_value)
                    {
                        max_score_value = score_value;
                        pre_tag_value = pre_tag_idx;
                    }
                }
                (*p_path_matrix)[time_step][cur_postag_idx] = pre_tag_value;
            }
        }
        // calc gold 
        size_t gold_trans_flatten_idx = p_tag_seq->at(time_step - 1) * postag_dict_size + p_tag_seq->at(time_step);
        gold_score_exp = gold_score_exp + trans_score[gold_trans_flatten_idx] + 
                                          emit_score[time_step][p_tag_seq->at(time_step)];
    }
    Expression predict_score_exp = logsumexp(cur_score_exp_cont);

    // predict is the max-score of lattice
    // if totally correct , loss = 0 (predict_score = gold_score , that is , predict sequence equal to gold sequence)
    // else , loss = predict_score - gold_score
    Expression loss =  predict_score_exp - gold_score_exp;
    if (p_stat)
    {
        Index end_predicted_idx = 0;
        dynet::real max_score_value = as_scalar(cg.get_value(cur_score_exp_cont[end_predicted_idx]));
        for (size_t postag_idx = 1; postag_idx < postag_dict_size; ++postag_idx)
        {
            dynet::real score_value = as_scalar(cg.get_value(cur_score_exp_cont[postag_idx]));
            if (score_value > max_score_value)
            {
               max_score_value = score_value;
               end_predicted_idx = postag_idx;
            }
        }
        ++p_stat->total_tags;
        if (end_predicted_idx == p_tag_seq->at(sent_len - 1)) ++p_stat->correct_tags;
        Index pre_predicted_tag = end_predicted_idx;
        for (unsigned backtrace_idx = sent_len - 1; backtrace_idx >= 1; --backtrace_idx)
        {
            pre_predicted_tag = (*p_path_matrix)[backtrace_idx][pre_predicted_tag];
            ++p_stat->total_tags;
            if (pre_predicted_tag == p_tag_seq->at(backtrace_idx - 1)) ++p_stat->correct_tags;
        }
        if(p_path_matrix) delete p_path_matrix ;
    }
    return loss;
}

void BILSTMCRFDCModel4POSTAG::viterbi_predict(ComputationGraph *p_cg, 
    const IndexSeq *p_dynamic_sent, const IndexSeq *p_fixed_sent, IndexSeq *p_predict_tag_seq)
{
    // The main structure is just a copy from build_bilstm4tagging_graph2train! 
    const unsigned sent_len = p_dynamic_sent->size();
    ComputationGraph &cg = *p_cg;
    // New graph , ready for new sentence 
    // New graph , ready for new sentence
    merge_doublechannel_layer->new_graph(cg);
    bilstm_layer->new_graph(cg);
    merge_hidden_layer->new_graph(cg);
    emit_layer->new_graph(cg);

    bilstm_layer->start_new_sequence();

    // Some container
    vector<Expression> merge_dc_exp_cont(sent_len);
    vector<Expression> l2r_lstm_output_exp_cont; // for storing left to right lstm output(deepest hidden layer) expression for every timestep
    vector<Expression> r2l_lstm_output_exp_cont; // right to left                                                  
    
    // 1. get word embeddings for sent 
    for (unsigned i = 0; i < sent_len; ++i)
    {
        Expression dynamic_word_lookup_exp = lookup(cg, dynamic_words_lookup_param, p_dynamic_sent->at(i));
        Expression fixed_word_lookup_exp = const_lookup(cg, fixed_words_lookup_param, p_fixed_sent->at(i)); // const look up
        Expression merge_dc_exp = merge_doublechannel_layer->build_graph(dynamic_word_lookup_exp, fixed_word_lookup_exp);
        merge_dc_exp_cont[i] = rectify(merge_dc_exp); // rectify for merged expression
    }
    // 2. calc Expression of every timestep of BI-LSTM

    bilstm_layer->build_graph(merge_dc_exp_cont, l2r_lstm_output_exp_cont, r2l_lstm_output_exp_cont);
    
    //viterbi - preparing score
    vector<Expression> all_postag_exp_cont(postag_dict_size);
    vector<dynet::real> init_score(postag_dict_size);
    vector<dynet::real> trans_score(postag_dict_size * postag_dict_size);
    vector<vector<dynet::real>> emit_score(sent_len, vector<dynet::real>(postag_dict_size));

    // get init score
    for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
    {
        Expression init_exp = lookup(cg, init_score_lookup_param, postag_idx);
        init_score[postag_idx] = as_scalar(cg.get_value(init_exp));
    }
    // get trans score
    for (size_t flat_idx = 0; flat_idx < postag_dict_size * postag_dict_size; ++flat_idx)
    {
        Expression trans_exp = lookup(cg, trans_score_lookup_param, flat_idx);
        trans_score[flat_idx] = as_scalar(cg.get_value(trans_exp));
    }
    // get emit score
    for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
    {
        all_postag_exp_cont[postag_idx] = lookup(cg, postags_lookup_param, postag_idx);
    }
    for (size_t time_step = 0; time_step < sent_len; ++time_step)
    {
        for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
        {
            Expression hidden_out_exp = merge_hidden_layer->build_graph(l2r_lstm_output_exp_cont[time_step],
                r2l_lstm_output_exp_cont[time_step], all_postag_exp_cont[postag_idx]);
            Expression emit_score_exp = emit_layer->build_graph(rectify(hidden_out_exp));
            emit_score[time_step][postag_idx] =  as_scalar(cg.get_value(emit_score_exp));
        }
            
    }
    // viterbi - process
    vector<vector<size_t>> path_matrix(sent_len, vector<size_t>(postag_dict_size));
    vector<dynet::real> current_scores(postag_dict_size);
    // time 0
    for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
    {
        current_scores[postag_idx] = init_score[postag_idx] + emit_score[0][postag_idx];
    }
    // continues time
    vector<dynet::real>  pre_timestep_scores(current_scores);
    for (size_t time_step = 1; time_step < sent_len; ++time_step)
    {
        swap(pre_timestep_scores, current_scores); // move current_score -> pre_timestep_score
        for (size_t postag_idx = 0; postag_idx < postag_dict_size; ++postag_idx)
        {
            size_t pre_tag_with_max_score = 0;
            dynet::real max_score = pre_timestep_scores[0] + trans_score[postag_idx]; // 0*postag_dict_size + postag_idx
            for (size_t pre_tag_idx = 1; pre_tag_idx < postag_dict_size; ++pre_tag_idx)
            {
                size_t flat_idx = pre_tag_idx * postag_dict_size + postag_idx;
                dynet::real score = pre_timestep_scores[pre_tag_idx] + trans_score[flat_idx];
                if (score > max_score)
                {
                    pre_tag_with_max_score = pre_tag_idx;
                    max_score = score;
                }
            }
            path_matrix[time_step][postag_idx] = pre_tag_with_max_score;
            current_scores[postag_idx] = max_score + emit_score[time_step][postag_idx];
        }
    }
    // get result 
    IndexSeq tmp_predict_tag_seq(sent_len);
    Index end_predicted_idx = distance(current_scores.begin() ,  max_element(current_scores.begin(), current_scores.end()));
    tmp_predict_tag_seq[sent_len - 1] = end_predicted_idx;
    Index pre_predicted_idx = end_predicted_idx;
    for (size_t reverse_idx = sent_len - 1; reverse_idx >= 1 ; --reverse_idx)
    {
        pre_predicted_idx = path_matrix[reverse_idx][pre_predicted_idx]; // backtrace
        tmp_predict_tag_seq[reverse_idx-1] = pre_predicted_idx;

    }
    swap(tmp_predict_tag_seq, *p_predict_tag_seq);
}


} // end of namespace
