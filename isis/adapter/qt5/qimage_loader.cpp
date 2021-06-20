//
// Created by enrico on 20.06.21.
//

#include "qimage_loader.hpp"

#include <utility>
#include "../../core/io_factory.hpp"
#include "common.hpp"

isis::qt5::QtImageLoader::QtImageLoader(QObject *parent,
										isis::util::slist _input,
										std::list<util::istring> _formatstack,
										std::list<util::istring> _dialects)
: QThread(parent), input(std::move(_input)),formatstack(std::move(_formatstack)),dialects(std::move(_dialects))
{}

void isis::qt5::QtImageLoader::run()
{
	util::slist rejected;
	auto tImages=data::IOFactory::load(input,formatstack,dialects,&rejected);

	if ( tImages.empty() ){
		LOG( Runtime, notice ) << "No data acquired from asynchronous load...";
	} else {
		for( auto a = tImages.begin(); a != tImages.end(); a++ ) {
			for( auto b = a; ( ++b ) != tImages.end(); ) {
				const data::Image &aref = *a, bref = *b;
				LOG_IF( aref.getDifference( bref ).empty(), Runtime, warning )
					<< "The metadata of the images "
				   << aref.identify(true,false) << ":" << std::distance<std::list<data::Image> ::const_iterator>( tImages.begin(), a )
				   << " and " << bref.identify(true,false) << ":" << std::distance<std::list<data::Image> ::const_iterator>( tImages.begin(), b )
				   << " are equal. Maybe they are duplicates.";
			}
		}
	}

	IsisImageList q_images;
	std::copy(tImages.begin(),tImages.end(),std::back_inserter(q_images));
	emit imagesReady(q_images, slist_to_QStringList(rejected));
}
