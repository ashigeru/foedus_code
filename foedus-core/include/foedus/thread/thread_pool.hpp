/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_THREAD_THREAD_POOL_HPP_
#define FOEDUS_THREAD_THREAD_POOL_HPP_
#include <stdint.h>
#include <foedus/cxx11.hpp>
#include <foedus/fwd.hpp>
#include <foedus/initializable.hpp>
#include <foedus/thread/fwd.hpp>
namespace foedus {
namespace thread {

/**
 * @defgroup THREADPOOL Thread-Pool and Impersonation
 * @brief APIs to \b pool and \b impersonate worker threads in libfoedus-core
 * @ingroup THREAD
 * @details
 * @section POOL Thread Pool
 * bluh
 * @section IMPERSONATE Impersonation
 * @section EX Examples
 * bluh
 */

/**
 * @brief A pure-virtual functor object to be executed when impersonation succedes.
 * @ingroup THREADPOOL
 * @details
 * The user program should define a class that derives from this class and pass it to
 * ThreadPool#impersonate(). Each task might be fine-grained (eg. one short transaction as a task)
 * or coarse-graiend (eg. a series of transactions assigned to one thread/CPU-core, like DORA).
 * This interface is totally general, so it's up to the client.
 */
class ImpersonateTask {
 public:
    virtual ~ImpersonateTask() {}

    /**
     * @brief The method called back from the engine on one of its pooled threads.
     * @param[in] worker the impersonated thread
     * @details
     * Note that the thread calling this method is NOT the client program's thread
     * that invoked ThreadPool#impersonate(), but an asynchronous thread that was pre-allocated
     * in the engine.
     * When this method returns, the engine releases the impersonated thread to the thread pool
     * so that other clients can grab it for new sessions.
     */
    virtual ErrorStack run(Thread* worker) = 0;
};

/**
 * @brief A user session running on an \e impersonated thread.
 * @ingroup THREADPOOL
 * @details
 * @par Overview
 * This object represents an impersonated session, which is obtained by calling
 * ThreadPool#impersonate() and running the given task on a pre-allocated thread.
 * This object also works as a \e future to synchronously or asynchronously
 * wait for the completion of the functor from the client program's thread.
 *
 * @par Result of Impersonation
 * The client program has to first check is_valid() for the given session because
 * there are two possible cases as the result of impersonation.
 *  \li Impersonation succeded (is_valid()==true), running the task on the impersonated thread.
 * In this case, the client's thread (original thread that invoked ThreadPool#impersonate()) can
 * either wait for the end of the impersonated thread (call wait() or wait_for()),
 * or do other things (e.g., launch more impersonated threads).
 *
 *  \li Impersonation failed (is_valid()==false). This might happen for various reasons; timeout,
 * the engine shuts down while waiting, unexpected errors, etc.
 * In this case, get_invalid_cause() indicates the causes of the failed impersonation.
 *
 * @par Wait for the completion of the session
 * When is_valid()==true, this object also behaves as std::shared_future<ErrorStack>,
 * and we actually use it. But, because of the \ref CXX11 issue, we wrap it as a usual class.
 * This object is copiable like std::shared_future, not std::shared_future.
 * Actually, this class is based on std::shared_future just to provide copy semantics
 * for non-C++11 clients. Additional overheads shouldn't matter.
 */
class ImpersonateSession {
 public:
    friend class ThreadPoolPimpl;
    /** Result of wait_for() */
    enum Status {
        /** If called for an invalid session. */
        INVALID_SESSION = 0,
        /** The session has completed. */
        READY,
        /** Timeout duration has elapsed. */
        TIMEOUT,
    };

    explicit ImpersonateSession(ImpersonateTask* task);
    ~ImpersonateSession();
    ImpersonateSession(const ImpersonateSession& other);
    ImpersonateSession& operator=(const ImpersonateSession& other);

    /**
     * Returns if the impersonation succeeded.
     */
    bool is_valid() const;

    /**
     * Returns the error stack of the impersonation failure.
     * @pre is_valid()==false
     */
    ErrorStack  get_invalid_cause() const;
    /** Sets  the error stack of the impersonation failure. */
    void        set_invalid_cause(ErrorStack cause);

    /** Returns the impersonated task running on this session. */
    ImpersonateTask* get_task() const;

    /**
     * @brief Waits until the completion of the asynchronous session and retrieves the result.
     * @details
     * It effectively calls wait() in order to wait for the result.
     * The behavior is undefined if is_valid()== false.
     * This is analogous to std::future::get().
     * @pre is_valid()==true
     */
    ErrorStack get_result();

    /**
     * @brief Blocks until the completion of the asynchronous session.
     * @details
     * The behavior is undefined if is_valid()== false.
     * It effectively calls wait_for(-1) in order to unconditionally wait for the result.
     * This is analogous to std::future::wait().
     * @pre is_valid()==true
     */
    void wait() const;

    /**
     * @brief Waits for the completion of the asynchronous session, blocking until specified timeout
     * duration has elapsed or the session completes, whichever comes first.
     * @param[in] timeout timeout duration in microseconds.
     * @details
     * This is analogous to std::future::wait_for() although we don't provide std::chrono.
     */
    Status wait_for(TimeoutMicrosec timeout) const;

 private:
    ImpersonateSessionPimpl*     pimpl_;
};

/**
 * @brief The pool of pre-allocated threads in the engine to execute transactions.
 * @ingroup THREADPOOL
 * @details
 * This is the main API class of thread package.
 */
class ThreadPool : public virtual Initializable {
 public:
    ThreadPool() CXX11_FUNC_DELETE;
    explicit ThreadPool(Engine *engine);
    ~ThreadPool();
    ErrorStack  initialize() CXX11_OVERRIDE CXX11_FINAL;
    bool        is_initialized() const CXX11_OVERRIDE CXX11_FINAL;
    ErrorStack  uninitialize() CXX11_OVERRIDE CXX11_FINAL;

    /**
     * @brief Impersonate as one of pre-allocated threads in this engine, calling
     * back the functor from the impersonated thread (\b NOT the current thread).
     * @param[in] task the callback functor the client program should define
     * @param[in] timeout how long we wait for impersonation if there is no available thread
     * @details
     * This is similar to launch a new thread that calls the functor.
     * The difference is that this doesn't actually create a thread (which is very expensive)
     * but instead just impersonates as one of the pre-allocated threads in the engine.
     * @return The resulting session of impersonation.
     * @todo currently, timeout is ignored. It behaves as if timeout=0
     */
    ImpersonateSession  impersonate(ImpersonateTask* task, TimeoutMicrosec timeout = -1);

    /**
     * Overload to specify a NUMA node to run on.
     * @see impersonate()
     * @todo currently, timeout is ignored. It behaves as if timeout=0
     */
    ImpersonateSession  impersonate_on_numa_node(ImpersonateTask* task,
                                        ThreadGroupId numa_node, TimeoutMicrosec timeout = -1);

    /**
     * Overload to specify a core to run on.
     * @see impersonate()
     * @todo currently, timeout is ignored. It behaves as if timeout=0
     */
    ImpersonateSession  impersonate_on_numa_core(ImpersonateTask* task,
                                        ThreadId numa_core, TimeoutMicrosec timeout = -1);

 private:
    ThreadPoolPimpl*    pimpl_;
};
}  // namespace thread
}  // namespace foedus
#endif  // FOEDUS_THREAD_THREAD_POOL_HPP_