#pragma once

#include "../../core/message.hpp"
#include <QString>
#include <QObject>
#include <QDateTime>

namespace isis::qt5
{

class QDefaultMessageHandler : public util::MessageHandlerBase
{
public:
	void commit( const util::Message &msg ) override;
	void qmessageBelow( LogLevel level );
	explicit QDefaultMessageHandler( LogLevel level );
	~QDefaultMessageHandler() override;
private:
	LogLevel m_PushMsgBoxLogLevel;

};

}
