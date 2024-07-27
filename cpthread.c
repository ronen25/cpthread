#include "cpthread.h"

union thread_mutex_t 
    { 
    void* align; 
    char data[ 64 ];
    };

union thread_signal_t 
    { 
    void* align; 
    char data[ 116 ];
    };

union thread_atomic_int_t 
    {
    void* align;
    long i;
    };

union thread_atomic_ptr_t 
    {
    void* ptr;
    };

union thread_timer_t 
    { 
    void* data; 
    char d[ 8 ]; 
    };

struct thread_queue_t
    {
    thread_signal_t data_ready;
    thread_signal_t space_open;
    thread_atomic_int_t count;
    thread_atomic_int_t head;
    thread_atomic_int_t tail;
    void** values;
    int size;
    #ifndef NDEBUG
        thread_atomic_int_t id_produce_is_set;
        thread_id_t id_produce;
        thread_atomic_int_t id_consume_is_set;
        thread_id_t id_consume;
    #endif
    };

#ifndef THREAD_ASSERT
    #undef _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #undef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
    #include <assert.h>
    #define THREAD_ASSERT( expression, message ) assert( ( expression ) && ( message ) )
#endif


#if defined( _WIN32 )

    #pragma comment( lib, "winmm.lib" )

    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS

    #if !defined( _WIN32_WINNT ) || _WIN32_WINNT < 0x0501 
        #undef _WIN32_WINNT
        #define _WIN32_WINNT 0x501// requires Windows XP minimum
    #endif

    #define _WINSOCKAPI_
    #pragma warning( push )
    #pragma warning( disable: 4619 ) 
    #pragma warning( disable: 4668 ) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
    #pragma warning( disable: 4768 ) // __declspec attributes before linkage specification are ignored	
    #pragma warning( disable: 4255 ) // 'function' : no function prototype given: converting '()' to '(void)'
    #include <windows.h>
    #pragma warning( pop )

   
#elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

    #include <pthread.h>
    #include <errno.h>
    #include <string.h>
    #include <sys/time.h>
    #include <stdint.h>

#else 
    #error Unknown platform.
#endif



thread_id_t thread_current_thread_id( void )
    {
    #if defined( _WIN32 )

        return (void*) (uintptr_t)GetCurrentThreadId();

    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )
    
        return (void*) pthread_self();

    #else 
        #error Unknown platform.
    #endif
    }


void thread_yield( void )
    {
    #if defined( _WIN32 )

        SwitchToThread();
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )
    
        sched_yield();
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_exit( int return_code )
    {
    #if defined( _WIN32 )

        ExitThread( (DWORD) return_code );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )
    
        pthread_exit( (void*)(uintptr_t) return_code );
    
    #else 
        #error Unknown platform.
    #endif
    }


