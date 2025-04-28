#pragma once
#include <QObject>
#include <qlocalserver.h>
#include <QLocalSocket>
#include <qobject.h>
#include <qobjectdefs.h>
#include <QByteArray>

class SingleInstanceManager : public QObject {
  Q_OBJECT

  public:
  SingleInstanceManager(const QString& serverName, QObject* parent = nullptr);
  ~SingleInstanceManager() override;

  void cleanUpServer();
  
  bool tryListen();
  void sendMessage(const QByteArray& message);

  signals:
  void messageReceived(const QByteArray& message);

  private:
  QLocalServer* server;
  QString m_serverName;
};
