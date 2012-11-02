#ifndef LIBTESTS_TEST_H
#define LIBTESTS_TEST_H

#include <set>
#include "../libyb/vector_ref.hpp"

struct test_registrar
{
	test_registrar(void (*testfn)(), char const * name, char const * file, int line, char const * tags);

	bool is_eligible(std::set<yb::string_ref> const & args) const;

	void (*testfn)();
	char const * name;
	char const * file;
	char const * tags;
	int line;
	test_registrar * next;
};

#define TEST_CASE(name, ...) \
	void name(); \
	test_registrar name ## _registrar(&name, #name, __FILE__, __LINE__, __VA_ARGS__); \
	void name()

void run_tests();
void run_tests(int argc, char const * const * argv);

#endif // LIBTESTS_TEST_H