thread_ptr_t thread_create( int (*thread_proc)( void* ), void* user_data, int stack_size )
    {
    #if defined( _WIN32 )

        DWORD thread_id;
        HANDLE handle = CreateThread( NULL, stack_size > 0 ? (size_t)stack_size : 0U, 
            (LPTHREAD_START_ROUTINE)(uintptr_t) thread_proc, user_data, 0, &thread_id );
        if( !handle ) return NULL;

        return (thread_ptr_t) handle;
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_t thread;
        if( 0 != pthread_create( &thread, NULL, ( void* (*)( void * ) ) thread_proc, user_data ) )
            return NULL;

        return (thread_ptr_t) thread;
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_destroy( thread_ptr_t thread )
    {
    #if defined( _WIN32 )

        WaitForSingleObject( (HANDLE) thread, INFINITE );
        CloseHandle( (HANDLE) thread );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_join( (pthread_t) thread, NULL );

    #else 
        #error Unknown platform.
    #endif
    }


int thread_join( thread_ptr_t thread )
    {
    #if defined( _WIN32 )

        WaitForSingleObject( (HANDLE) thread, INFINITE );
        DWORD retval;
        GetExitCodeThread( (HANDLE) thread, &retval );
        return (int) retval;
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        void* retval;
        pthread_join( (pthread_t) thread, &retval );
        return (int)(uintptr_t) retval;

    #else 
        #error Unknown platform.
    #endif
    }


int thread_detach( thread_ptr_t thread )
    {
    #if defined( _WIN32 )

        return CloseHandle( (HANDLE) thread ) != 0;

    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return pthread_detach( (pthread_t) thread ) == 0;

    #else 
        #error Unknown platform.
    #endif
    }

    
void thread_set_high_priority( void )
    {
    #if defined( _WIN32 )

        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        struct sched_param sp;
        memset( &sp, 0, sizeof( sp ) );
        sp.sched_priority = sched_get_priority_min( SCHED_RR );
        pthread_setschedparam( pthread_self(), SCHED_RR, &sp);

    #else 
        #error Unknown platform.
    #endif
    }


void thread_mutex_init( thread_mutex_t* mutex )
    {
    #if defined( _WIN32 )

        // Compile-time size check
        #pragma warning( push )
        #pragma warning( disable: 4214 ) // nonstandard extension used: bit field types other than int
        struct x { char thread_mutex_type_too_small : ( sizeof( thread_mutex_t ) < sizeof( CRITICAL_SECTION ) ? 0 : 1 ); }; 
        #pragma warning( pop )

        InitializeCriticalSectionAndSpinCount( (CRITICAL_SECTION*) mutex, 32 );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        // Compile-time size check
        struct x { char thread_mutex_type_too_small : ( sizeof( thread_mutex_t ) < sizeof( pthread_mutex_t ) ? 0 : 1 ); };

        pthread_mutex_init( (pthread_mutex_t*) mutex, NULL );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_mutex_term( thread_mutex_t* mutex )
    {
    #if defined( _WIN32 )
        
        DeleteCriticalSection( (CRITICAL_SECTION*) mutex );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_destroy( (pthread_mutex_t*) mutex );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_mutex_lock( thread_mutex_t* mutex )
    {
    #if defined( _WIN32 )

        EnterCriticalSection( (CRITICAL_SECTION*) mutex );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_lock( (pthread_mutex_t*) mutex );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_mutex_unlock( thread_mutex_t* mutex )
    {
    #if defined( _WIN32 )

        LeaveCriticalSection( (CRITICAL_SECTION*) mutex );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_unlock( (pthread_mutex_t*) mutex );
    
    #else 
        #error Unknown platform.
    #endif
    }


struct thread_internal_signal_t
    {
    #if defined( _WIN32 )

        #if _WIN32_WINNT >= 0x0600
            CRITICAL_SECTION mutex;
            CONDITION_VARIABLE condition;
            int value;
        #else 
            #pragma message( "Warning: _WIN32_WINNT < 0x0600 - condition variables not available" )
            HANDLE event;
        #endif 
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_t mutex;
        pthread_cond_t condition;
        int value;

    #else 
        #error Unknown platform.
    #endif
    };


void thread_signal_init( thread_signal_t* signal )
    {
    // Compile-time size check
    #pragma warning( push )
    #pragma warning( disable: 4214 ) // nonstandard extension used: bit field types other than int
    struct x { char thread_signal_type_too_small : ( sizeof( thread_signal_t ) < sizeof( struct thread_internal_signal_t ) ? 0 : 1 ); };
    #pragma warning( pop )
    
    struct thread_internal_signal_t* internal = (struct thread_internal_signal_t*) signal;
        
    #if defined( _WIN32 )

        #if _WIN32_WINNT >= 0x0600
            InitializeCriticalSectionAndSpinCount( &internal->mutex, 32 );
            InitializeConditionVariable( &internal->condition );
            internal->value = 0;
        #else 
            internal->event = CreateEvent( NULL, FALSE, FALSE, NULL );
        #endif 
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_init( &internal->mutex, NULL );
        pthread_cond_init( &internal->condition, NULL );
        internal->value = 0;
    
    #else 
        #error Unknown platform.
    #endif
    }


 void thread_signal_term( thread_signal_t* signal )
    {
    struct thread_internal_signal_t* internal = (struct thread_internal_signal_t*) signal;

    #if defined( _WIN32 )

        #if _WIN32_WINNT >= 0x0600
            DeleteCriticalSection( &internal->mutex );
        #else 
            CloseHandle( internal->event );
        #endif 
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_destroy( &internal->mutex );
        pthread_cond_destroy( &internal->condition );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_signal_raise( thread_signal_t* signal )
    {
    struct thread_internal_signal_t* internal = (struct thread_internal_signal_t*) signal;

    #if defined( _WIN32 )

        #if _WIN32_WINNT >= 0x0600
            EnterCriticalSection( &internal->mutex );
            internal->value = 1;
            LeaveCriticalSection( &internal->mutex );       
            WakeConditionVariable( &internal->condition );
        #else 
            SetEvent( internal->event );
        #endif 
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_mutex_lock( &internal->mutex );
        internal->value = 1;
        pthread_mutex_unlock( &internal->mutex );
        pthread_cond_signal( &internal->condition );
    
    #else 
        #error Unknown platform.
    #endif
    }


int thread_signal_wait( thread_signal_t* signal, int timeout_ms )
    {
    struct thread_internal_signal_t* internal = (struct thread_internal_signal_t*) signal;

    #if defined( _WIN32 )

        #if _WIN32_WINNT >= 0x0600
            int timed_out = 0;
            EnterCriticalSection( &internal->mutex );
            while( internal->value == 0 )
                {
                BOOL res = SleepConditionVariableCS( &internal->condition, &internal->mutex, timeout_ms < 0 ? INFINITE : timeout_ms );
                if( !res && GetLastError() == ERROR_TIMEOUT ) { timed_out = 1; break; }
                }
            internal->value = 0;
            LeaveCriticalSection( &internal->mutex );       
            return !timed_out;
        #else 
            int failed = WAIT_OBJECT_0 != WaitForSingleObject( internal->event, timeout_ms < 0 ? INFINITE : timeout_ms );
            return !failed;
        #endif 
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        struct timespec ts;
        if( timeout_ms >= 0 )
            {
            struct timeval tv;
            gettimeofday( &tv, NULL );
            ts.tv_sec = time( NULL ) + timeout_ms / 1000;
            ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * ( timeout_ms % 1000 );
            ts.tv_sec += ts.tv_nsec / ( 1000 * 1000 * 1000 );
            ts.tv_nsec %= ( 1000 * 1000 * 1000 );
            }

        int timed_out = 0;
        pthread_mutex_lock( &internal->mutex );
        while( internal->value == 0 )
            {
            if( timeout_ms < 0 ) 
                pthread_cond_wait( &internal->condition, &internal->mutex );
            else if( pthread_cond_timedwait( &internal->condition, &internal->mutex, &ts ) == ETIMEDOUT )
                {
                timed_out = 1;
                break;
                }

            }           
        if( !timed_out ) internal->value = 0;
        pthread_mutex_unlock( &internal->mutex );
        return !timed_out;
    
    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_load( thread_atomic_int_t* atomic )
    {
    #if defined( _WIN32 )

        return InterlockedCompareExchange( &atomic->i, 0, 0 );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return (int)__sync_fetch_and_add( &atomic->i, 0 );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_atomic_int_store( thread_atomic_int_t* atomic, int desired )
    {
    #if defined( _WIN32 )
    
        InterlockedExchange( &atomic->i, desired );

    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        __sync_fetch_and_and( &atomic->i, 0 );
        __sync_fetch_and_or( &atomic->i, desired );
    
    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_inc( thread_atomic_int_t* atomic )
    {
    #if defined( _WIN32 )
    
        return InterlockedIncrement( &atomic->i ) - 1;
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return (int)__sync_fetch_and_add( &atomic->i, 1 );
    
    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_dec( thread_atomic_int_t* atomic )
    {
    #if defined( _WIN32 )
    
        return InterlockedDecrement( &atomic->i ) + 1;
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return (int)__sync_fetch_and_sub( &atomic->i, 1 );

    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_add( thread_atomic_int_t* atomic, int value )
    {
    #if defined( _WIN32 )
    
        return InterlockedExchangeAdd ( &atomic->i, value );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return (int)__sync_fetch_and_add( &atomic->i, value );
    
    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_sub( thread_atomic_int_t* atomic, int value )
    {
    #if defined( _WIN32 )
    
        return InterlockedExchangeAdd( &atomic->i, -value );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return (int)__sync_fetch_and_sub( &atomic->i, value );

    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_swap( thread_atomic_int_t* atomic, int desired )
    {
    #if defined( _WIN32 )
    
        return InterlockedExchange( &atomic->i, desired );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        int old = (int)__sync_lock_test_and_set( &atomic->i, desired );
        __sync_lock_release( &atomic->i );
        return old;
    
    #else 
        #error Unknown platform.
    #endif
    }


int thread_atomic_int_compare_and_swap( thread_atomic_int_t* atomic, int expected, int desired )
    {
    #if defined( _WIN32 )
    
        return InterlockedCompareExchange( &atomic->i, desired, expected );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return (int)__sync_val_compare_and_swap( &atomic->i, expected, desired );
    
    #else 
        #error Unknown platform.
    #endif
    }


void* thread_atomic_ptr_load( thread_atomic_ptr_t* atomic )
    {
    #if defined( _WIN32 )
    
        return InterlockedCompareExchangePointer( &atomic->ptr, 0, 0 );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return __sync_fetch_and_add( &atomic->ptr, 0 );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_atomic_ptr_store( thread_atomic_ptr_t* atomic, void* desired )
    {
    #if defined( _WIN32 )
    
        #pragma warning( push )
        #pragma warning( disable: 4302 ) // 'type cast' : truncation from 'void *' to 'LONG'
        #pragma warning( disable: 4311 ) // pointer truncation from 'void *' to 'LONG'
        #pragma warning( disable: 4312 ) // conversion from 'LONG' to 'PVOID' of greater size
        InterlockedExchangePointer( &atomic->ptr, desired );
        #pragma warning( pop )

    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        __sync_lock_test_and_set( &atomic->ptr, desired );
        __sync_lock_release( &atomic->ptr );
    
    #else 
        #error Unknown platform.
    #endif
    }


void* thread_atomic_ptr_swap( thread_atomic_ptr_t* atomic, void* desired )
    {
    #if defined( _WIN32 )
    
        #pragma warning( push )
        #pragma warning( disable: 4302 ) // 'type cast' : truncation from 'void *' to 'LONG'
        #pragma warning( disable: 4311 ) // pointer truncation from 'void *' to 'LONG'
        #pragma warning( disable: 4312 ) // conversion from 'LONG' to 'PVOID' of greater size
        return InterlockedExchangePointer( &atomic->ptr, desired );
        #pragma warning( pop )
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        void* old = __sync_lock_test_and_set( &atomic->ptr, desired );
        __sync_lock_release( &atomic->ptr );
        return old;
    
    #else 
        #error Unknown platform.
    #endif
    }


void* thread_atomic_ptr_compare_and_swap( thread_atomic_ptr_t* atomic, void* expected, void* desired )
    {
    #if defined( _WIN32 )
    
        return InterlockedCompareExchangePointer( &atomic->ptr, desired, expected );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return __sync_val_compare_and_swap( &atomic->ptr, expected, desired );

    #else 
        #error Unknown platform.
    #endif
    }


void thread_timer_init( thread_timer_t* timer )
    {
    #if defined( _WIN32 )

        // Compile-time size check
        #pragma warning( push )
        #pragma warning( disable: 4214 ) // nonstandard extension used: bit field types other than int
        struct x { char thread_timer_type_too_small : ( sizeof( thread_mutex_t ) < sizeof( HANDLE ) ? 0 : 1 ); }; 
        #pragma warning( pop )

        TIMECAPS tc;
        if( timeGetDevCaps( &tc, sizeof( TIMECAPS ) ) == TIMERR_NOERROR ) 
            timeBeginPeriod( tc.wPeriodMin );

        *(HANDLE*)timer = CreateWaitableTimer( NULL, TRUE, NULL );

    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        // Nothing

    #else 
        #error Unknown platform.
    #endif
    }


void thread_timer_term( thread_timer_t* timer )
    {
    #if defined( _WIN32 )

        CloseHandle( *(HANDLE*)timer );
    
        TIMECAPS tc;
        if( timeGetDevCaps( &tc, sizeof( TIMECAPS ) ) == TIMERR_NOERROR ) 
            timeEndPeriod( tc.wPeriodMin );

    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        // Nothing
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_timer_wait( thread_timer_t* timer, THREAD_U64 nanoseconds )
    {
    #if defined( _WIN32 )

        LARGE_INTEGER due_time;
        due_time.QuadPart = - (LONGLONG) ( nanoseconds / 100 );
        BOOL b = SetWaitableTimer( *(HANDLE*)timer, &due_time, 0, 0, 0, FALSE );
        (void) b;
        WaitForSingleObject( *(HANDLE*)timer, INFINITE ); 
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        struct timespec rem;
        struct timespec req;
        req.tv_sec = nanoseconds / 1000000000ULL;
        req.tv_nsec = nanoseconds - req.tv_sec * 1000000000ULL;
        while( nanosleep( &req, &rem ) )
            req = rem;
    
    #else 
        #error Unknown platform.
    #endif
    }


thread_tls_t thread_tls_create( void )
    {
    #if defined( _WIN32 )

        DWORD tls = TlsAlloc();      
        if( tls == TLS_OUT_OF_INDEXES )
            return NULL;
        else
            return (thread_tls_t) (uintptr_t) tls;
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_key_t tls;
        if( pthread_key_create( &tls, NULL ) == 0 )
            return (thread_tls_t) tls;
        else
            return NULL;

    #else 
        #error Unknown platform.
    #endif
    }


void thread_tls_destroy( thread_tls_t tls )
    {
    #if defined( _WIN32 )

        TlsFree( (DWORD) (uintptr_t) tls );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_key_delete( (pthread_key_t) (uintptr_t) tls );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_tls_set( thread_tls_t tls, void* value )
    {
    #if defined( _WIN32 )

        TlsSetValue( (DWORD) (uintptr_t) tls, value );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        pthread_setspecific( (pthread_key_t) (uintptr_t) tls, value );
    
    #else 
        #error Unknown platform.
    #endif
    }


void* thread_tls_get( thread_tls_t tls )
    {
    #if defined( _WIN32 )

        return TlsGetValue( (DWORD) (uintptr_t) tls );
    
    #elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

        return pthread_getspecific( (pthread_key_t) (uintptr_t) tls );
    
    #else 
        #error Unknown platform.
    #endif
    }


void thread_queue_init( thread_queue_t* queue, int size, void** values, int count )
    {
    queue->values = values;
    thread_signal_init( &queue->data_ready );
    thread_signal_init( &queue->space_open );
    thread_atomic_int_store( &queue->head, 0 );
    thread_atomic_int_store( &queue->tail, count > size ? size : count );
    thread_atomic_int_store( &queue->count, count > size ? size : count );
    queue->size = size;
    #ifndef NDEBUG
        thread_atomic_int_store( &queue->id_produce_is_set, 0 );
        thread_atomic_int_store( &queue->id_consume_is_set, 0 );
    #endif
    }


void thread_queue_term( thread_queue_t* queue )
    {
    thread_signal_term( &queue->space_open );
    thread_signal_term( &queue->data_ready );
    }


int thread_queue_produce( thread_queue_t* queue, void* value, int timeout_ms )
    {
    #ifndef NDEBUG
        if( thread_atomic_int_compare_and_swap( &queue->id_produce_is_set, 0, 1 ) == 0 )
            queue->id_produce = thread_current_thread_id();
        THREAD_ASSERT( thread_current_thread_id() == queue->id_produce, "thread_queue_produce called from multiple threads" );
    #endif
    while( thread_atomic_int_load( &queue->count ) == queue->size )  // TODO: fix signal so that this can be an "if" instead of "while"
        {
        if( timeout_ms == 0 ) return 0;
        if( thread_signal_wait( &queue->space_open, timeout_ms == THREAD_QUEUE_WAIT_INFINITE ? THREAD_SIGNAL_WAIT_INFINITE : timeout_ms ) == 0 )
            return 0;
        }
    int tail = thread_atomic_int_inc( &queue->tail );
    queue->values[ tail % queue->size ] = value;
    if( thread_atomic_int_inc( &queue->count ) == 0 )
        thread_signal_raise( &queue->data_ready );
    return 1;
    }


void* thread_queue_consume( thread_queue_t* queue, int timeout_ms )
    {
    #ifndef NDEBUG
        if( thread_atomic_int_compare_and_swap( &queue->id_consume_is_set, 0, 1 ) == 0 )
            queue->id_consume = thread_current_thread_id();
        THREAD_ASSERT( thread_current_thread_id() == queue->id_consume, "thread_queue_consume called from multiple threads" );
    #endif
    while( thread_atomic_int_load( &queue->count ) == 0 ) // TODO: fix signal so that this can be an "if" instead of "while"
        {
        if( timeout_ms == 0 ) return NULL;
        if( thread_signal_wait( &queue->data_ready, timeout_ms == THREAD_QUEUE_WAIT_INFINITE ? THREAD_SIGNAL_WAIT_INFINITE : timeout_ms ) == 0 )
            return NULL;
        }
    int head = thread_atomic_int_inc( &queue->head );
    void* retval = queue->values[ head % queue->size ];
    if( thread_atomic_int_dec( &queue->count ) == queue->size )
        thread_signal_raise( &queue->space_open );
    return retval;
    }

    
int thread_queue_count( thread_queue_t* queue )
    {
    return thread_atomic_int_load( &queue->count );
    }
