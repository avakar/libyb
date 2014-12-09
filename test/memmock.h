#ifndef MEMMOCK_H
#define MEMMOCK_H

#include <cassert>
#include <stddef.h>

struct alloc_filter_registration
{
	bool (*filter)(size_t alloc_index, void * context);
	void * context;
};

size_t get_total_alloc_count();
alloc_filter_registration set_alloc_filter(alloc_filter_registration reg);
alloc_filter_registration set_alloc_filter(bool (*filter)(size_t alloc_index, void * context), void * context);

class alloc_failer
{
public:
	explicit alloc_failer(size_t allowed_allocs = 0)
	{
		old_filter = set_alloc_filter(&alloc_failer::filter, this);
		this->reset(allowed_allocs);
	}

	~alloc_failer()
	{
		set_alloc_filter(old_filter);
	}

	void reset(size_t allowed_allocs)
	{
		base = get_total_alloc_count();
		failed_alloc_index = base + allowed_allocs;
	}

	size_t alloc_count() const
	{
		return get_total_alloc_count() - base;
	}

	static bool filter(size_t alloc_index, void * context)
	{
		alloc_failer * self = (alloc_failer *)context;
		return alloc_index != self->failed_alloc_index;
	}

private:
	size_t base;
	size_t failed_alloc_index;
	alloc_filter_registration old_filter;
};

class alloc_mocker
{
public:
	alloc_mocker()
		: m_state(st_prealloc), m_total_count(0), m_current_iteration(0)
	{
	}

	bool next()
	{
		switch (m_state)
		{
		case st_prealloc:
			failer.reset(0);
			m_state = st_measure;
			return true;
		case st_measure:
			failer.reset(0);
			m_state = st_mock;
			return true;
		case st_mock:
			if (m_total_count == 0)
				m_total_count = failer.alloc_count();

			if (m_current_iteration == m_total_count)
				return false;

			failer.reset(++m_current_iteration);
			return true;
		}
	}

	size_t alloc_count() const
	{
		return m_total_count;
	}

	bool good() const
	{
		return m_total_count == 0;
	}

private:
	enum state_t { st_prealloc, st_measure, st_mock } m_state;
	size_t m_total_count;
	size_t m_current_iteration;
	alloc_failer failer;
};

#endif
