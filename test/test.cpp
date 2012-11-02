#include "test.h"
#include <iostream>
#include <set>
#include <string>

static test_registrar * g_reg = 0;

test_registrar::test_registrar(void (*testfn)(), char const * name, char const * file, int line, char const * tags)
	: testfn(testfn), name(name), file(file), line(line), tags(tags)
{
	next = g_reg;
	g_reg = this;
}

bool test_registrar::is_eligible(std::set<yb::string_ref> const & args) const
{
	bool one_selected = false;

	if (args.find(this->name) != args.end())
		one_selected = true;

	char const * first = this->tags;
	for (char const * cur = first;; ++cur)
	{
		if (*cur != ' ' && *cur != 0)
			continue;

		if (first != cur)
		{
			bool is_required = first[0] == '+';

			if (is_required && args.find(yb::string_ref(first, cur)) == args.end())
				return false;

			if (!is_required && args.find(yb::string_ref(first, cur)) != args.end())
				one_selected = true;
		}

		if (*cur == 0)
			break;

		first = cur + 1;
	}

	return one_selected || args.empty();
}

void run_tests()
{
	run_tests(0, 0);
}

void run_tests(int argc, char const * const * argv)
{
	std::set<yb::string_ref> cmdline_tests;
	for (int i = 1; i < argc; ++i)
		cmdline_tests.insert(argv[i]);

	for (test_registrar * reg = g_reg; reg != 0; reg = reg->next)
	{
		if (!reg->is_eligible(cmdline_tests))
			continue;

		std::cout << reg->name << std::endl;

		try
		{
			reg->testfn();
		}
		catch (std::exception const & e)
		{
			std::cout << reg->file << "(" << reg->line << "): error: " << e.what() << std::endl;
		}
		catch (...)
		{
			std::cout << reg->file << "(" << reg->line << "): error: unexpected exception was thrown" << std::endl;
		}
	}
}
