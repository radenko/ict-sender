#ifndef ICT_MISC_H
#define ICT_MISC_H

#define STATUS_DEAD 0
#define STATUS_UNSTABLE 1
#define STATUS_LIVE 2
/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>
#include <libconfig.h++>
#include <vector>
#include <string>
#include <list>
#include <map>
#include <set>
#include <cstdio>
#include <limits>
#include <sys/un.h>
#include <cerrno>
#include "ict-common/socket.h"
*/
#include <sys/time.h>

long getMNow() {
	struct timeval nowStruct;
	gettimeofday(&nowStruct, NULL);
		
	long now=nowStruct.tv_sec*1000+nowStruct.tv_usec/1000+0.5;
	
	return now;
}

long getUNow() {
	struct timeval nowStruct;
	gettimeofday(&nowStruct, NULL);
		
	long now=nowStruct.tv_sec*1000*1000+nowStruct.tv_usec/1000;
	
	return now;
}

long getSNow() {
	struct timeval nowStruct;
	gettimeofday(&nowStruct, NULL);
		
	long now=nowStruct.tv_sec;
	
	return now;
}

typedef vector<string> Strings;
typedef Strings::iterator StringsIterator;
typedef Strings::const_iterator StringsConstIterator;  

Strings split(const string Splitter,const string Str,const int maxCount=std::numeric_limits<int>::max()) {
	Strings Result;
	size_t oldpos,pos=-Splitter.size();
	oldpos=pos;
	while( Result.size() < maxCount  &&  (pos=Str.find(Splitter,pos+Splitter.size())) != string::npos ) {
		if (pos-oldpos-Splitter.size()) {
			Result.push_back(Str.substr(oldpos+Splitter.size(),pos-oldpos-Splitter.size()));
		}
		oldpos=pos;
	}

	Result.push_back(Str.substr(oldpos+Splitter.size(),string::npos));
	return Result;
}

#endif