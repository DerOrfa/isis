#include "qvariant_adapter.hpp"
#include <QVariant>
#include <variant>
#include <QColor>
#include <QVector3D>
#include <QVector4D>
#include <QDateTime>

namespace isis::qt5::_internal{
/// Value => Variant
template<typename T> QVariant makeQVariant_impl(const T &value){
	return QVariant(value);
}
template<> QVariant makeQVariant_impl<int64_t>(const int64_t &value){
	return QVariant(qint64(value));
}
template<> QVariant makeQVariant_impl<uint64_t>(const uint64_t &value){
	return QVariant(quint64(value));
}
template<> QVariant makeQVariant_impl(const util::color24 &value){
	return QVariant(QColor::fromRgb(value.r,value.g,value.b));
}
template<> QVariant makeQVariant_impl(const util::color48 &value){
	return QVariant(QColor::fromRgba64(value.r,value.g,value.b));
}
template<typename T> QVariant makeQVariant_impl(const util::vector3<T> &value){
	return QVariant(QVector3D(value[0],value[1],value[2]));
}
template<typename T> QVariant makeQVariant_impl(const util::vector4<T> &value){
	return QVariant(QVector4D(value[0],value[1],value[2],value[3]));
}
template<typename T> QVariant makeQVariant_impl(const std::list<T> &value){
	QVariantList ret;
	std::transform(
		value.begin(),value.end(),
		std::back_inserter(ret),
		[](const T &v){return makeQVariant_impl<T>(v);}
	);
	return ret;
}
template<> QVariant makeQVariant_impl<std::string>(const std::string &value){
	return QVariant(QString::fromStdString(value));
}
template<typename T> QVariant makeQVariant_impl(const std::complex<T> &value){
	//@todo implement me
	return QVariant("IMPLEMENT_ME");
}
template<> QVariant makeQVariant_impl(const std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds> &value){
	return QDateTime::fromMSecsSinceEpoch(value.time_since_epoch().count());
}
template<> QVariant makeQVariant_impl(const util::date &value){
	const util::timestamp msec=std::chrono::time_point_cast<std::chrono::milliseconds>(value);
	return QDateTime::fromMSecsSinceEpoch(msec.time_since_epoch().count()).date();
}
template<> QVariant makeQVariant_impl(const util::duration &value){
	//@todo implement me
	return QVariant("IMPLEMENT_ME");
}
}

QVariant isis::qt5::makeQVariant(const isis::util::Value &val)
{
	return std::visit([](const auto &value)->QVariant{
		return _internal::makeQVariant_impl(value);
	},static_cast<const isis::util::ValueTypes&>(val));
}
