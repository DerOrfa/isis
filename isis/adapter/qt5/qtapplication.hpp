/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <QGuiApplication>

#include "../../core/io_application.hpp"
#include "../../core/io_factory.hpp"
#include "qdefaultmessageprint.hpp"
#include "qimage_loader.hpp"
#include "common.hpp"
#include <QApplication>


namespace isis::qt5
{
//template<typename Obj> using log_receive_slot = void (Obj::*) (qt5::LogEvent event);

namespace _internal{
	template<class ISISApp> class QtApplicationBase : public ISISApp{
		std::unique_ptr<QApplication> m_qapp;
	protected:
		void _init( int &argc, char **argv)
		{
			if ( m_qapp ) {
				LOG( Debug, error ) << "The QApplication already exists. This should not happen. I'll not touch it";
			} else{
				m_qapp = std::make_unique<QApplication>(argc, argv);
			}
		}
	public:
		template<class... Args> explicit QtApplicationBase(Args&&... args): ISISApp(args...){}
		std::shared_ptr<isis::util::MessageHandlerBase> makeLogHandler(isis::LogLevel level) const
		{
			return std::shared_ptr< isis::util::MessageHandlerBase >( level ? new isis::qt5::QDefaultMessageHandler( level ) : 0 );
		}
		int exec(){
			if(m_qapp)
				return m_qapp->exec();
			else
				LOG( Debug, error ) << "The QApplication was not yet created, you should run init() before using getQApplication.";
			return -1;
		}
		virtual bool init( int &argc, char **argv, bool exitOnError )
		{
			_init(argc,argv);
			return ISISApp::init( argc, argv, exitOnError );
		}
//		template<typename Obj> void registerLogReceiver(Obj* rec_obj, log_receive_slot<Obj> rec_slot){
//			qRegisterMetaType<isis::qt5::LogEvent>("qt5::LogEvent");
//			for(auto &handler:this->resetLogging()){
//				auto qHander = std::dynamic_pointer_cast<QDefaultMessageHandler>(handler);
//				if(handler)
//					QObject::connect(qHander.get(),&QDefaultMessageHandler::commitMessage,rec_obj,rec_slot);
//				else
//					LOG(Runtime,error) << "Log handler is not QDefaultMessageHandler, won't connect to " << rec_obj->objectName().toStdString();
//			}
//		}
	};
}

class QtApplication : public _internal::QtApplicationBase<util::Application>
{
public:
	explicit QtApplication( const char name[] );
};

class IOQtApplication : public _internal::QtApplicationBase<data::IOApplication>
{
public:
	explicit IOQtApplication( const char name[], bool have_input = true, bool have_output = true );
	template<typename Obj> QMetaObject::Connection asyncAutoload(Obj *load_rcv, image_receive_slot<Obj> load_slot, const std::string &param_suffix= "")
	{
		const util::slist input = parameters[std::string("in") + param_suffix];
		const util::slist rf = parameters[std::string("rf") + param_suffix];
		const util::slist dl = parameters[std::string("rdialect") + param_suffix];

		const std::list<util::istring> formatstack = util::makeIStringList(rf);

		LOG(Runtime, info)
			<< "loading " << util::MSubject(input)
			<< util::NoSubject(rf.empty() ? "" : std::string(" using the format stack: ") + util::listToString(rf.begin(), rf.end()))
			<< util::NoSubject((!rf.empty() && !dl.empty()) ? " and" : "")
			<< util::NoSubject(dl.empty() ? "" : std::string(" using the dialect: ") + util::listToString(dl.begin(), dl.end()));
		return asyncLoad(input,formatstack,util::makeIStringList(dl),load_rcv,load_slot);
	}
	template<typename Obj> Obj* init( int &argc, char **argv, image_receive_slot<Obj> load_slot, bool exitOnError = true )
	{
		_init(argc,argv);
		if ( !util::Application::init( argc, argv, exitOnError )  )
			return nullptr;

		auto load_rcv = new Obj;
		if ( m_input ) {
			QMetaObject::Connection c= asyncAutoload(load_rcv, load_slot);
			LOG_IF(!c,Runtime,error) << "Failed to create a connection with the receiver of the asynchronous load";
		}
		return load_rcv;
	}
};

}
