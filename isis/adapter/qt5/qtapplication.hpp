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

class QtApplication : public util::Application
{
	std::unique_ptr<QApplication> m_qapp;
public:
	QApplication &getQApplication();
	explicit QtApplication( const char name[] );
	virtual bool init( int &argc, char **argv, bool exitOnError = true );
	[[nodiscard]] virtual std::shared_ptr<util::MessageHandlerBase> getLogHandler( std::string module, isis::LogLevel level )const;
	int exec();
};

class IOQtApplication : public data::IOApplication
{
	int m_argc; //same as above
	char **m_argv;
	std::unique_ptr<QApplication> m_qapp;
	bool _init(int &argc, char **argv);
public:
	QApplication &getQApplication();
	explicit IOQtApplication( const char name[], bool have_input = true, bool have_output = true );
	template<typename Obj> QMetaObject::Connection async_autoload(Obj *load_rcv, image_receive_slot<Obj> load_slot,const std::string &param_suffix="")
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
	virtual bool init( int &argc, char **argv, bool exitOnError = true );
	template<typename Obj> Obj* init( int &argc, char **argv, image_receive_slot<Obj> load_slot, bool exitOnError = true )
	{
		const bool ok = _init(argc,argv) && util::Application::init( argc, argv, exitOnError );
		if ( !ok  )
			return nullptr;

		auto load_rcv = new Obj;
		if ( m_input ) {
			QMetaObject::Connection c=async_autoload(load_rcv,load_slot);
			LOG_IF(!c,Runtime,error) << "Failed to create a connection with the receiver of the asynchronous load";
		}
		return load_rcv;
	}
	[[nodiscard]] virtual std::shared_ptr<util::MessageHandlerBase> getLogHandler( std::string module, isis::LogLevel level )const;
	int exec();
};

}
