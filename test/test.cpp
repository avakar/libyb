#include "test.h"
#include <iostream>

static test_registrar * g_reg = 0;

test_registrar::test_registrar(void (*testfn)(), char const * name, char const * file, int line, char const * tags)
	: testfn(testfn), name(name), file(file), line(line), tags(tags)
{
	next = g_reg;
	g_reg = this;
}

void run_tests()
{
	for (test_registrar * reg = g_reg; reg != 0; reg = reg->next)
	{
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
