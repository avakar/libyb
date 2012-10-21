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

void run_tests()
{
	run_tests(0, 0);
}

void run_tests(int argc, char const * const * argv)
{
	std::set<std::string> cmdline_tests;
	for (int i = 1; i < argc; ++i)
		cmdline_tests.insert(argv[i]);

	for (test_registrar * reg = g_reg; reg != 0; reg = reg->next)
	{
		if (!cmdline_tests.empty() && cmdline_tests.find(reg->name) == cmdline_tests.end())
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
