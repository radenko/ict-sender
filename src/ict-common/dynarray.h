#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <cstring>
#include <cstdlib>
#include <limits>

template <class T>
#define tsize sizeof(T)
class DynArray {
	typedef int (*ItemCompare)(const T &a,const T &b);

	protected:
		T *ptr;
		size_t datasize;

		size_t add_(const T &item) {
			setSize(datasize+1);
			
			ptr[datasize-1]=item;
			return datasize-1;
		}

		void erase_(const size_t pos,const size_t len=1) {
			if (pos>=datasize) return;
		
			if (pos+len<datasize) {
				memcpy(&ptr[pos],&ptr[pos+len],tsize*(datasize-pos-len));
				setSize(datasize-len);
			} else {
				setSize(pos);
			}
		}
	public:
		static const size_t npos=0xFFFFFFFF;
		ItemCompare itemCompare;
	
		const size_t size() const{
			return datasize;
		}
	
		DynArray() {
			ptr=NULL;
			datasize=0;
		}

		DynArray(const DynArray<T> &src) {
			ptr=NULL;
			datasize=0;

			setSize(src.datasize);
			memcpy(ptr,src.ptr,datasize*tsize);
		}
		
		~DynArray() {
			setSize(0);
		}
	
		T *data() {
			return ptr;
		}
		
		void setSize(const size_t newSize) {
			if (newSize>0) {
				ptr=(T*)realloc(ptr,newSize*tsize);
			}
			
			if (newSize<=0 && ptr!=NULL) {
				free(ptr);
				ptr=NULL;
			}
			
			datasize=newSize;
		}
		
		T &at(const size_t pos) {
			return ptr[pos];
		}

		const T &at(const size_t pos) const {
			return ptr[pos];
		}
		
		T &operator[] (const size_t pos) {
			return at(pos);
		}

		const T &operator[] (const size_t pos) const {
			return at(pos);
		}
		
		size_t add(const T &item) {
			return add_(item);
		}

		void erase(const size_t pos,const size_t len=1) {
			erase_(pos,len);
		}
		
		size_t find(const T &item) {
			if (!itemCompare) return npos;
			
			size_t result;
			for (result=0;result<size();result++) {
				if (0==itemCompare(item,at(result))) {
					return result;
				}
			}
			
			return npos;
		}

		template <typename U,typename V>
		size_t find (const U &item,V comp) {
			for (int i=0;i<size();i++) 
				if (bool(comp(at(i),item)))
					return i;
			return npos;
		}
};

template <class T>
#define tsize sizeof(T)
class DynPArray {
	typedef int (*ItemCompare)(const T *a,const T *b);

	protected:
		T **ptr;
		size_t datasize;

		T *add_() {
			setSize_(datasize+1);
			return ptr[datasize-1];
		}

		size_t add_(T *item) {
			datasize++;
			ptr=(T**)realloc(ptr,datasize*tsize);
			ptr[datasize-1]=item;
			return datasize-1;
		}

		void erase_(const size_t pos,const size_t len=1) {
			if (pos>=datasize) return;
		
			if (pos+len<datasize) {
				for (int i=0;i<len;i++){
					delete ptr[pos+i];
				}
				
				memcpy(&ptr[pos],&ptr[pos+len],tsize*(datasize-pos-len));
				size_t newSize=datasize-len;
				ptr=(T*)realloc(ptr,newSize*tsize);
				datasize=newSize;
			} else {
				setSize(pos);
			}
		}

		void setSize_(const size_t newSize) {
			if (newSize>0) {
				if (newSize<datasize) {
					for (int i=newSize;i<datasize;i++) {
						delete ptr[i];
					}
				}

				ptr=(T**)realloc(ptr,newSize*tsize);

				if (newSize>datasize) {
					for (int i=datasize;i<newSize;i++) {
						ptr[i]=new T;
					}
				}
			}
			
			if (newSize<=0 && ptr!=NULL) {
				for (int i=0;i<datasize;i++) {
					delete ptr[i];
				}
				free(ptr);
				ptr=NULL;
			}
			
			datasize=newSize;
		}
	public:
		static const size_t npos=0xFFFFFFFF;
		ItemCompare itemCompare;
	
		const size_t size() const{
			return datasize;
		}
	
		DynPArray() {
			ptr=NULL;
			datasize=0;
		}

		DynPArray(const DynArray<T> &src) {
			ptr=NULL;
			datasize=0;

			setSize(src.datasize);
			memcpy(ptr,src.ptr,datasize*tsize);
		}
		
		~DynPArray() {
			setSize(0);
		}

		T **data() {
			return ptr;
		}
		
		
		T *at(const size_t pos) {
			return ptr[pos];
		}

		void setSize(const size_t newSize) {
			setSize_(newSize);
		}

		const T *at(const size_t pos) const {
			return ptr[pos];
		}
		
		T *operator[] (const size_t pos) {
			return at(pos);
		}

		const T *operator[] (const size_t pos) const {
			return at(pos);
		}
		
		T *add() {
			return add_();
		}

		size_t add(T &item) {
			T *itemPtr=add_();
			(*itemPtr)=item;
			return datasize-1;
		}

		size_t add(T *item) {
			return add_(*item);
		}

		void erase(const size_t pos,const size_t len=1) {
			erase_(pos,len);
		}
		
		T *back() {
			return at(size()-1);
		}
		const T *back() const {
			return at(size()-1);
		}

		size_t find(const T *item) {
			if (!itemCompare) return npos;
			
			size_t result;
			for (result=0;result<size();result++) {
				if (*item==*at(result)) {
					return result;
				}
			}
			
			return npos;
		}

		template <typename U,typename V>
		T *find (const U &item,V comp) {
			for (int i=0;i<size();i++) 
				if (bool(comp(at(i),item)))
					return at(i);
			return NULL;
		}

		template <typename U,typename V>
		size_t getPos (const U &item,V comp) {
			for (int i=0;i<size();i++) 
				if (bool(comp(at(i),item)))
					return i;
			return npos;
		}
};

/*
    int main (int argc,char *argv[]) {
	dynarray<int> intarr1;
	
	intarr1.setSize(12);
	intarr1[4]=100;
	dynarray<int> intarr2(intarr1);
	
	intarr1.setSize(0);
	
	intarr2.add(1222);
	intarr2.erase(1);
	intarr2.erase(2,400);
	
	for (int i=0;i<intarr2.size();i++) {
		printf("%d,",intarr2[i]);
	}
	
	printf("item %p,%d\n",intarr2.data(),intarr2[4]);
    }
*/

#endif
