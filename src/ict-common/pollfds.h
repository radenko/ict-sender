#ifndef POLLFDS_H
#define POLLFDS_H
#include "dynarray.h"
#include <poll.h>

typedef DynArray<pollfd> dynpollfd;
class Pollfds:private dynpollfd {
	public:
		size_t add(const int fd,const int events,const bool replace=true) {
			size_t i=getIndex(fd);
			if (i!=npos) {
				if (replace) at(i).events=events;
				return i;
			} else {
				pollfd item={fd,events,0};
				dynpollfd::add(item);
			}
		}
		
		const size_t size() const {
			return dynpollfd::size();
		}
		
		pollfd *data() {
			return dynpollfd::data();
		}

		const pollfd &at(const size_t pos) const {
			return dynpollfd::at(pos);
		}

		pollfd &at(const size_t pos) {
			return dynpollfd::at(pos);
		}

		size_t addEvents(const int fd,const int events) {
			size_t i=getIndex(fd);
			if (i!=npos) {
				at(i).events|=events;
			} else {
				pollfd item={fd,events,0};
				dynpollfd::add(item);
			}
		}

		size_t getIndex (const int fd) {
			for (int i=0;i<size();i++) {
				if (at(i).fd==fd) return i;
			}
			return npos;
		}

		pollfd find (const int fd) {
			size_t i=getIndex(fd);
			if (i!=npos) return at(i);
			
			pollfd result={0,0,0};
			return result;
		}
	
		bool remove( const int fd ) {
			size_t pos=getIndex(fd);
		
			if (pos!=npos) {
				dynpollfd::erase(pos);
				return true;
			}
		
			return false;
		}

		size_t removeEvents( const int fd, const int events ) {
			size_t pos=getIndex(fd);
			if (pos!=npos) {
				at(pos).events&=events;
			}
			return pos;
		}
		
		operator pollfd*() {
			return data();
		}
		
		operator nfds_t() const {
			return size();
		}
		
		pollfd &operator[] (const size_t pos) {
			return at(pos);
		}

		const pollfd &operator[] (const size_t pos) const {
			return at(pos);
		}
};

int poll (Pollfds &pollfds,int timeout) {
	return poll(pollfds.data(),pollfds.size(),timeout);
}
#endif
