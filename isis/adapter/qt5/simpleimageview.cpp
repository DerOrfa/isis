/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2016  Enrico Reimer <reimer@cbs.mpg.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "simpleimageview.hpp"
#include "gradientwidget.hpp"
#include "common.hpp"
#include "../../core/io_factory.hpp"
#include <QSlider>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QFileDialog>
#include <QGraphicsSceneEvent>

namespace isis{
namespace qt5{
namespace _internal{
	
TransferFunction::TransferFunction(std::pair<util::Value, util::Value> in_minmax): minmax(in_minmax)
{
	//we dont actually want the converter, but its scaling function
	//which is why we hard-wire it to double->uint8_t, so we always get a full scaling
	c=data::ValueArray::getConverterFromTo(util::typeID<double>(), util::typeID<uint8_t>());
	updateScale(0,1);
}

std::pair<double,double> TransferFunction::updateScale(qreal bottom, qreal top){
	const auto min = minmax.first.as<double>()+ bottom*(minmax.second.as<double>()-minmax.first.as<double>());
	const auto max = minmax.second.as<double>()*top;
	scale= c->getScaling(min,max);
	return std::pair<double,double>(min,max);
}

class MagnitudeTransfer : public TransferFunction{
	template<typename T> void transferFunc(uchar *dst, const data::TypedArray<std::complex<T>> &line)const{
		const T t_scale=scale.scale.as<T>();
		const T t_offset=scale.offset.as<T>();
		for(const std::complex<T> &v:line){
			*(dst++)=std::min<T>(std::abs(v)*t_scale+t_offset,0xFF);
		}
	}

public:
	MagnitudeTransfer(std::pair<util::Value, util::Value> minmax): TransferFunction(minmax){}
	void operator()(uchar * dst, const data::ValueArray & line) const override{
		switch(line.getTypeID()){
			case util::typeID<std::complex<float>>():
				transferFunc(dst,data::TypedArray<std::complex<float>>(line));
				break;
			case util::typeID<std::complex<double>>():
				transferFunc(dst,data::TypedArray<std::complex<double>>(line));
				break;
			default:
				LOG(Runtime,error) << line.typeName() << " is no supported complex-type";
		}
	}
};

class PhaseTransfer : public TransferFunction{
	
