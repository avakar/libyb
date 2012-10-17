struct test_registrar
{
	test_registrar(void (*testfn)(), char const * name, char const * file, int line, char const * tags);

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
