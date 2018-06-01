/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------

cpthread.c
Base on thread.h - v0.3.1 - Cross platform threading functions for C/C++.
*/


#include "cpthread.h"

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
    #pragma warning( disable: 4668 ) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
    #pragma warning( disable: 4255 )
    #include <windows.h>
    #pragma warning( pop )

    // To set thread name
    const DWORD MS_VC_EXCEPTION = 0x406D1388;
    #pragma pack( push, 8 )
    typedef struct tagTHREADNAME_INFO
        {
        DWORD dwType;
        LPCSTR szName;
        DWORD dwThreadID;
        DWORD dwFlags;
        } THREADNAME_INFO;
    #pragma pack(pop)

#elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

#include <pthread.h>
#include <sys/time.h>

#else
#error Unknown platform.
#endif


#ifndef NDEBUG
#include <assert.h>
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


thread_ptr_t thread_create( int (*thread_proc)( void* ), void* user_data, char const* name, int stack_size )
{
#if defined( _WIN32 )

    DWORD thread_id;
        HANDLE handle = CreateThread( NULL, stack_size > 0 ? (size_t)stack_size : 0U,
            (LPTHREAD_START_ROUTINE)(uintptr_t) thread_proc, user_data, 0, &thread_id );
        if( !handle ) return NULL;

        // Yes, this crazy construct with __try and RaiseException is how you name a thread in Visual Studio :S
        if( name && IsDebuggerPresent() )
            {
            THREADNAME_INFO info;
            info.dwType = 0x1000;
            info.szName = name;
            info.dwThreadID = thread_id;
            info.dwFlags = 0;

            __try
                {
                RaiseException( MS_VC_EXCEPTION, 0, sizeof( info ) / sizeof( ULONG_PTR ), (ULONG_PTR*) &info );
                }
            __except( EXCEPTION_EXECUTE_HANDLER )
                {
                }
            }

        return (thread_ptr_t) handle;

#elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

    pthread_t thread;
    if( 0 != pthread_create( &thread, NULL, ( void* (*)( void * ) ) thread_proc, user_data ) )
        return NULL;

#if !defined( __APPLE__ ) // max doesn't support pthread_setname_np. alternatives?
    if( name ) pthread_setname_np( thread, name );
#endif

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
                int res = SleepConditionVariableCS( &internal->condition, &internal->mutex, timeout_ms < 0 ? INFINITE : timeout_ms );
                if( !res && GetLastError() == ERROR_TIMEOUT ) { timed_out = 1; break; }
                }
            if( !timed_out ) internal->value = 0;
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

    __sync_lock_test_and_set( &atomic->i, desired );
    __sync_lock_release( &atomic->i );

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

    pthread_key_delete( (pthread_key_t) tls );

#else
#error Unknown platform.
#endif
}


void thread_tls_set( thread_tls_t tls, void* value )
{
#if defined( _WIN32 )

    TlsSetValue( (DWORD) (uintptr_t) tls, value );

#elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

    pthread_setspecific( (pthread_key_t) tls, value );

#else
#error Unknown platform.
#endif
}


void* thread_tls_get( thread_tls_t tls )
{
#if defined( _WIN32 )

    return TlsGetValue( (DWORD) (uintptr_t) tls );

#elif defined( __linux__ ) || defined( __APPLE__ ) || defined( __ANDROID__ )

    return pthread_getspecific( (pthread_key_t) tls );

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
    assert( thread_current_thread_id() == queue->id_produce );
#endif
    if( thread_atomic_int_load( &queue->count ) == queue->size )
    {
        if( timeout_ms == 0 ) return 0;
        thread_signal_wait( &queue->space_open, timeout_ms == THREAD_QUEUE_WAIT_INFINITE ? THREAD_SIGNAL_WAIT_INFINITE : timeout_ms );
    }
    int tail = thread_atomic_int_inc( &queue->tail );
    queue->values[ tail % queue->size ] = value;
    if( thread_atomic_int_inc( &queue->count ) == 0 )
        thread_signal_raise( &queue->data_ready );
    return 0;
}


void* thread_queue_consume( thread_queue_t* queue, int timeout_ms )
{
#ifndef NDEBUG
    if( thread_atomic_int_compare_and_swap( &queue->id_consume_is_set, 0, 1 ) == 0 )
        queue->id_consume = thread_current_thread_id();
    assert( thread_current_thread_id() == queue->id_consume );
#endif
    if( thread_atomic_int_load( &queue->count ) == 0 )
    {
        if( timeout_ms == 0 ) return NULL;
        thread_signal_wait( &queue->data_ready, THREAD_SIGNAL_WAIT_INFINITE );
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

/*
revision history:
    0.3.1   Adapted into two files + added includes.
    0.3     set_high_priority API change. Fixed spurious wakeup bug in signal. Added
            timeout param to queue produce/consume. Various cleanup and trivial fixes.
    0.2     first publicly released version
*/

/*
------------------------------------------------------------------------------

This software is available under 2 licenses - you may choose the one you like.

------------------------------------------------------------------------------

ALTERNATIVE A - MIT License

Copyright (c) 2015 Mattias Gustavsson

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------

ALTERNATIVE B - Public Domain (www.unlicense.org)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

------------------------------------------------------------------------------
*/