#ifndef ICT_DATAGRAM_H
#define ICT_DATAGRAM_H
class ictDatagram_t {
	public:
		char buf[256*256];
		uint16_t length;
		const uint16_t getDataSize() const {
			return length-6;
		}

		const uint16_t getDataLength() const {
			return getDataSize();
		}
	
		const uint16_t getNumber() const {
			return *((uint16_t*)buf);
		}
		const uint32_t getTime() const {
			return *((uint32_t*)&buf[2]);
		}
		const char *getData() const {
			return &buf[6];
		}

		void setNumber(uint16_t nr) {
			memcpy(buf,&nr,sizeof(nr));
		}
		void setTime(uint32_t time) const {
			memcpy((char*)&buf[2],&time,sizeof(time));
		}
		void setData(const char *data,const int cnt) {
			memcpy((char*)&buf[6],data,cnt);
			length=cnt+6;
		}
		void setDataLength(uint16_t dataLen) {
			length=dataLen+6;
		}
		
		char *data() {
			return &buf[6];
		}
		
		const int size() const {
			return length;
		}

		ictDatagram_t() {
			length=0;
		}
		
		operator char*() {
			return (char*)&buf;
		}
};

#endif
