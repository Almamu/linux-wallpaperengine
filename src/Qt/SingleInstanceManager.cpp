#include "SingleInstanceManager.h"
#include <qchar.h>
#include <qlocalserver.h>
#include <qlocalsocket.h>
#include <qobject.h>

SingleInstanceManager::SingleInstanceManager(const QString& name, QObject* parent) : QObject(parent), m_serverName(name) {
  this->server = new QLocalServer(this);
}

SingleInstanceManager::~SingleInstanceManager() {
  cleanUpServer();
}

void SingleInstanceManager::cleanUpServer() {
  if(server->isListening()) {
    server->close();
  }
}

bool SingleInstanceManager::tryListen() {
  if (!server->listen(m_serverName)) return false;

  connect(server, &QLocalServer::newConnection, this, [this]() {
    auto client = server->nextPendingConnection();
    connect(client, &QLocalSocket::readyRead, client, [this, client]() {
      emit messageReceived(client->readAll());
      client->disconnectFromServer();
    });
  });
  return true;
}