	template<typename T> void transferFunc(uchar *dst, const data::TypedArray<std::complex<T>> &line)const{
		const T scale=M_PI/128;
		for(const std::complex<T> &v:line){
			*(dst++)=std::arg(v)*scale+128;
		}
	}

public:
	PhaseTransfer():TransferFunction(std::pair<util::Value, util::Value>(util::Value(-180), util::Value(180))){}
	void operator()(uchar * dst, const data::ValueArray & line) const override{
		switch(line.getTypeID()){
			case util::typeID<std::complex<float>>():
				transferFunc(dst,data::TypedArray<std::complex<float>>(line));
				break;
			case util::typeID<std::complex<double>>():
				transferFunc(dst,data::TypedArray<std::complex<double>>(line));
				break;
			default:
				LOG(Runtime,error) << line.typeName() << " is no supported complex-type";
		}
	}
};

class LinearTransfer : public TransferFunction{
public:
	LinearTransfer(std::pair<util::Value, util::Value> minmax): TransferFunction(minmax){}
	void operator()(uchar * dst, const data::ValueArray & line) const override{
		if(line.isFloat() || line.isInteger() ) {
			line.copyToMem<uint8_t>(dst, line.getLength(), scale);
		} else if(line.is<util::color24>() || line.is<util::color48>()){
			auto *c_dst=reinterpret_cast<QRgb*>(dst);
			for(const util::color24 &c:data::TypedArray<util::color24>(line,scale))
				*(c_dst++)=qRgb(c.r,c.g,c.b);
		} else {
			LOG(Runtime,error) << "Sorry " << line.typeName() << " data cannot be displayed";
		}
	}
};

class MaskTransfer : public TransferFunction{
public:
	MaskTransfer():TransferFunction({0,1}){}
	void operator()(uchar * dst, const data::ValueArray & line) const override{
		for(const bool &c:data::TypedArray<bool>(line)){
			if(c)
				*(dst++)=255;
			else 
				*(dst++)=0;
		}
	}
};

MriGraphicsView::MriGraphicsView(QWidget *parent):QGraphicsView(parent),crossHair(std::numeric_limits<qreal>::quiet_NaN(),std::numeric_limits<qreal>::quiet_NaN()){
 	setCursor(Qt::BlankCursor);
}
void MriGraphicsView::wheelEvent(QWheelEvent * event) {
	setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

	if(event->angleDelta().x()>0)
		scale(1.1,1.1);
	else
		scale(0.9,0.9);
}
void MriGraphicsView::mouseMoveEvent(QMouseEvent * event){
	if(event->buttons()==0)
		mouseMoved(mapToScene(event->pos()));
	QGraphicsView::mouseMoveEvent(event);
}
void MriGraphicsView::mousePressEvent(QMouseEvent * event){
	setDragMode(QGraphicsView::ScrollHandDrag);
	QGraphicsView::mousePressEvent(event);
}
void MriGraphicsView::mouseReleaseEvent(QMouseEvent * event){
	setDragMode(QGraphicsView::NoDrag);
	QGraphicsView::mousePressEvent(event);
}
void MriGraphicsView::drawForeground(QPainter * painter, const QRectF & rect){
	QGraphicsView::drawForeground(painter,rect);
	if(std::isnan(crossHair.x()) || std::isnan(crossHair.x()))
		return;
	
	painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	QPen pen(QColor(0xff, 0xff, 0xff));
	pen.setWidth(0);
	painter->setPen(pen);

	painter->drawLine(QLineF(crossHair.x(),rect.top(),crossHair.x(),rect.bottom()));
	painter->drawLine(QLineF(rect.right(),crossHair.y(),rect.left(),crossHair.y()));
}
void MriGraphicsView::moveCrosshair(QPointF to){
	crossHair=to;
	scene()->update();
}



} //namespace _internal

void SimpleImageView::setupUi(){

	QGridLayout *gridLayout = new QGridLayout(this);

	sliceSelect = new QSlider(this);
	sliceSelect->setMinimum(1);
	sliceSelect->setOrientation(Qt::Vertical);
	sliceSelect->setTickPosition(QSlider::TicksBelow);

	gridLayout->addWidget(sliceSelect, 0, 1, 1, 1);

    graphicsView = new _internal::MriGraphicsView(this);
	gridLayout->addWidget(graphicsView, 0, 0, 1, 1);
	graphicsView->setMouseTracking(true);

	timeSelect = new QSlider(this);
	timeSelect->setMinimum(1);
	timeSelect->setOrientation(Qt::Horizontal);
	timeSelect->setTickPosition(QSlider::TicksBelow);
	gridLayout->addWidget(timeSelect, 2, 0, 1, 1);

	QFrame *coordfield = new QFrame(this);
	gridLayout->addWidget(coordfield,1,0,1,1);
	new QHBoxLayout(coordfield);
	
	coordfield->layout()->addWidget(pos_label=new QLabel());
	coordfield->layout()->addWidget(value_label=new QLabel());

	QPushButton *savebtn=new QPushButton("save",this);
	gridLayout->addWidget(savebtn, 1, 1, 1, 2);
	connect(savebtn, SIGNAL(clicked(bool)), SLOT(doSave()));
	
	connect(timeSelect, SIGNAL(valueChanged(int)), SLOT(timeChanged(int)));
	connect(sliceSelect, SIGNAL(valueChanged(int)), SLOT(sliceChanged(int)));
	
	if(type==complex){
		QGroupBox *groupBox = new QGroupBox("complex representation");
		transfer_function_group = new QButtonGroup(groupBox);
		QHBoxLayout *vbox = new QHBoxLayout;
		groupBox->setLayout(vbox);
		
		QRadioButton *mag=new QRadioButton("magnitude"),*pha=new QRadioButton("phase");
		vbox->addWidget(mag);
		vbox->addWidget(pha);
		transfer_function_group->addButton(mag,1);
		transfer_function_group->addButton(pha,2);
		transfer_function_group->setExclusive(true);
		mag->setChecked(true);
		gridLayout->addWidget(groupBox,2,0,1,1);
	}
}

SimpleImageView::SimpleImageView(data::Image img, QString title, QWidget *parent):QWidget(parent),m_img(img)
{
	const data::Chunk &firstChunk=img.getChunkAt(0);
	switch(firstChunk.getTypeID())//@todo a bit of a dirty trick to avoid expensive getMajorTypeID
	{
		case util::typeID<std::complex<float>>():
		case util::typeID<std::complex<double>>():
			type=complex;
			break;
		case util::typeID<util::color24>():
		case util::typeID<util::color48>():
			type=color;
			break;
		case util::typeID<bool>():
			type=mask;
			break;
		default:
			if(firstChunk.isFloat() || firstChunk.isInteger() )
				type=normal;
			else
				type=unsupported;

			break;
	}
	
	setupUi();
	
	if(title.isEmpty())
		title= QString::fromStdString(img.identify(true,false));
	
	if(!title.isEmpty())
		setWindowTitle(title);
	
	const float dpmm_x=physicalDpiX()/25.4, dpmm_y=physicalDpiY()/25.4;
	
	auto voxelsize=img.getValueAs<util::fvector3>("voxelSize");
	graphicsView->scale(voxelsize[0]*dpmm_x,voxelsize[1]*dpmm_y);
	
	auto minmax=img.getMinMax();
	if(type==color){
		auto first_c= minmax.first.as<util::color48>();
		auto second_c=minmax.second.as<util::color48>();
		minmax.first = (first_c.r+first_c.g+first_c.b)/3.0;
		minmax.second= (second_c.r+second_c.g+second_c.b)/3.0;
		transfer_function.reset(new _internal::LinearTransfer(minmax));
	} else if(type==complex){
		magnitude_transfer.reset(new _internal::MagnitudeTransfer(minmax));
		phase_transfer.reset(new _internal::PhaseTransfer);
		
		transfer_function=magnitude_transfer;
		connect(transfer_function_group, SIGNAL(buttonToggled(int, bool)),SLOT(selectTransfer(int,bool)));
	} else if(type==mask){
		transfer_function.reset(new _internal::MaskTransfer());
	} else if(type==normal){
		transfer_function.reset(new _internal::LinearTransfer(minmax));
	} else {
		LOG(Runtime,error) << "Sorry " << firstChunk.typeName() << " images cannot be displayed";
	}
	
	const std::array<size_t,4> img_size= img.getSizeAsVector();
	m_img.spliceDownTo(data::sliceDim);

	if(img_size[data::sliceDim]>1)
		sliceSelect->setMaximum(img_size[data::sliceDim]);
	else
		sliceSelect->setEnabled(false);

	if(img_size[data::timeDim]>1)
		timeSelect->setMaximum(img_size[data::timeDim]);
	else
		timeSelect->setEnabled(false);

	graphicsView->setScene(new QGraphicsScene(0,0,img_size[data::rowDim],img_size[data::columnDim],graphicsView));
	if(img_size[data::sliceDim]>1)
		sliceSelect->setValue(img_size[data::sliceDim]/2);
	
	if(type!=mask){
		QWidget *gradient;
		if(img.hasProperty("window/max") && img.hasProperty("window/min")){
			const double min=minmax.first.as<double>(),max=minmax.second.as<double>();
			const double hmin=img.getValueAs<double>("window/min"),hmax=img.getValueAs<double>("window/max");
			double bottom=(hmin-min) / (max-min);
			double top= hmax / max;

			if(std::isinf(bottom))bottom=0;
			if(std::isinf(top))top=1;

			transfer_function->updateScale(bottom,top);
			
			gradient=new GradientWidget(this,std::make_pair(minmax.first.as<double>(),minmax.second.as<double>()),bottom,top);
		} else 
			gradient=new GradientWidget(this,std::make_pair(minmax.first.as<double>(),minmax.second.as<double>()));
		
		dynamic_cast<QGridLayout*>(layout())->addWidget(gradient,0,2,1,1);
		connect(gradient,SIGNAL(scaleUpdated(qreal, qreal)),SLOT(reScale(qreal,qreal)));
	}

	updateImage();
}

void SimpleImageView::sliceChanged(int slice){
	curr_slice=slice-1;
	updateImage();
}
void SimpleImageView::timeChanged(int time)
{
	curr_time=time-1;
	updateImage();
}
void SimpleImageView::updateImage()
{
	graphicsView->scene()->clear();
	if(!transfer_function) //@todo generate something to show to the user
		return;
	auto transfer=transfer_function; //lambdas cannot bind members ??
	QImage qimage;
	
	const auto ch_dims=m_img.getChunkAt(0,false).getRelevantDims();
	
	assert(ch_dims<=data::sliceDim); // we at least should have a sliced image (the constructor should have made sure of that)
	if(ch_dims==data::sliceDim){ // we have a sliced image
		qimage = makeQImage(
			m_img.getChunk(0,0,curr_slice,curr_time),
			m_img.getDimSize(data::rowDim),
			[transfer](uchar *dst, const data::ValueArray &line){
				transfer->operator()(dst,line);
			}
		);
	} else { // if we have a "lines-image"
		std::vector<data::ValueArray> lines(m_img.getDimSize(1));
		for(size_t l=0;l<m_img.getDimSize(1);l++){
			lines[l]=m_img.getChunk(0,l,curr_slice,curr_time);
		}
		qimage = makeQImage(
			lines,
			[transfer](uchar *dst, const data::ValueArray &line){
				transfer->operator()(dst,line);
			}
		);
	}

	graphicsView->scene()->addPixmap(QPixmap::fromImage(qimage));
	connect(graphicsView,SIGNAL(mouseMoved(QPointF)),SLOT(onMouseMoved(QPointF)));
}
void SimpleImageView::selectTransfer(int id, bool checked)
{
	if(checked){//prevent the toggle-off from triggering a useless redraw
		transfer_function= (id==1?magnitude_transfer:phase_transfer);
		updateImage();
	}
}

void SimpleImageView::reScale(qreal bottom, qreal top)
{
	std::pair<double,double> window=transfer_function->updateScale(bottom,top);
	m_img.setValueAs("window/min",window.first);
	m_img.setValueAs("window/max",window.second);
	updateImage();
}
void SimpleImageView::doSave(){
	data::IOFactory::write(m_img,QFileDialog::getSaveFileName(this,"Store image as..").toStdString());
}
void SimpleImageView::onMouseMoved(QPointF pos){
	graphicsView->moveCrosshair(pos);
	pos_label->setText(QString("Position: %1-%2").arg(pos.x()).arg(pos.y()));
	
	const QRect rect(0,0,m_img.getDimSize(data::rowDim),m_img.getDimSize(data::rowDim));
	
	if(rect.contains(pos.toPoint())){
		const std::string value=m_img.getVoxelValue(pos.x(),pos.y(),curr_slice,curr_time).toString();
		value_label->setText(QString("Value: ")+QString::fromStdString(value));
	} else 
		value_label->setText(QString("Value: --"));
}
}
}
