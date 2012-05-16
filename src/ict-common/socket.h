#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <sstream>

#ifndef SO_PROTOCOL
#define SO_PROTOCOL 0x1028 
#endif

#ifndef SO_DOMAIN
#define SO_DOMAIN 0x1029
#endif

using namespace std;

typedef void (*socketEvent)(class socket_t &socket);

class DynAddr {
    private:
	string data_;
	string textual_;
    public:
	void set(const sockaddr *addr, const socklen_t addrlen) {
		data_.assign((char*)addr,addrlen);
	}

	void set(const sockaddr_in *addr) {
		data_.assign((char*)addr,sizeof(*addr));
	}

	void set(const sockaddr_in &addr) {
		data_.assign((char*)&addr,sizeof(addr));
	}

	void set(const sockaddr_un *addr, const socklen_t addrlen) {
		data_.assign((char*)addr,addrlen);
	}
	
	void set_in(const string ip,const int port) {
		struct sockaddr_in serv_addr; bzero((char *)&serv_addr,sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = ip.empty()?INADDR_ANY:inet_addr(ip.c_str());
		serv_addr.sin_port =  htons(port);
		
		set(serv_addr);
	}

	void set_in(const string ip_port) {
	
	}

	void set_un(const string file) {
		int servlen;
		struct sockaddr_un serv_addr; bzero((char *)&serv_addr,sizeof(serv_addr));
		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path, file.c_str());
		servlen = file.size() + sizeof(serv_addr.sun_family);

		set(&serv_addr,servlen);
	}
	
	string &toString() {
		textual_.clear();
		char family=getFamily();
		if (family==AF_INET) {
			sockaddr_in *addr=(sockaddr_in*)data_.data();
			std::stringstream out;
			out << inet_ntoa(addr->sin_addr);
			out << ':';
			out << ntohs(addr->sin_port);
			textual_ = out.str();
		}

		if (family==AF_UNIX) {
			sockaddr_un *addr=(sockaddr_un*)data_.data();
			string result;
			result.assign(addr->sun_path,data_.size()-sizeof(addr->sun_family));
			textual_ = result;
		}
		
		return textual_;
	}

	operator string&() {
		return toString();
	}

	DynAddr &assign(const DynAddr &addr) {
		data_.assign(addr.data_);
		
		return *this;
	}

	DynAddr &operator= (const DynAddr &addr) {
		return assign(addr);
	}
	
	const bool operator== (const DynAddr &addr) const {
		return equal(addr);
	}

	const bool equal(const DynAddr &addr) const {
		return data_.compare(addr.data_)==0;
	}

	const bool equal(const string &addr) const {
		return data_.compare(addr)==0;
	}

	const bool equal(const sockaddr* addr,const socklen_t addrlen) const {
		string addrStr((char*)addr,addrlen);
		return equal(addrStr);
	}

	char getFamily() {
		if (data_.empty()) return 255;
		return toSockAddrP()->sa_family;
	}
	
	const sockaddr *toSockAddrP() const {
		return (sockaddr *)data_.data();
	}

	const int size() const {
		return data_.size();
	}
	
	const bool empty() const {
		return data_.empty();
	}
};

class socket_t {
    private:
	int handle_;

	int domain_;
	int type_;
	int protocol_;
    public:
	DynAddr local;
	DynAddr peer;
	DynAddr lastPeer;

	
	socketEvent onNewPeer;

	const int getHandle() const {
		return handle_;
	}
	
	const bool handles(const int handle) const {
		return handle_==handle;
	}

	socket_t() {
		handle_=-1;
		domain_=type_=protocol_=-1;
	}

	~socket_t() {
	}

	socket_t(const int domain,const int type,const int protocol) {
		domain_=domain;
		type_=type;
		protocol_=protocol;
		init(domain_,type_,protocol_);
	}

	int init(const int domain,const int type,const int protocol) {
		domain_=domain;
		type_=type;
		protocol_=protocol;
		return handle_=socket(domain_,type_,protocol_);
	}
	
	int reinit() {
		close();
		
		return handle_=socket(domain_,type_,protocol_);
	}

	int connect() {
		int result=::connect(handle_,peer.toSockAddrP(),peer.size());
		updateLocalData();
		return result;
	}
	
	bool isConnected() {
		int optval;
		socklen_t optlen = sizeof(optval);
		int res = getsockopt(SOL_SOCKET,SO_ERROR,&optval, &optlen);
		return (optval==0 && res==0);
	}

	const int getType() const {
		int optval;
		socklen_t optlen = sizeof(optval);
		
		int res = getsockopt(SOL_SOCKET,SO_TYPE,&optval, &optlen);
		return res==0?optval:-1;
	}

	const int getDomain() const {
		int optval;
		socklen_t optlen = sizeof(optval);
		
		int res = getsockopt(SOL_SOCKET,SO_DOMAIN,&optval, &optlen);
		return res==0?optval:-1;
	}

