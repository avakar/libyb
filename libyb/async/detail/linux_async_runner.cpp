#include "../async_runner.hpp"
#include "../sync_runner.hpp"
#include "../promise.hpp"
#include <pthread.h>
using namespace yb;

struct async_runner::impl
{
    sync_runner m_runner;
    promise<void> m_stop_promise;
    pthread_t m_thread;

    impl()
        : m_runner(/*associate_thread_now=*/false)
    {
    }

    static void * thread_proc(void * param);
};

void * async_runner::impl::thread_proc(void * param)
{
    impl * pimpl = static_cast<impl *>(param);
    pimpl->m_runner.associate_current_thread();
    pimpl->m_runner.run(pimpl->m_stop_promise.wait_for());
    return 0;
}

async_runner::async_runner()
    : m_pimpl(new impl())
{
    int r = pthread_create(&m_pimpl->m_thread, 0, &impl::thread_proc, m_pimpl.get());
    if (r != 0)
        throw std::runtime_error("Failed to create thread"); // TODO: errcode
}

async_runner::~async_runner()
{
    m_pimpl->m_stop_promise.set_value();
    pthread_join(m_pimpl->m_thread, 0);
}

void async_runner::submit(detail::prepared_task * pt)
{
    m_pimpl->m_runner.submit(pt);
}

void async_runner::run_until(detail::prepared_task * pt)
{
    pt->shadow_wait();
}
