#pragma once
#include "common.hpp"
#include <QImage>

namespace isis::qt5{

void fillQImage(QImage &dst, const data::ValueArray &slice, size_t line_length, data::scaling_pair scaling = data::scaling_pair() );
void fillQImage(QImage &dst, const data::ValueArray &slice, size_t line_length, const std::function<void (uchar *, const data::ValueArray &)> &transfer_function );

void fillQImage(QImage &dst, const std::vector<data::ValueArray> &lines, data::scaling_pair scaling = data::scaling_pair() );
void fillQImage(QImage &dst, const std::vector<data::ValueArray> &lines, const std::function<void (uchar *, const data::ValueArray &)> &transfer_function );

QImage makeQImage(const data::ValueArray &slice, size_t line_length, data::scaling_pair scaling = data::scaling_pair() );
QImage makeQImage(const data::ValueArray &slice, size_t line_length, const std::function<void (uchar *, const data::ValueArray &)> &transfer_function);

QImage makeQImage(const std::vector<data::ValueArray> &lines, data::scaling_pair scaling = data::scaling_pair() );
QImage makeQImage(const std::vector<data::ValueArray> &lines, const std::function<void (uchar *, const data::ValueArray &)> &transfer_function);

}
