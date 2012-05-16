#ifndef PTHREADDEQUE_H
#define PTHREADDEQUE_H

#include <deque>

template <class T>
class pthread_deque: private deque<T> {
	private:
		pthread_mutex_t locker;
		pthread_cond_t  condit;
	public:
		pthread_deque() : deque<T>() {
			pthread_mutex_init(&locker,NULL);
			pthread_cond_init (&condit,NULL);
		}

		~pthread_deque() {
			pthread_mutex_destroy(&locker);
			pthread_cond_destroy (&condit);
//			~deque<T>();
		}
		
		void wait (){
			pthread_mutex_lock	( &locker );
			pthread_cond_wait	( &condit, &locker);
			pthread_mutex_unlock	( &locker );
		}
		
		bool empty() {
			pthread_mutex_lock	( &locker );
			bool result=deque<T>::empty();
			pthread_mutex_unlock	( &locker );
			return result;
		}
		
		void push_back ( const T& x ) {
			pthread_mutex_lock	( &locker );
			deque<T>::push_back(x);
			pthread_cond_signal	( &condit );
			pthread_mutex_unlock	( &locker );
		}

		void push_front ( const T& x ) {
			pthread_mutex_lock	( &locker );
			deque<T>::push_front(x);
			pthread_cond_signal	( &condit );
			pthread_mutex_unlock	( &locker );
		}

		void pop_front () {
			pthread_mutex_lock	( &locker );
			deque<T>::pop_front();
			pthread_cond_signal	( &condit );
			pthread_mutex_unlock	( &locker );
		}

		void pop_back () {
			pthread_mutex_lock	( &locker );
			deque<T>::push_back();
			pthread_cond_signal	( &condit );
			pthread_mutex_unlock	( &locker );
		}

		T &back ( ) {
			pthread_mutex_lock	( &locker );
			T &result = deque<T>::back();
			pthread_mutex_unlock	( &locker );
			
			return result;
		}

		const T &back ( ) const {
			pthread_mutex_lock	( &locker );
			const T &result = deque<T>::back();
			pthread_mutex_unlock	( &locker );
			
			return result;
		}

		T &front ( ) {
			pthread_mutex_lock	( &locker );
			T &result = deque<T>::front();
			pthread_mutex_unlock	( &locker );
			
			return result;
		}

		const T &front ( ) const {
			pthread_mutex_lock	( &locker );
			const T &result = deque<T>::front();
			pthread_mutex_unlock	( &locker );
			
			return result;
		}
};

/*
class C1 {
	public:
	~C1() {
		printf("C1 destructor called");
	}
};

class C2: protected C1 {
	public:
	~C2() {
		printf("C2 destructor called");
	}
};
*/

#endif
