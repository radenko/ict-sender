#define CONTROL_THREAD

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
#include <deque>
#include <cstdio>
#include <sys/time.h>
#include <limits>
#include <sys/un.h>
#include <cerrno>

#include <iostream>
#include <fstream>

#include "ict-common/socket.h"
#include "ict-common/ict-datagram.h"
#include "ict-common/reconnector.h"
#include "ict-common/pthreaddeque.h"

using namespace std;
using namespace libconfig;

struct Datagram{
	uint32_t time;
	uint16_t number;
	string data;
};
    
typedef vector<string> Strings;
typedef Strings::iterator StringsIterator;

typedef vector<string> CStrings;
typedef CStrings::const_iterator CStringsIterator;

typedef set<int> CSocketSet;
typedef CSocketSet::const_iterator CSocketSetIterator;

struct ListeningArgs {
	int port;
	string proto;
	string host;
	uint16_t mtu;
	
	int copyPort;
	string copyHost;
};

struct StreamingArgs {
	int port;
	string host;
	string proto;
	uint64_t bitrate;
	bool autoSuspend;
	bool suspendOnStart;
};

struct ControlArgs {
	int port;
	string proto;
	string host;
};
    
struct CleaningArgs {
	int bufferSize; //sec
};

ListeningArgs listeningArgs;
StreamingArgs streamingArgs;
CleaningArgs cleaningArgs;
ControlArgs controlArgs;

bool forceLogToFile;
string logFile;
string pidFile;
std::string configFile="sender.cfg";

bool fflag=false, vflag=false;
bool sendingSuspended=false;

uint64_t keptLastTime=0;
string controlStr;
bool sendingInitialized=false;

pthread_deque<uint16_t> outQueue;
//pthread_mutex_t outQueueMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t	cleaningThread;
pthread_t	streamingThread;
pthread_t	keepingThread;
#ifdef CONTROL_THREAD
pthread_t	controlThread;
#endif

class ictStream {

};

int abs(int a) {
	return a<0?-a:a;
}

void *streaming (void *);
void *cleaning (void *);
void *controlThreadFcn (void *);

bool sendControl(const char *data, const int datalen);
bool sendControl(const string data);
string processControl (const string Msg,const socket_t &sock); //(const string Msg,const int &sock,const struct sockaddr_in *from=NULL,const socklen_t *fromlen=NULL);

Datagram datagramBuffer[65536];
pthread_mutex_t datagramBufferLock;

bool reinit=true;

int fdmax=0;

int yes = 1;
/* master file descriptor list */
fd_set master,controlSocks;

socket_t control;
socket_t listener;
socket_t copier;
socket_t sender;

uint16_t streamingNumber;

pthread_mutex_t streaming_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  streaming_cond  = PTHREAD_COND_INITIALIZER;

const uint32_t MCAST_ADDR=inet_addr("224.0.0.0");
const uint32_t MCAST_MASK=inet_addr("240.0.0.0");

void *cleaningFcn (void *argPtr);
void *keepingFcn (void *argPtr);
void *streamingFcn (void *argPtr);

int mypipe[2];

void waitFor(pthread_cond_t *var,pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
    pthread_cond_wait(var,mutex);
    pthread_mutex_unlock(mutex);
}

void releaseWaitFor(pthread_cond_t *var,pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
    pthread_cond_signal(var);
    pthread_mutex_unlock(mutex);
}

void stringprintf(string &dst, const char *fmt, ...) {
	char *buffer;
	va_list ap;
	va_start(ap, fmt);

	vasprintf(&buffer,fmt, ap);
	dst=buffer;
	free(buffer);

	va_end(ap);
}

string stringprintf(const char *fmt, ...) {
	char *buffer;
	string result;
	va_list ap;
	va_start(ap, fmt);

	vasprintf(&buffer,fmt, ap);
	result=buffer;
	free(buffer);

	va_end(ap);
	
	return result;
}

CStrings split(const string Splitter,const string Str,const int maxCount=std::numeric_limits<int>::max(),int *rest=NULL) {
	CStrings Result;
	size_t oldpos,pos=-Splitter.size();
	oldpos=pos;
	if (rest!=NULL) *rest=0;

	while( (pos=Str.find(Splitter,pos+Splitter.size())) != string::npos ) {
		if (Result.size() >= maxCount) {
			if (rest!=NULL) *rest=2;
			break;
		} else { 
			if (pos-oldpos-Splitter.size()) {
				Result.push_back(Str.substr(oldpos+Splitter.size(),pos-oldpos-Splitter.size()));
			}
			oldpos=pos;
		}
	}

	if (oldpos+Splitter.size()<Str.length()) {
		Result.push_back(Str.substr(oldpos+Splitter.size(),string::npos));
		if (rest!=NULL && *rest==0) *rest=1;
	}

	return Result;
}

