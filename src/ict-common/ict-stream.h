#ifndef ICT_STREAM_H
#define ICT_STREAM_H

struct ictStreamItemHeader_t;

class ictStreamItem_t {
	private:
		char buf[256*256];
//		uint16_t length;
	public:
		static const int headerSize=8;
//		static const int maxDataSize=sizeof(buf)-headerSize;
		const uint16_t getDataSize() const {
			uint16_t dataLen;
			memcpy(&dataLen,(char*)&buf[6],sizeof(dataLen));
			return dataLen;
		}
		
		char *getBuf() {
		    return buf;
		}
		
		ictStreamItemHeader_t &header() {
			return *((ictStreamItemHeader_t*)buf);
		}

		const uint16_t getLength() const {
		    return getDataSize()+headerSize;
		}

		const uint16_t getDataLength() const {
			return getDataSize();
		}
	
		const uint16_t getNumber() const {
			return *((uint16_t*)&buf);
		}
		const uint32_t getTime() const {
			return *((uint32_t*)&buf[2]);
		}
		const char *getData() const {
			return &buf[headerSize];
		}

		void setNumber(uint16_t nr) {
			memcpy(buf,&nr,sizeof(nr));
		}
		void setTime(uint32_t time) const {
			memcpy((char*)&buf[2],&time,sizeof(time));
		}
		void setData(const char *data,const uint16_t cnt) {
			memcpy((char*)&buf[6],&cnt,sizeof(cnt));
			memcpy((char*)&buf[headerSize],data,cnt);
		}
		
		void setDataLength(uint16_t dataLen) {
			memcpy((char*)&buf[6],&dataLen,sizeof(dataLen));
		}
		
		char *data() {
			return &buf[headerSize];
		}

		const int size() const {
			return getLength();
		}

		ictStreamItem_t() {
			uint16_t cnt=0;
			memcpy((char*)&buf[6],&cnt,sizeof(cnt));
		}
		
		operator char*() {
			return (char*)&buf;
		}
		operator void*() {
			return (void*)&buf;
		}
};

#pragma pack(push)
#pragma pack(1)
struct ictStreamItemHeader_t {
		uint16_t number;
		uint32_t time;
		uint16_t length;
};
#pragma pack(pop)

#endif
