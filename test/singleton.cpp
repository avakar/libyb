#include "test.h"
#include <libyb/utils/singleton.hpp>

size_t test_singleton_construct_count = 0;
size_t test_singleton_destruct_count = 0;
size_t test_singleton_test_count = 0;

struct test_singleton
{
	test_singleton()
	{
		++test_singleton_construct_count;
	}

	~test_singleton()
	{
		++test_singleton_destruct_count;
	}

	void test()
	{
		++test_singleton_test_count;
	}
};

TEST_CASE(SingletonTest, "singleton")
{
	yb::singleton<test_singleton> ss;

	{
		yb::singleton_ptr<test_singleton> sp;
		sp.reset(ss);
		sp->test();
	}

	{
		yb::singleton_ptr<test_singleton> sp;
		sp.reset(ss);
	}

	{
		yb::singleton_ptr<test_singleton> sp1(ss);
		yb::singleton_ptr<test_singleton> sp2(ss);
	}

	assert(test_singleton_construct_count == 3);
	assert(test_singleton_destruct_count == 3);
	assert(test_singleton_test_count == 1);
}
