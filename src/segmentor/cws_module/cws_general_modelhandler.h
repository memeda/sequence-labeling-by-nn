#ifndef SLNN_SEGMENTOR_CWS_MODULE_CWS_GENERAL_MODELHANDLER_H_
#define SLNN_SEGMENTOR_CWS_MODULE_CWS_GENERAL_MODELHANDLER_H_
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <boost/program_options/variables_map.hpp>
#include "utils/stat.hpp"
#include "cws_reader.h"
#include "trivial/charcode/charcode_detector.h"
#include "utils/typedeclaration.h"
namespace slnn{
namespace segmentor{
namespace modelhandler{

namespace modelhandler_inner{

template <typename SLModel>
unsigned read_annotated_data(std::ifstream &is, SLModel &slm, 
    std::vector<typename SLModel::AnnotatedDataProcessedT> &out_ann_processed_data);

template <typename SLModel>
unsigned read_unannotated_data(std::ifstream &is, SLModel &slm, 
    std::vector<typename SLModel::UnannotatedDataProcessedT> &out_unann_processed_data);

class TrainingUpdateRecorder
{
public:
    TrainingUpdateRecorder(float error_threshold = 20.f);
    void update_training_state(float current_score, int nr_epoch, int nr_devel_order);
    bool is_training_ok();
    void set_train_error_threshold(float error_threshold);
public: // getter
    int get_best_epoch(){ return nr_epoch_when_best; }
    int get_best_devel_order(){ return nr_devel_order_when_best; }
    float get_best_score(){ return best_score; }
private:
    float best_score;
    int nr_epoch_when_best;
    int nr_devel_order_when_best;
    float train_error_threshold;
    bool is_good;
};

} // end of namespace modelhandler-inner

inline
const std::string& WordOutputDelimiter()
{
    // see Effective C++ , item 04.
    // for non-local static variable, to avoid initialization-race-condition, using local static variable.
    // that is, Singleton Pattern Design
    static std::string WordOutputDelimiterLocalStatic = "\t";
    return WordOutputDelimiterLocalStatic;
}

template <typename SLModel>
bool set_model_structure_param(SLModel &slm, const boost::program_options::variables_map &args);

template <typename SLModel>
void read_training_data(std::ifstream &is, &slm, std::vector<typename SLModel::AnnotatedDataProcessedT> &out_training_processed_data);

template <typename SLModel>
void read_devel_data(std::ifstream &is, SLModel &slm, std::vector<typename SLModel::AnnotatedDataProcessedT> &out_devel_processed_data);

template <typename SLModel>
void read_test_data(std::ifstream &is, SLModel &slm, std::vector<typename SLModel::UnannotatedDataProcessedT> &out_test_processed_data);

template <typename SLModel>
void build_model(SLModel &slm);

template <typename SLModel, typename TrainingOpts>
void train(SLModel &slm,
    const std::vector<typename SLModel::AnnotatedDataProcessedT> &training_data,
    const std::vector<typename SLModel::AnnotatedDataProcessedT> &devel_data,
    const TrainingOpts &opts);

template <typename SLModel>
void devel(SLModel &slm,
    const std::vector<SLModel::AnnotatedDataProcessedT> &devel_data);

template <typename SLModel>
void predict(SLModel &slm,
    std::istream &is,
    std::ostream &os);


/*********************************************************
 * Inline Implementation
 *********************************************************/

namespace modelhandler_inner{

template <typename SLModel>
unsigned read_annotated_data(std::ifstream &is, SLModel &slm, std::vector<typename SLModel::AnnotatedDataProcessedT> &out_ann_processed_data)
{
    reader::SegmentorUnicodeReader reader(is,
        charcode::EncodingDetector::get_detector()->detect_and_set_encoding_from_fstream(is));
    std::vector<typename SLModel::AnnotatedDataProcessedT> dataset;
    unsigned detected_line_cnt = reader.count_line();
    dataset.reserve(detected_line_cnt);
    std::vector<std::u32string> wordseq;
    unsigned readline_cnt = 0,
        report_cnt = detected_line_cnt / 5;
    while( reader.read_segmented_line(wordseq) )
    {
        if( wordseq.empty() ){ continue; }
        typename SLModel::AnnotatedDataProcessedT processed_data;
        slm.get_token_module()->process_annotated_data(wordseq, processed_data);
        dataset.push_back(std::move(processed_data));
        ++readline_cnt;
        if( report_cnt && readline_cnt % report_cnt == 0 )
        {
            std::cerr << "read instance : " << readline_cnt << " [" << readline_cnt / report_cnt << "/5]\n";
        }
    }
    return readline_cnt;
}

template <typename SLModel>
unsigned read_unannotated_data(std::ifstream &is, SLModel &slm,
    std::vector<typename SLModel::UnannotatedDataProcessedT> &out_unann_processed_data)
{
    reader::SegmentorUnicodeReader reader(is,
        charcode::EncodingDetector::get_detector()->detect_and_set_encoding_from_fstream(is));
    std::vector<typename SLModel::UnannotatedDataProcessedT> dataset;
    unsigned detected_line_cnt = reader.count_line();
    dataset.reserve(detected_line_cnt);
    std::u32string charseq;
    unsigned readline_cnt = 0,
        report_cnt = detected_line_cnt / 5;
    while( reader.readline(charseq) )
    {
        typename SLModel::UnannotatedDataProcessedT processed_data;
        slm.get_token_module()->process_annotated_data(charseq, processed_data);
        dataset.push_back(std::move(processed_data));
        ++readline_cnt;
        if( report_cnt && readline_cnt % report_cnt == 0 )
        {
            std::cerr << "read instance : " << readline_cnt << " [" << readline_cnt / report_cnt << "/5]\n";
        }
    }
    return readline_cnt;
}


inline
void TrainingUpdateRecorder::set_train_error_threshold(float error_threshold)
{
    train_error_threshold = error_threshold;
}

inline 
void TrainingUpdateRecorder::update_training_state(float cur_score, int nr_epoch, int nr_devel_order)
{
    is_good = ((best_score - cur_score) < train_error_threshold);
    if( cur_score > best_score )
    {
        nr_epoch_when_best = nr_epoch;
        nr_devel_order_when_best = nr_devel_order;
        best_score = cur_score;
    }
}

inline 
bool TrainingUpdateRecorder::is_training_ok()
{
    return is_good;
}

} // end of namespce modelhandler-inner


template <typename SLModel>
bool set_model_structure_param(SLModel &slm, const boost::program_options::variables_map &args)
{
    slm.set_model_structure_from_outer(args);
}

template <typename SLModel>
void read_training_data(std::ifstream &is, &slm, std::vector<typename SLModel::AnnotatedDataProcessedT> &out_training_processed_data)
{
    std::cerr << "+ Process training data.";
    unsigned line_cnt = modelhandler_inner::read_annotated_data(is, slm, out_training_processed_data);
    std::cerr << "- Training data processed done. ( line count: " << line_cnt << ", instance number: " <<
        out_training_processed_data.size() << "\n";
    slm.finish_read_training_data();
}

template <typename SLModel>
void read_devel_data(std::ifstream &is, SLModel &slm, std::vector<typename SLModel::AnnotatedDataProcessedT> &out_devel_processed_data)
{
    std::cerr << "+ Process devel data.";
    unsigned line_cnt = modelhandler_inner::read_annotated_data(is, slm, out_devel_processed_data);
    std::cerr << "- devel data processed done. (line count: " << line_cnt << ", instance number: " 
        << out_devel_processed_data.size() << ")\n";
}

template <typename SLModel>
void read_test_data(std::ifstream &is, SLModel &slm, std::vector<typename SLModel::UnannotatedDataProcessedT> &out_test_processed_data)
{
    std::cerr << "+ Process test data.";
    unsigned line_cnt = modelhandler_inner::read_unannotated_data(is, slm, out_test_processed_data);
    std::cerr << "- test data processed done. (line count: " << line_cnt << ", instance number: "
        << out_test_processed_data.size() << ")\n";
}

template <typename SLModel>
void build_model(SLModel &slm)
{
    slm.build_model_structure();
}

template <typename SLModel, typename TrainingOpts>
void train(SLModel &slm,
    const std::vector<typename SLModel::AnnotatedDataProcessedT> &training_data,
    const std::vector<typename SLModel::AnnotatedDataProcessedT> &devel_data,
    const TrainingOpts &opts)
{
    unsigned nr_samples = training_data.size();
    std::cerr << "+ Train at " << nr_samples << " instances .";

    modelhandler_inner::TrainingUpdateRecorder update_recorder;
    auto do_devel_in_training = [&devel_data, &slm, &update_recorder](int nr_epoch, int nr_devel_order) 
    {
        // function : 1. devel; 2. stash model when best; 3. update training  state.
        float f1 = this->devel(slm, devel_data);
        slm.get_nn()->stash_model_when_best(f1);
        update_recorder.update_training_state(f1, nr_epoch, nr_devel_order);
    };

    // for randomly select instance
    std::vector<unsigned> access_order(nr_samples);
    for( unsigned i = 0; i < nr_samples; ++i ) access_order[i] = i;

    unsigned line_cnt_for_devel = 0;
    unsigned long long total_time_cost_in_seconds = 0ULL;
    for( unsigned nr_epoch = 0; nr_epoch < opts.max_epoch ; ++nr_epoch )
    {
        std::cerr << "++ Epoch " << nr_epoch + 1 << "/" << max_epoch << " start ";
        // shuffle samples by random access order
        std::shuffle(access_order.begin(), access_order.end(), slm.get_mt19937());

        // For loss , accuracy , time cost report
        BasicStat training_stat_per_epoch;
        training_stat_per_epoch.start_time_stat();

        int nr_devel_order = 0; // for record
        // train for every Epoch 
        for( unsigned i = 0; i < nr_samples; ++i )
        {
            unsigned access_idx = access_order[i];
            const  typename SLModel::AnnotatedDataProcessedT &instance = training_data[i];
            // GO
            slm.build_training_graph(instance);
            slnn::type::real loss = slm.get_nn()->forward_as_scalar();
            slm.get_nn()->backward();
            slm.get_nn()->update(opts.training_update_scale);
            // record loss
            training_stat_per_epoch.loss += loss;
            training_stat_per_epoch.total_tags += instance.size() ;
            if( 0 == (i + 1) % trivial_report_freq ) // Report 
            {
                std::string trivial_header = std::to_string(i + 1) + " instances have been trained.";
                BOOST_LOG_TRIVIAL(trace) << training_stat_per_epoch.get_stat_str(trivial_header);
            }
            ++line_cnt_for_devel;
            // do devel at every `do_devel_freq` and if update error, just exit the current process
            if( 0 == line_cnt_for_devel % opts.do_devel_freq )
            {
                line_cnt_for_devel = 0; // clear
                do_devel_in_training(nr_epoch, ++nr_devel_order);
                if( !update_recorder.is_training_ok() ){ break;  }
            }
        }

        // End of an epoch 
        // 1. update epoch
        slm.get_nn()->update_epoch();
        // 2. end timing 
        training_stat_per_epoch.end_time_stat();
        // 3. output info at end of every eopch
        std::ostringstream tmp_sos;
        tmp_sos << "- Epoch " << nr_epoch + 1 << "/" << opts.max_epoch << " finished .\n";
        std::cerr << training_stat_per_epoch.get_stat_str(tmp_sos.str()) << "\n";
        total_time_cost_in_seconds += training_stat_per_epoch.get_time_cost_in_seconds();
        // do validation at every ends of epoch
        if( update_recorder.is_training_ok() )
        {
            std::cerr << "- Do validation at every ends of epoch .\n";
            do_devel_in_training(nr_epoch, -1); // -1 stands for the end of the epoch.
        }
        // check angin! (previous devel process may change the state.)
        if( !update_recorder.is_training_ok() ){ break; }
    }
    if( !update_recorder.is_training_ok() )
    { 
        std::cerr << "- Gradient may have been updated error ! Exit ahead of time.\n" ; 
    }
    std::cerr << "+ Training finished. Time cost:" << total_time_cost_in_seconds << "s, "
        << "best score(F1): " << update_recorder.get_best_score() << ", "
        << "at epoch: " << update_recorder.get_best_epoch() << ", "
        << "devel order: " << update_recorder.get_best_devel_order() << "\n";
}

} // end of namespace modelhandler
} // end of namespace segmentor
} // end of namespace slnn

#endif