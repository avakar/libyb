#ifndef LIBYB_UTILS_SIGNAL_HPP
#define LIBYB_UTILS_SIGNAL_HPP

#include <functional>
#include <list>

namespace yb {

template <typename T>
class signal
{
public:
	template <typename F>
	void add(F && f)
	{
		m_clients.push_back(std::forward<F>(f));
	}

	template <typename F>
	void oneshot(F const & f)
	{
		m_clients.push_back([f](T const & v) -> bool { f(v); return false; });
	}

	void broadcast(T const & v)
	{
		for (std::list<std::function<bool(T)>>::iterator it = m_clients.begin(); it != m_clients.end();)
		{
			if ((*it)(v))
				++it;
			else
				it = m_clients.erase(it);
		}
	}

private:
	std::list<std::function<bool(T)>> m_clients;
};

} // namespace yb

#endif // LIBYB_UTILS_SIGNAL_HPP