bool socketIsConnected(int socket) {
	int optval;
	socklen_t optlen = sizeof(optval);
	int res = getsockopt(socket,SOL_SOCKET,SO_ERROR,&optval, &optlen);
	return (optval==0 && res==0);
}

int getSocketType(int socket) {
	int optval;
	socklen_t optlen = sizeof(optval);
	int res = getsockopt(socket,SOL_SOCKET,SO_TYPE,&optval, &optlen);
	return res==0?optval:-1;
}

int getSocketLocalPort(int socket) {
	struct sockaddr_in sin;
	socklen_t addrlen = sizeof(sin);
	
	if(getsockname(socket, (struct sockaddr *)&sin, &addrlen) == 0 && sin.sin_family == AF_INET && addrlen == sizeof(sin)) {
		return ntohs(sin.sin_port);
	}
	
	return -1;
}

const char *mons[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","sep","Oct","Nov","Dec"};
void makeLog(int priority,const char *fmt,...) {
	if (priority==LOG_DEBUG && !vflag) return;

	char *buffer;
	va_list ap;
	va_start(ap, fmt);

	if (fflag) {
		if (priority==LOG_ERR) {
			vfprintf(stderr,fmt,ap);
			fprintf(stderr,"\n");
		}
		else {
		    vprintf(fmt,ap);
		    printf("\n");
		}
	}
	else {
		if (logFile.empty() || logFile.compare("--")==0) {
			vasprintf(&buffer,fmt, ap);
			syslog(priority, buffer);
			free(buffer);
		} else {
			time_t seconds=time (NULL);
			struct tm * timeinfo = localtime ( &seconds );

			FILE *fHandle;
			fHandle=fopen(logFile.c_str(),"a");
			
			if (fHandle!=NULL) {
				fprintf(fHandle,"%s %02u %02u:%02u:%02u ",mons[timeinfo->tm_mon],timeinfo->tm_mday,timeinfo->tm_hour,timeinfo->tm_min,timeinfo->tm_sec);
				vfprintf(fHandle,fmt,ap);
				fputc('\n',fHandle);
				fclose(fHandle);
			}
		}
	}
	va_end(ap);
}

void readConfig(Config &config) {
	listeningArgs.mtu=1500;
	streamingArgs.suspendOnStart=false;
	streamingArgs.bitrate=0;

	config.readFile(configFile.c_str());
	config.lookupValue ("logFile", logFile);
	config.lookupValue ("pidFile", pidFile);
	config.lookupValue ("forceLogToFile", forceLogToFile);
		
	config.lookupValue ("listen.port", listeningArgs.port);
	config.lookupValue ("listen.host", listeningArgs.host);
	config.lookupValue ("listen.proto", listeningArgs.proto);
	int mtu;
	config.lookupValue ("listen.mtu", mtu);mtu=int(mtu/188)*188;listeningArgs.mtu=mtu;

	config.lookupValue ("streaming.host",streamingArgs.host);
	config.lookupValue ("streaming.port",streamingArgs.port);
	config.lookupValue ("streaming.autoSuspend",streamingArgs.autoSuspend);
	config.lookupValue ("streaming.suspendOnStart",streamingArgs.suspendOnStart);
	config.lookupValue ("streaming.bitrate",streamingArgs.bitrate);

	config.lookupValue ("copy.host",listeningArgs.copyHost);
	config.lookupValue ("copy.port",listeningArgs.copyPort);

	config.lookupValue ("control.port", controlArgs.port);
	config.lookupValue ("control.host", controlArgs.host);

	streamingArgs.bitrate*=1000;
	
//	printf("Bitrate: %lu\n",streamingArgs.bitrate);
	if (streamingArgs.port<=0) {
		makeLog(LOG_WARNING,"streaming port is bad: %d, check configuration file!!!",streamingArgs.port);
	}
}

void terminateFcn (int param)
{
	makeLog (LOG_INFO,"Terminating program...");
	if (!pidFile.empty()) {
		remove (pidFile.c_str());
	}
	
	for(int j = 0; j <= fdmax; j++)
		if(FD_ISSET(j, &master)) {
			close(j);
			FD_CLR(j, &master);
		}
		
	pthread_mutex_destroy(&datagramBufferLock);
	
	pthread_kill(cleaningThread,SIGTERM);
	pthread_kill(streamingThread,SIGTERM);
	pthread_kill(keepingThread,SIGTERM);
	pthread_kill(controlThread,SIGTERM);
	
	exit(1);
}

void terminateFcn() {
    terminateFcn(0);
}

void pushDatagram (const ictDatagram_t &datagram) {
	uint16_t number=datagram.getNumber();
	const uint16_t dataLength=datagram.getDataLength();
	pthread_mutex_lock(&datagramBufferLock);
	datagramBuffer[number].number=number;
	datagramBuffer[number].time=datagram.getTime();
	datagramBuffer[number].data.assign(datagram.getData(),dataLength);
	pthread_mutex_unlock(&datagramBufferLock);

	releaseWaitFor(&streaming_cond,&streaming_mutex);

//	makeLog(LOG_DEBUG,"Pushing nr=%u,len=%d",number,datagramBuffer[number].data.length());
	//if ($$streaming_sema<1) { $streaming_sema->up(); }
}

void getDatagram (ictDatagram_t &datagram,const uint16_t number) {
	pthread_mutex_lock(&datagramBufferLock);
	
	datagram.setNumber(datagramBuffer[number].number);
	datagram.setTime(datagramBuffer[number].time);
	datagram.setData(datagramBuffer[number].data.data(),datagramBuffer[number].data.size());
	
	pthread_mutex_unlock(&datagramBufferLock);
}

int getDatagramLength (const uint16_t number) {
	pthread_mutex_lock(&datagramBufferLock);
	int result=datagramBuffer[number].data.length();
	pthread_mutex_unlock(&datagramBufferLock);
	
	return result;
}

void removeDatagram(uint32_t number) {
	pthread_mutex_lock(&datagramBufferLock);
	datagramBuffer[number].number=0;
	datagramBuffer[number].time=0;
	datagramBuffer[number].data.clear();
	pthread_mutex_unlock(&datagramBufferLock);
}

void clearBuffer () {
	pthread_mutex_lock(&datagramBufferLock);
	for (int i=0;i<65536;i++) {
		datagramBuffer[i].number=0;
		datagramBuffer[i].time=0;
		datagramBuffer[i].data.clear();
	}
	pthread_mutex_unlock(&datagramBufferLock);
}

bool sendControl(const string data) {
	makeLog(LOG_DEBUG,"Control: Sending {%s}",data.c_str());

	if (control.peer.empty()) {
	    makeLog(LOG_ERR,"Control: Peer party is not connected!");
	    return false;
	}

	string buf(data);
	buf.append("\n");
	int sentLength=control.write(buf);

	if (sentLength <= 0) {
		return false;
	} else if (buf.size() != sentLength) {
		makeLog(LOG_ERR,"Control: Unable to send complete data");
	}

	return true;

}

bool sendControl(const char *data,const int datalen) {
	string buf(data,datalen);
	sendControl(buf);
}

uint64_t getMNow() {
	struct timeval nowStruct;
	gettimeofday(&nowStruct, NULL);
		
	return uint64_t(uint64_t(nowStruct.tv_sec)*1000+uint64_t(nowStruct.tv_usec)/1000l+0.5);
}

uint64_t getUNow() {
	struct timeval nowStruct;
	gettimeofday(&nowStruct, NULL);
		
	long now=uint64_t(nowStruct.tv_sec)*1000*1000+nowStruct.tv_usec;
	
	return now;
}

/*TODO
void setControlPeer (const struct sockaddr_in from,const socklen_t *fromlen) {
	if (control.peer.sin_addr.s_addr!=from.sin_addr.s_addr || control.peer.sin_port!=from.sin_port) {
		control.peer=from;
		makeLog(LOG_INFO,"Control from other party connected: %s:%d",inet_ntoa(control.peer.sin_addr),ntohs(control.peer.sin_port));
		sendControl(stringprintf("GETBUF"));
	}

	control.peer=from;
}
*/

void processControls (string &msgsStr,const socket_t &sock) {
	int msgsStrEnd;
	const CStrings msgs=split("\n",msgsStr,std::numeric_limits<int>::max(),&msgsStrEnd);

	for (CStringsIterator msgsItem=msgs.begin();msgsItem!=msgs.end();msgsItem++) {
	    if (!msgsItem->empty()) {
		string resp=processControl(*msgsItem,sock);
		if (!resp.empty()) {
//			makeLog(LOG_DEBUG,"Sending control {%s}",resp.c_str());
			sendControl(resp);//socket,resp.c_str(),resp.length(),0,(const sockaddr*)&control.peer,sizeof(control.peer));
		}
	    }
	}
	
	
	msgsStr=msgsStrEnd?msgs.back():"";
}

string processControl (const string Msg,const socket_t &sock) {
	const CStrings data=split(" ",Msg);
	const CStringsIterator name=data.begin();
	string result="";
	makeLog(LOG_DEBUG,"Control: Processing {%s}",Msg.c_str());

	if (name->compare("SEND")==0) {
		uint16_t resendNumber = (data.size()>1)?atoi(data[1].c_str()):0;
		uint16_t cnt = (data.size()>2)?atoi(data[2].c_str()):0;
		if (cnt<1) cnt=1;
		
//		makeLog(LOG_DEBUG,"RESEND request: %d:%d",resendNumber,cnt);

		bool wasMissing=false;
		for (int i=0;i<cnt;i++) {
			int missing = getDatagramLength(resendNumber);
			
			if (missing<=0) {
				wasMissing=true;
				makeLog(LOG_INFO,"Streamer: Datagram lost: %d",resendNumber);
				sendControl(stringprintf("SEND_OOB %d",resendNumber));
			} else {
				ictDatagram_t ictDatagram;
				getDatagram(ictDatagram,resendNumber);
				
				outQueue.push_front	( resendNumber );
				
//				sendto(sender.socket,ictDatagram,ictDatagram.size(),0,(const sockaddr*)&sender.peer,sizeof(sender.peer));
			}
			
			resendNumber++;
		}

		if (wasMissing) {
			makeLog(LOG_WARNING,"Streamer: Packet(s) is out of buffer");
		}
	}
	else if (name->compare("STARTED")==0) {
		makeLog (LOG_INFO,"Control: method STARTED");
	}
	else if (name->compare("KEPT")==0) {
		keptLastTime=getMNow();
	}
	else if (name->compare("SETBUF")==0) {
		//makelog ("Control message: {$msg1}");
		int newBufferTime=(data.size()>1)?atoi(data[1].c_str()):-1;

		if (newBufferTime!=-1 && cleaningArgs.bufferSize!=newBufferTime) {
			cleaningArgs.bufferSize=newBufferTime;
			makeLog (LOG_INFO,"Buffer is set to new value: %d",cleaningArgs.bufferSize);
		}
		return stringprintf("BUFSET %d",cleaningArgs.bufferSize);
	}
	else if (name->compare("RESETSTREAM")==0) {
		FD_CLR(sender.getHandle(),&master);
		sender.reinit();
		FD_SET(sender.getHandle(),&master);
		
		makeLog (LOG_INFO,"Streamer: Restart on receiver requested");
		result+=stringprintf("STREAMRESET %d",0);
	}
	else if (name->compare("SUSPEND")==0) {
		if (!sendingSuspended) {
			sendingSuspended=true;
			makeLog(LOG_INFO,"Streamer: Supended on receiver request");
		}
		
		return stringprintf("SUSPENDED %d",0); //TODO O by malo byt cislo noveho portu na vysielacej strane
	}
	else if (name->compare("RESUME")==0) {
		sendingSuspended=false;
	
		return stringprintf("RESUMED %d",0); //TODO O by malo byt cislo noveho portu na vysielacej strane
	}
	else if (name->compare("CINIT")==0) {
		int newBufferTime=data.size()>1?atoi(data[1].c_str()):-1;
		sendingSuspended=false;
		makeLog(LOG_INFO,"Clinet initialization request from receiver");

		if (newBufferTime!=-1 && cleaningArgs.bufferSize!=newBufferTime) {
			cleaningArgs.bufferSize=newBufferTime;
			makeLog (LOG_INFO,"Buffer is set to new value: %d",cleaningArgs.bufferSize);
			result+=stringprintf("BUFSET %d",cleaningArgs.bufferSize);
		}

		result+=stringprintf("CINITED %d",0);//getSocketLocalPort(sender.socket)); //TODO O by malo byt cislo noveho portu na vysielacej strane
	}

	else if (name->compare("CUNINIT")==0) {
		clearBuffer();
		sendingInitialized=false;
		result+=stringprintf("CUNINITED %d",0); //TODO O by malo byt cislo noveho portu na vysielacej strane
	}
	else if (name->compare("CONNECTCONTROL")==0) {
		return "CONTROLCONNECT\n";
	} else {
		makeLog(LOG_ERR,"Control: unknown command: %s",name->c_str());
	}

	return result;
}


void reconnectListener(socket_t *sock, void *arg) {
	FD_SET(sock->getHandle(),&master);
	makeLog(LOG_INFO,"Listener: Connected");
}

int main(int argc, char *argv[]) {
	//clear the master and temp sets
	FD_ZERO(&master);
	FD_ZERO(&controlSocks);
	
	char c;
	int hSocket;
	uint16_t number=0;
	uint64_t lastAnnounceTime=0;
	Reconnector reconnector;

	set_terminate(terminateFcn);

	void (*prev_fn)(int);
	prev_fn = signal (SIGINT,terminateFcn);
	if (prev_fn==SIG_ERR)
		makeLog(LOG_ERR,"Main: Unable to set termination funtion"); 

	while ((c = getopt (argc, argv, "fvc:")) != -1)
	switch (c) {
	    case 'f':
		fflag = true;
	    break;
	    case 'v':
		vflag = true;
	    break;
	    case 'c':
		configFile = optarg;
	    break;
	    case '?':
		if (optopt == 'c')
		    fprintf (stderr, "Main: Option -%c requires an argument.\n", optopt);
		else if (isprint (optopt))
		    fprintf (stderr, "Main: Unknown option `-%c'.\n", optopt);
		else
		    fprintf (stderr, "Main: Unknown option character `\\x%x'.\n", optopt);
		return 1;
	    default:
		abort ();
	}

	Config config;
	readConfig(config);
	makeLog(LOG_DEBUG,"Read configuration");
	
	if (!pidFile.empty()) {
		makeLog(LOG_DEBUG,"Main: Checking pid file: %s",pidFile.c_str());
		ifstream pidfile(pidFile.c_str());
		if (pidfile.is_open()) {
			char line[20];
			int otherpid;
			pidfile.getline(line,sizeof(line));
			sscanf(line,"%u",&otherpid);
			
			if (kill(otherpid,0)==0) {
				makeLog(LOG_ERR,"Main: Other instance is running: %d",otherpid);
				pidFile="";
				terminate();
			}
			pidfile.close();
		}
	}
	
//	makeLog(LOG_DEBUG,"PID check finished");
	
	if (!fflag) daemon (0,1);
	
	if (!pidFile.empty()) {
		ofstream pidfile;

		pidfile.open(pidFile.c_str());
		pidfile << getpid();
		pidfile << "\n";
		pidfile.close();
	}

	if (listeningArgs.proto.compare("tcp")==0) {
		//get the listener
		if(listener.init(AF_INET, SOCK_STREAM, 0) == -1) {
			makeLog(LOG_ERR,"Listener: Unable to create socket");
			exit(1);
		}
		
		listener.peer.set_in(listeningArgs.host,listeningArgs.port);

		if (listener.connect() < 0) {
			makeLog(LOG_ERR,"Listener: Cannot connect: %s;",strerror(errno));
			reconnector.add(&listener,reconnectListener,NULL);
		} else {
			// add the listener to the master set
			listener.updatefdmax(&fdmax);
			makeLog(LOG_INFO,"Listener: Connected to %s:%d;",listeningArgs.host.c_str(),listeningArgs.port);
		}

		FD_SET(listener.getHandle(), &master);
	} else {
		const bool mcast=((inet_addr(listeningArgs.host.c_str()) & MCAST_MASK) == MCAST_ADDR);

		//get the listener
		if(listener.init(AF_INET, SOCK_DGRAM, 0) == -1) {
			makeLog(LOG_ERR,"Listener: Server-socket() error lol!");
			terminate();
		}

		makeLog(LOG_INFO,"Listener: Server-socket() is OK...");

		//"address already in use" error message
		if(listener.setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			makeLog(LOG_ERR,"Listener: Setsockopt error: %s",strerror(errno));
			terminate();
		}
		makeLog(LOG_INFO,"Listener: Setsockopt() is OK...");
		
		// bind
		listener.local.set_in(listeningArgs.host,listeningArgs.port);

		if(listener.bind() == -1)
		{
			makeLog(LOG_ERR,"Listener: Unable to bind: %s",strerror(errno));
			terminate();
		}
		
		if (mcast) {
			ip_mreq imreq;
			imreq.imr_multiaddr.s_addr = inet_addr(listeningArgs.host.c_str());
			imreq.imr_interface.s_addr = INADDR_ANY; // use DEFAULT interface   // JOIN multicast group on default interface
			int status = listener.setsockopt(IPPROTO_IP, IP_ADD_MEMBERSHIP, (const void *)&imreq, sizeof(struct ip_mreq));
			makeLog(LOG_INFO,"Listener: Joining multicast %s",listeningArgs.host.c_str());
		}

		makeLog(LOG_INFO,"Listener: ready udp://%s:%d",listeningArgs.host.c_str(),listeningArgs.port);

		// add the listener to the master set
		FD_SET(listener.getHandle(), &master);
		listener.updatefdmax(&fdmax);
	}

	if(control.init(AF_INET, SOCK_DGRAM, 0) == -1) {
		makeLog(LOG_ERR,"Control: Unable to create socket: %s",strerror(errno));
		terminate();
	}

	if (controlArgs.host.empty()) {
		control.local.set_in("",controlArgs.port);

		if (control.bind()>=0) 
			makeLog(LOG_INFO,"Control: Listening on *:%d",controlArgs.port);
		else {
			makeLog(LOG_INFO,"Control: Unable to bind socket for udp port %d: %s",controlArgs.port,strerror(errno));
			exit(0);
		}	
	} else {
		makeLog(LOG_INFO,"Control: NAT mode");
		if (controlArgs.proto.compare("tcp")==0) {
		
		} else {
			control.peer.set_in(controlArgs.host,controlArgs.port);
		}
	}

	#ifdef CONTROL_THREAD
	FD_SET(control.getHandle(), &controlSocks);
	control.updatefdmax(&fdmax);
	#else
	FD_SET(control.getHandle(), &master);
	control.updatefdmax(&fdmax);
	#endif

	if(sender.init(AF_INET, SOCK_DGRAM, 0) == -1) {
		makeLog(LOG_ERR,"Streamer: Unable to create socket: %s",strerror(errno));
		terminate();
	}

	if (streamingArgs.proto.compare("tcp")==0) {
		
	} else {
		sender.peer.set_in(streamingArgs.host,streamingArgs.port);
		makeLog(LOG_INFO,"Streamer: Peer port was set up %s:%d",streamingArgs.host.c_str(),streamingArgs.port);
	}

	#ifdef CONTROL_THREAD
	FD_SET(sender .getHandle(), &controlSocks);
	sender.updatefdmax(&fdmax);
	#else
	FD_SET(sender.getHandle(), &master);
	sender.updatefdmax(&fdmax);
	#endif

	if (!listeningArgs.copyHost.empty()) {
		if(copier.init(AF_INET, SOCK_DGRAM, 0) == -1) {
			makeLog(LOG_ERR,"Copier: Unable to create socket: %s",strerror(errno));
		}

		copier.peer.set_in(listeningArgs.copyHost,listeningArgs.copyPort);
		makeLog(LOG_INFO,"Copier: Resend data to: %s",copier.peer.toString().c_str());

		FD_SET(copier.getHandle(), &master);
		copier.updatefdmax(&fdmax);
	}

	openlog("ict-receiver", LOG_PID|LOG_CONS, LOG_USER);

	sendingSuspended=streamingArgs.suspendOnStart;

        if (int errcode=pthread_create(&cleaningThread,NULL,&cleaningFcn,&cleaningArgs)) {
            makeLog(LOG_ERR,"Cleaner: Unable to create thread: %s",strerror(errno));
        }

	if (streamingArgs.bitrate) {
		if(pipe(mypipe) < 0) {
			makeLog(LOG_ERR,"Streamer: Unable to create pipe %s",strerror(errno));
		}
	
		if (int errcode=pthread_create(&streamingThread,NULL,&streamingFcn,&streamingArgs)) {
			makeLog(LOG_ERR,"Streamer: Unable to create thread: %s",strerror(errno));
		}
        }

        if (int errcode=pthread_create(&keepingThread,NULL,&keepingFcn,NULL)) {
            makeLog(LOG_ERR,"Keeper: Unable to create thread: %s",strerror(errno));
        }

	#ifdef CONTROL_THREAD
	if (int errcode=pthread_create(&controlThread,NULL,&controlThreadFcn,&controlArgs)) {
		makeLog(LOG_ERR,"Control: Unable to create thread: %s",strerror(errno));
	}
	#endif

	sendControl("START");

	timeval timeout={10,0}; //sec,usec

	// loop
	for(;;) {
		// copy it
		fd_set read_fds = master;
		int readCnt=select(fdmax+1, &read_fds, NULL, NULL, &timeout);

		if(  readCnt == -1  ) {
			makeLog(LOG_ERR,"Main: select error: %s",strerror(errno));
			exit(1);
		}
		
		reconnector.tryReconnect(10);

		if (  readCnt > 0  ) {
			//run through the existing connections looking for data to be read
			for(hSocket = 0; hSocket <= fdmax; hSocket++) {
				if(FD_ISSET(hSocket, &read_fds))
				{ // we got one...
					if(listener.handles(hSocket)) {
						// buffer for client data
						ictDatagram_t ictDatagram;
					
						int recvlen = listener.recv(ictDatagram.data(), listeningArgs.mtu, 0);

						if (recvlen>0) {
							if (copier.inited()) {
								copier.write(ictDatagram.data(),recvlen);
							}
					
							ictDatagram.setDataLength(recvlen);
							if (!sendingSuspended) {
								ictDatagram.setTime(getMNow());
								ictDatagram.setNumber(number);

								pushDatagram (ictDatagram);
								if (streamingArgs.bitrate) {
//									makeLog(LOG_DEBUG,"Writenumber: %u",number);

									write(mypipe[1],&number,sizeof(number));
//									outQueue.push_back	( number );
								} else {
									int sentcount=sender.send(ictDatagram,ictDatagram.size(),0);
								}

								number++;
							} else {//if suspended
								uint64_t now=getMNow();
								if (now-lastAnnounceTime>30*1000) {
									sender.send("UU",2,0);
									lastAnnounceTime=now;
								}
							} //if suspended
						} //if recvLen>0
						else if (recvlen==0){//Client must be reconnected!!!!
							if (listeningArgs.proto.compare("tcp")==0) {
									FD_CLR(listener.getHandle(),&master);
									if (listener.isConnected()) {
										listener.reinit();
									}
/*							
									if (listener.connect()==0) {
										makeLog(LOG_INFO,"Listener: Connected");
									} else {
										makeLog(LOG_INFO,"Listener: Reconnect failed");
										sleep(10);
									}
*/
									reconnector.add(&listener,reconnectListener,NULL);
									makeLog(LOG_INFO,"Listener: Disconnected");
							}
						} //if recvlen==0
					} //if sock is listener
					#ifndef CONTROL_THREAD
					if (control.handles(hSocket)) {
						char cmsg[1500];
						int recvlen = control.recv((void*)cmsg,sizeof(cmsg),0);

						if (recvlen>0) {
							makeLog(LOG_DEBUG,"Processing control from main");
							controlStr.append(cmsg,recvlen);
							processControls(controlStr,control);
						}
					}

					if (sender.handles(hSocket)) {
						char cmsg[1500];
						int recvlen = sender.recv(cmsg,sizeof(cmsg),0);

						if (recvlen>0) {
							makeLog(LOG_DEBUG,"Processing control from main");
							controlStr.append(cmsg,recvlen);
							processControls(controlStr,sender);
						}
					}
					#endif
				}
			}//for
		}//if readCnt>0
	}

	closelog();
	return 0;
}

void *streamingFcn (void *argP) {
	StreamingArgs *args=(StreamingArgs*)argP;
	ictDatagram_t datagram;
	uint16_t number;
	
	makeLog(LOG_DEBUG,"Streamer: Thread started");

	for (;;) {
		read(mypipe[0],&number,sizeof(number));
	
//		makeLog(LOG_DEBUG,"Readnumber: %u",number);
//		bool empty=outQueue.empty();
//		if (!empty) {
//			number=outQueue.front();
//			outQueue.pop_front();
//		}

//		if (!empty) {
			getDatagram (datagram,number);

			uint64_t took=getUNow();
			int sentcount=sender.send(datagram,datagram.size(),0);//,(const sockaddr*)&sender.peer,sizeof(sender.peer));
			if (sentcount<0) {
				makeLog(LOG_WARNING,"Sending status error: %d",sentcount);
			} else if (sentcount>0) {
				took=getUNow()-took;
				//1000*1000/bitrate -time for sending 1bit/milisec
				//sentcount*1000*1000/bitrate - time for sending sentcount bits/milisec
				//sentcount*8000*1000/bitrate - time for sending sentcount bytes/milisec
				int64_t wait=long(sentcount)*8000*1000/args->bitrate;
				wait-=took;
//				makeLog(LOG_DEBUG,"Should wait: %ld,%ld,%lu",wait,took,args->bitrate);
				if (wait>0) usleep(wait);
			}
//		} else {
//			outQueue.wait();
//			pthread_mutex_lock	( &outQueueMutex );
//			pthread_cond_wait	( &streamingCnd, &outQueueMutex );
//			pthread_mutex_unlock	( &outQueueMutex );
//		}
	}
}

void *keepingFcn (void *argPtr) {
	makeLog(LOG_DEBUG,"Keeper have started");

	for (;;) {
		uint64_t now=getMNow();
		sendControl(stringprintf("KEEP %ld",now));

		sleep(10);
		
		if (streamingArgs.autoSuspend) {
		    if (now-keptLastTime>30000) {
			if (sendingSuspended!=1) {
				sendingSuspended=1;
				makeLog(LOG_WARNING,"Sending is supended, control is not kept");
			}
		    }
		}
		
//		reconnector.tryReconnect();
	}
}

void *cleaningFcn (void *argPtr) {
	makeLog(LOG_DEBUG,"Cleaner have started");
	CleaningArgs *args=(CleaningArgs *)argPtr;
	bool emptyCycle=false;

	for (uint16_t i=0;;i++) {//infinite loop
		ictDatagram_t datagram;
		getDatagram (datagram,i);
		
		if (datagram.getDataSize()) {
			emptyCycle=false;
			uint32_t now=getMNow();
			uint32_t time=datagram.getTime();
			int64_t wait=time+args->bufferSize*1000-now;
		
			if (wait>cleaningArgs.bufferSize*1000) {
				wait-=0xFFFFFFFF;
			}
		
			if (wait>0) {
				usleep(wait*1000);
			}
		
//			makeLog(LOG_DEBUG,"Removing datagram %d now=%d,time=%ld,buffer=%d",i,getMNow(),time,args->bufferSize);
			removeDatagram(i);
		}
		
		if (i==0) {
			if (emptyCycle) {
			    sleep(2);
			}
			
			emptyCycle=true;
			sleep(1);
		}
	}//infinite loop
}

#ifdef CONTROL_THREAD
void *controlThreadFcn (void *argPtr) {
	makeLog(LOG_DEBUG,"Control: Thread started");
	ControlArgs *args=(ControlArgs *)argPtr;

	int fdmax=-1;
	fd_set read_fds;
	int hSocket;
	timeval timeout={10,0};//sec,usec

	FD_SET(control.getHandle(), &controlSocks);
	control.updatefdmax(&fdmax);
	FD_SET(sender .getHandle(), &controlSocks);
	sender.updatefdmax(&fdmax);

	for (;;) {
		read_fds=controlSocks;
		int selectCnt=select(fdmax+1, &read_fds, NULL, NULL, &timeout);

		if(selectCnt == -1) {
			makeLog(LOG_ERR,"Control: select error: %s",strerror(errno));
			terminate();
		}
		else if(selectCnt > 0) {
			for(hSocket = 0; hSocket <= fdmax; hSocket++) {
				if(FD_ISSET(hSocket, &read_fds))
				{ // we got one...
					makeLog(LOG_DEBUG,"Processing control from thread");
					if(control.handles(hSocket)) {
						char cmsg[1500];
						int recvlen = control.recv((void*)cmsg,sizeof(cmsg),0);

						if (recvlen>0) {
							controlStr.append(cmsg,recvlen);
							processControls(controlStr,control);
						} else if (recvlen==0) {
							FD_CLR( hSocket, &controlSocks );
						}
					}

					if (sender.handles(hSocket)) {
						char cmsg[1500];
						int recvlen = sender.recv(cmsg,sizeof(cmsg),0);

						if (recvlen>0) {
							controlStr.append(cmsg,recvlen);
							processControls(controlStr,sender);
						} else if (recvlen==0) {
							FD_CLR( hSocket, &controlSocks );
						}
					}
				}//if FD_ISSET
			} //for loop through sockets
		} //selectCnt > 0
	}//infinite loop
	
	FD_CLR( control.getHandle(), &controlSocks );
	FD_CLR( sender .getHandle(), &controlSocks );
}
#endif