	const int getProtocol() const {
		int optval;
		socklen_t optlen = sizeof(optval);
		
		int res = getsockopt(SOL_SOCKET,SO_PROTOCOL,&optval, &optlen);
		return res==0?optval:-1;
	}

	void updateLocalData() {
		struct sockaddr_in sin;
		socklen_t addrlen = sizeof(sin);
		int res;

		if(getsockname(handle_, (struct sockaddr *)&sin, &addrlen) == 0) {
			local.set(sin);
		}
	}

	int getLocalPort(int socket) {
		struct sockaddr_in sin;
		socklen_t addrlen = sizeof(sin);
		
		if(getsockname(handle_, (struct sockaddr *)&sin, &addrlen) == 0 && sin.sin_family == AF_INET && addrlen == sizeof(sin)) {
			return ntohs(sin.sin_port);
		}

		return -1;
	}
	
	const DynAddr &getLastPeer() const {
		return lastPeer;
	}

	const int send(const char *buf, const int addrlen,const int flags) const {
		if (type_==SOCK_DGRAM)
			return sendto(buf,addrlen,0,peer);
		return ::send(handle_,buf,addrlen,flags);
	}
	
	const int send(const string &msg,const int flags=0) const {
		return send(msg.data(),msg.size(),flags);
	}

	const int write(const char *buf, const socklen_t buflen) const {
		return send(buf,buflen,0);
	}

	const int write(const string &msg) const {
		return send(msg.data(),msg.size(),0);
	}

	void recv(string &msg,const socklen_t maxlen,const int flags) {
		char buf[maxlen];
		
		int result=recv(&buf[0],sizeof(buf),flags);
		if (result>=0) msg.assign(&buf[0],result);
	}

	
	int recv(void *buf,const int bufsize,const int flags) {
		struct sockaddr from;
		socklen_t fromlen=sizeof(from);

		int result=::recvfrom(handle_,buf,bufsize,flags,(struct sockaddr*)&from,&fromlen);
		lastPeer.set (&from,fromlen);
		
		if (result>0 && !peer.equal(lastPeer)) {
//			peer.set(&from,fromlen);
			if (onNewPeer) onNewPeer(*this);
		}
		
		return result;
	}

	int recvfrom(void *buf,const int bufsize,const int flags,DynAddr &fromAddr) {
		struct sockaddr from;
		socklen_t fromlen=sizeof(from);

		int result=::recvfrom(handle_,buf,bufsize,0,(struct sockaddr*)&from,&fromlen);
		fromAddr.set (&from,fromlen);
		lastPeer.set (&from,fromlen);
		
		if (result>0 && !peer.equal(lastPeer)) {
			if (onNewPeer) onNewPeer(*this);
		}
		
		return result;
	}

	const int sendto(const char *buf, const socklen_t bufsize,const int flags, const sockaddr *destination, const socklen_t dstsize) const {
		return ::sendto(handle_,buf,bufsize,flags,destination,dstsize);
	}

	const int sendto(const char *buf, const socklen_t bufsize,const int flags, const DynAddr destination) const {
		return ::sendto(handle_,buf,bufsize,flags,destination.toSockAddrP(),destination.size());
	}

	const int sendto(const string &buf, const int flags, const sockaddr *destination, const socklen_t dstsize) const {
		return sendto(buf.data(),buf.size(),flags,destination,dstsize);
	}

	const int sendto(const string &buf, const int flags, const DynAddr destination) const {
		return sendto(buf.data(),buf.size(),flags,destination.toSockAddrP(),destination.size());
	}
	
	int bind () {
		return ::bind(handle_,local.toSockAddrP(),local.size());
	}
	
	const int getsockopt(const int level, const int optname, void *optval, socklen_t *optlen) const {
		return ::getsockopt(handle_,level,optname,optval,optlen);
	}
	
	int setsockopt(const int level, const int optname, const void *optval, const socklen_t optlen) {
		return ::setsockopt(handle_,level,optname,optval,optlen);
	}
	
	void updatefdmax (int *fdmax) const {
		if (handle_>*fdmax) *fdmax=handle_;
	}

	void close() {
		if (handle_>=0) {
			::close(handle_);
			handle_=-1;
		}
	}

	const bool closed() const {
		return handle_<0;
	}
	
	int listen (const int cnt) {
		return ::listen(handle_,cnt);
	}
	
	socket_t accept() const {
//		char family=getDomain();
		socket_t result;
		struct sockaddr cli_addr;
		socklen_t clilen = sizeof(cli_addr);

		int newClient = ::accept(handle_,&cli_addr,&clilen);

		if (newClient>0) {
			result.handle_   = newClient;
			result.peer.set(&cli_addr,clilen);
			result.local     = local;
			result.domain_   = result.getDomain();
			result.type_     = result.getType();
			result.protocol_ = result.getProtocol();
		}

		return result;
	}
	
	int accept (sockaddr *cli_addr, socklen_t *cli_len) {
		return ::accept(handle_,cli_addr,cli_len);
	}
	
	const bool inited () const {
		return handle_!=-1;
	}
};
#endif
