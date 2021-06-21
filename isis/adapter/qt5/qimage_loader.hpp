//
// Created by enrico on 20.06.21.
//

#pragma once

#include "../../core/image.hpp"
#include <QList>
#include <QThread>

Q_DECLARE_TYPEINFO(isis::data::Image,Q_COMPLEX_TYPE);
namespace isis::qt5
{
typedef QList<data::Image> IsisImageList;
}
Q_DECLARE_METATYPE(isis::qt5::IsisImageList);

namespace isis::qt5
{
template<typename Obj> using image_receive_slot = void (Obj::*) (isis::qt5::IsisImageList images,QStringList rejects);

class QtImageLoader : public QThread
{
	Q_OBJECT
private:
	isis::util::slist input;
	std::list<util::istring> formatstack,dialects;
public:
	QtImageLoader(QObject *parent,util::slist input, std::list<util::istring> formatstack, std::list<util::istring> dialects);
	void run()override;
Q_SIGNALS:
	void imagesReady(isis::qt5::IsisImageList images,QStringList rejects);
};

template<typename Obj> QMetaObject::Connection asyncLoad(const util::slist &input,
			   const std::list<util::istring> &formatstack,
			   const std::list<util::istring> &dialects,
			   Obj* rec_obj, image_receive_slot<Obj> rec_slot)
{
	qRegisterMetaType<IsisImageList>();
	util::vector4<int> x;
	auto loader = new QtImageLoader(rec_obj,input,formatstack,dialects);
	loader->connect(loader, &QtImageLoader::finished, &QObject::deleteLater);
	QMetaObject::Connection c=QObject::connect(loader, &QtImageLoader::imagesReady, rec_obj, rec_slot);
	loader->start();
	return std::move(c);
}

}
