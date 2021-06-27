//
// Created by enrico on 23.06.21.
//

#pragma once

#include <QAbstractButton>
#include "qdefaultmessageprint.hpp"

QT_BEGIN_NAMESPACE
namespace Ui
{
class QLogWidget;
}
QT_END_NAMESPACE

namespace isis::qt5
{
class QLogWidget : public QWidget
{
Q_OBJECT
	QAbstractButton *feedback_button=nullptr;
protected:
	QMetaObject::Connection feedback_connection;
	void showLevel(LogLevel level, bool show=true);
	void hideLevel(LogLevel level);
public:
	bool registerButton(QAbstractButton *btn, bool use_as_feedback);
	explicit QLogWidget(QWidget *parent = nullptr);
	~QLogWidget() override;
private:
	Ui::QLogWidget *ui;
public Q_SLOTS:
	void onLogEvent(int);
	void onShowError(bool);
	void onShowWarning(bool);
	void onShowNotice(bool);
	void onShowInfo(bool);
	void onShowVerbose(bool);
};
}