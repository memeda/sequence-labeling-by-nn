#ifndef SLNN_SEGMENTER_CWS_MODULE_LEXICON_FEATURE_H_
#define SLNN_SEGMENTER_CWS_MODULE_LEXICON_FEATURE_H_

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iostream>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/unordered_set.hpp>

#include "utils/typedeclaration.h"
#include "utils/utf8processing.hpp"
namespace slnn{

struct LexiconFeatureData
{
    unsigned char start_here_feature_index;
    unsigned char pass_here_feature_index;
    unsigned char end_here_feature_index;
    // start_here_feature_index = 0 for word with 1 character
    // pass_here_feature_index = 0 for word with less than 3 character
    // end_here_feature_index = 0 for word with 1 character
    LexiconFeatureData() : start_here_feature_index(0), pass_here_feature_index(0), end_here_feature_index(0){} 
    unsigned char get_start_here_feature_index() const { return start_here_feature_index; }
    unsigned char get_pass_here_feature_index() const { return pass_here_feature_index; }
    unsigned char get_end_here_feature_index() const { return end_here_feature_index; }
    
    void set_start_here_feature(unsigned char word_len_start_here)
    {
        assert(word_len_start_here >= 1U);
        set_start_here_feature_index(word_len_start_here - 1);
    }
    void set_pass_here_feature(unsigned char word_len_pass_here)
    {
        assert(word_len_pass_here >= 3U);
        set_pass_here_feature_index(word_len_pass_here - 2);
    }
    void set_end_here_feature(unsigned char word_len_end_here)
    {
        assert(word_len_end_here >= 1U);
        set_end_here_feature_index(word_len_end_here - 1);
    }
private:
    void set_start_here_feature_index(unsigned char val){ start_here_feature_index = val; } // no need do max
    void set_pass_here_feature_index(unsigned char val){ pass_here_feature_index = std::max(pass_here_feature_index, val); }
    void set_end_here_feature_index(unsigned char val){ end_here_feature_index = std::max(end_here_feature_index, val); }
};

using LexiconFeatureDataSeq = std::vector<LexiconFeatureData>;

class LexiconFeature
{
    friend class boost::serialization::access;
public :
    // variable (functtion)
    static constexpr unsigned WordLenLimit(){ return 5u;  }

public :
    LexiconFeature() = default; // for load 
    LexiconFeature(unsigned start_here_feature_dim, unsigned end_here_feature_dim, unsigned pass_here_feature_dim);
    LexiconFeature(unsigned featureDim) : LexiconFeature(featureDim, featureDim, featureDim){};

    void set_dim(unsigned start_here_feature_dim, unsigned end_here_feature_dim, unsigned pass_here_feature_dim);

    unsigned get_start_here_dict_size() const { return WordLenLimit(); }
    unsigned get_pass_here_dict_size() const { return WordLenLimit() - 1; }
    unsigned get_end_here_dict_size() const { return WordLenLimit(); }

    unsigned get_start_here_feature_dim() const { return start_here_feature_dim; }
    unsigned get_pass_here_feature_dim() const { return pass_here_feature_dim; }
    unsigned get_end_here_feature_dim() const { return end_here_feature_dim; }
    unsigned get_feature_dim() const { return start_here_feature_dim + pass_here_feature_dim + end_here_feature_dim; }

    void count_word_frequency(const Seq &word_seq);
    void build_lexicon();
    void extract(const Seq &char_seq, LexiconFeatureDataSeq &lexicon_feature_seq) const ;

    std::string get_feature_info() const;

    template<typename Archive>
    void serialize(Archive &ar, unsigned version);

    // DEBUG
    void debug_print_lexicon()
    {
        for( auto iter = lexicon.begin(); iter != lexicon.end(); ++iter )
        {
            std::cerr << *iter << " ";
        }
        std::cerr << "\n";
    }
    void debug_lexicon_feature_seq(const Seq &char_seq, const LexiconFeatureDataSeq &lexicon_feature_seq)
    {
        for( size_t i = 0; i < char_seq.size(); ++i )
        {
            std::cerr << char_seq[i] << " " 
                << static_cast<int>(lexicon_feature_seq[i].start_here_feature_index) 
                << " " 
                << static_cast<int>(lexicon_feature_seq[i].pass_here_feature_index)
                << " " 
                << static_cast<int>(lexicon_feature_seq[i].end_here_feature_index) ;
        }
        std::cerr << "\n";
    }

private:
    unsigned start_here_feature_dim;
    unsigned pass_here_feature_dim;
    unsigned end_here_feature_dim;
    unsigned lexicon_word_max_len;
    unsigned freq_threshold; // >= freq_threshold can be add to lexicon, it will be calculate automatically
    std::unordered_map<std::string, unsigned> word_count_dict;
    std::unordered_set<std::string> lexicon;
};

template<typename Archive>
void LexiconFeature::serialize(Archive &ar, unsigned version)
{
    ar & start_here_feature_dim & end_here_feature_dim
        & pass_here_feature_dim & lexicon_word_max_len
        & freq_threshold;
    ar & lexicon;
}

} // end of namespace slnn
#endif