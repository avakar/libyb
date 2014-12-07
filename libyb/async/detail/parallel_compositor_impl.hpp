namespace yb {
namespace detail {

/*template <typename F>
void parallel_compositor::cancel_and_wait(F f) throw()
{
	for (std::list<parallel_task>::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it)
		f(it->t.cancel_and_wait());
	m_tasks.clear();
}

template <typename F>
void parallel_compositor::finish_wait(task_wait_finalization_context & ctx, F f) throw()
{
	for (std::list<parallel_task>::iterator it = m_tasks.begin(); it != m_tasks.end();)
	{
		if (ctx.contains(it->m))
		{
			it->t.finish_wait(ctx);
			if (it->t.has_value() || it->t.has_exception())
			{
				f(it->t);
				it = m_tasks.erase(it);
				continue;
			}
		}

		++it;
	}
}*/

} // namespace detail
} // namespace yb
