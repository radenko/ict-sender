#ifndef RECONNECTOR_H
#define RECONNECTOR_H
#include "pollfds.h"
#include "socket.h"
#include <poll.h>
#include <pthread.h>
#include <set>
#include <map>
#include <sys/socket.h>
#include <sys/time.h>
#include <algorithm>

using namespace std;

void *tryReconnectThreadFcn(void *arg);

typedef void (*OnReconnectedFcn) (socket_t *socket,void *ptr);
typedef set<Pollfds *> PollfdsList;
typedef PollfdsList::iterator PollfdsListIterator;
typedef set<fd_set *> FdsetList;
typedef FdsetList::iterator FdsetListIterator;
typedef set<pollfd *> PolfdList;
typedef PolfdList::iterator PolfdListIterator;

class ReconnectSocket {
	private:
		PollfdsList pollfdsList_;
		FdsetList fdsetList_;
		PolfdList polfdList_;
	public:
		OnReconnectedFcn onReconnectedFcn;
		void *arg;
		int events;
		socket_t *socket;
		int tryInterval;

		ReconnectSocket() {
			onReconnectedFcn=NULL;
			arg=NULL;
			events=-1;
		}
		
		bool reconnect() {
			int res=socket->connect();
			if (res<0) return false;

			doReconnected();
			return true;
		}

		void doReconnected() {
			for (PollfdsListIterator index=pollfdsList_.begin();index!=pollfdsList_.end();index++) {
				(*index)->add(socket->getHandle(),events!=-1?events:POLLIN);
			}
			for (FdsetListIterator index=fdsetList_.begin();index!=fdsetList_.end();index++) {
				FD_SET(socket->getHandle(),*index);
			}
			for (PolfdListIterator index=polfdList_.begin();index!=polfdList_.end();index++) {
				(*index)->fd=socket->getHandle();
				(*index)->events|=(events!=-1?events:POLLIN);
			}

			if (onReconnectedFcn) onReconnectedFcn(this->socket,arg);
		}
		
		void addFdset(fd_set *set) {
			fdsetList_.insert(set);
		}

		void addPollfd(pollfd *set) {
			polfdList_.insert(set);
		}

		void addPollfds(Pollfds *list) {
			pollfdsList_.insert(list);
		}
};

class Reconnector {
	private:
		vector<ReconnectSocket> reconnectSockets_;
		pthread_t reconnectThread_;
		timeval lastTry_;
	public:
		ReconnectSocket &add(socket_t *socket,OnReconnectedFcn onReconnectedFcn,void *arg) {

			reconnectSockets_.resize(reconnectSockets_.size()+1);
			ReconnectSocket &reconnectSocket=reconnectSockets_.back();

			reconnectSocket.arg=arg;
			reconnectSocket.socket=socket;
			reconnectSocket.onReconnectedFcn=onReconnectedFcn;

			return reconnectSocket;
		}

		void erase(const socket_t &socket) {
			vector<ReconnectSocket>::iterator socketRecon;

			for (socketRecon=reconnectSockets_.begin();socketRecon!=reconnectSockets_.end();socketRecon++) {
				if (socketRecon->socket->getHandle()==socket.getHandle()) break;
			}
			
			if (socketRecon!=reconnectSockets_.end()) reconnectSockets_.erase(socketRecon);

//			socketRecon=find(reconnectSockets_.begin(),reconnectSockets_.end(),socket);
//TODO			reconnectSockets_.erase(socket);
		}

		void erase(const socket_t *socket) {
			vector<ReconnectSocket>::iterator socketRecon;

			for (socketRecon=reconnectSockets_.begin();socketRecon!=reconnectSockets_.end();socketRecon++) {
				if (socketRecon->socket==socket) break;
			}
			
			if (socketRecon!=reconnectSockets_.end()) reconnectSockets_.erase(socketRecon);

//			socketRecon=find(reconnectSockets_.begin(),reconnectSockets_.end(),socket);
//TODO			reconnectSockets_.erase(socket);
		}

		void erase(const int socket) {
			vector<ReconnectSocket>::iterator socketRecon;

			for (socketRecon=reconnectSockets_.begin();socketRecon!=reconnectSockets_.end();socketRecon++) {
				if (socketRecon->socket->getHandle()==socket) break;
			}
			
			if (socketRecon!=reconnectSockets_.end()) reconnectSockets_.erase(socketRecon);

//			socketRecon=find(reconnectSockets_.begin(),reconnectSockets_.end(),socket);
//TODO			reconnectSockets_.erase(socket);
		}

		ReconnectSocket &at(const size_t pos) {
			return reconnectSockets_[pos];
		}

		ReconnectSocket &operator[] (const size_t pos) {
			return at(pos);
		}
		
		void run () {
			pthread_create(&reconnectThread_,NULL,&tryReconnectThreadFcn,this);
		}
		
		const bool empty() const {
			return reconnectSockets_.empty();
		}
		
		void tryReconnect() {
			vector<ReconnectSocket>::iterator reconnectSocket;

			do {
				for (reconnectSocket=reconnectSockets_.begin();reconnectSocket!=reconnectSockets_.end();reconnectSocket++) {
					if (  reconnectSocket->reconnect()  ) {
						reconnectSockets_.erase(reconnectSocket);
						break;
					}
				}
				
			} while (reconnectSocket!=reconnectSockets_.end());

		}
		
		void tryReconnect(const int tryInterval) {
			if (empty()) return;
			timeval now;
			gettimeofday(&now, NULL);
			
			if (now.tv_sec-lastTry_.tv_sec>tryInterval) {
				lastTry_=now;
				tryReconnect();
			}
		}
};

class pthread_reconnector_t:protected Reconnector {
	private:
		vector<ReconnectSocket> reconnectSockets_;
		pthread_t reconnectThread_;
		pthread_mutex_t locker;
	public:
		ReconnectSocket &add(socket_t *socket,OnReconnectedFcn onReconnectedFcn,void *arg) {
			pthread_mutex_lock(&locker);
			ReconnectSocket &result=Reconnector::add(socket,onReconnectedFcn,arg);
			pthread_mutex_unlock(&locker);
			return result;
		}
		
		pthread_reconnector_t() {
			pthread_mutex_init(&locker,NULL);
		}
		
		~pthread_reconnector_t() {
			pthread_mutex_destroy(&locker);
		}

		void erase(const socket_t &socket) {
			pthread_mutex_lock(&locker);
			Reconnector::erase(socket);
			pthread_mutex_unlock(&locker);
		}

		ReconnectSocket &at(const size_t pos) {
			pthread_mutex_lock(&locker);
			ReconnectSocket &result=Reconnector::at(pos);
			pthread_mutex_unlock(&locker);
			return result;
		}

		ReconnectSocket &operator[] (const size_t pos) {
			return at(pos);
		}
		
		void run () {
			Reconnector::run();
		}
		
		void tryReconnect() {
			pthread_mutex_lock(&locker);
			Reconnector::tryReconnect();
			pthread_mutex_unlock(&locker);
		}

		const bool empty() {
			pthread_mutex_lock(&locker);
			bool result=Reconnector::empty();
			pthread_mutex_unlock(&locker);
			return result;
		}
};

void *tryReconnectThreadFcn(void *arg) {
	Reconnector *reconnector=(Reconnector *)arg;
	reconnector->tryReconnect();
}
#endif
