#include "qdefaultmessageprint.hpp"
#include "qlogstore.hpp"
#include "../../core/singletons.hpp"
#include "../../core/image.hpp"
#include <QMessageBox>


isis::qt5::QDefaultMessageHandler::QDefaultMessageHandler( isis::LogLevel level )
: MessageHandlerBase( level ),  m_PushMsgBoxLogLevel(isis::error ){}

void isis::qt5::QDefaultMessageHandler::qmessageBelow ( isis::LogLevel level )
{
	m_PushMsgBoxLogLevel = level;
}

void isis::qt5::QDefaultMessageHandler::commit( const isis::util::Message &msg )
{
	auto &store=util::Singletons::get<QLogStore, 10>();
	store.add(msg);

	if(!store.isAttached()){ // fall back to util::DefaultMsgPrint of nobody is listening
		util::DefaultMsgPrint pr(m_level);
		pr.commit(msg);
	}

	if( m_PushMsgBoxLogLevel > msg.m_level ) {
		QMessageBox msgBox;
		LogEvent qMessage(msg);
		std::string level;

		switch( msg.m_level ) {
		case isis::verbose_info:
		case isis::info:
		case isis::notice:
			msgBox.setIcon( QMessageBox::Information );
			break;
		case isis::warning:
			msgBox.setIcon( QMessageBox::Warning );
			break;
		case isis::error:
			msgBox.setIcon( QMessageBox::Critical );
			break;
		}

 		std::stringstream text;
 		text << util::logLevelName( msg.m_level ) << " in " << msg.m_file << ":" << qMessage.m_line;
		msgBox.setWindowTitle( QString("%1 (%2)").arg(qMessage.m_module).arg(qMessage.m_timeStamp.toString()) );
 		msgBox.setText( text.str().c_str() );
		msgBox.setInformativeText( qMessage.merge() );
 		msgBox.exec();
	}
}

isis::qt5::QDefaultMessageHandler::~QDefaultMessageHandler(){}
