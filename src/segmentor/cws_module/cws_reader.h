#ifndef SEGMENTOR_CWS_MODULE_CWS_READER_H_
#define SEGMENTOR_CWS_MODULE_CWS_READER_H_
#include <functional>
#include <memory>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "utils/reader.hpp"
#include "utils/typedeclaration.h"
#include "trivial/charcode/charcode_base.hpp"
#include "trivial/charcode/charcode_convertor.h"
namespace slnn{

/**************
 * below implementation will be abandon in future.
 **************/
class CWSReader : public Reader
{
public:
    static const char *CWSWordSeperator;

public:
    CWSReader(std::istream &is);
    bool read_segmented_line(Seq &word_seq);
    bool readline(Seq &char_seq);
};

namespace segmentor{
namespace reader{

namespace reader_inner{

inline 
bool is_seg_delimiter(char32_t uc)
{
    return uc == U'\t';
}

}

class SegmentorUnicodeReader : public Reader
{
public:
    using predicateT = std::function<bool(char32_t)>;
public:
    SegmentorUnicodeReader(std::istream &is, 
        charcode::EncodingType file_encoding=charcode::EncodingType::UTF8, 
        predicateT pred_func=reader_inner::is_seg_delimiter);
    bool read_segmented_line(std::vector<std::u32string> &out_wordseq);
    bool readline(std::u32string &out_charseq);
private:
    std::shared_ptr<charcode::CharcodeConvertor> conv;
    predicateT pred_func;
};


} // end of namespace reader
} // end of namespace segmentor
} // end of namespace slnn
#endif