#include <sstream>

template <typename T, typename U>
T yb::lexical_cast(U const & u)
{
	std::ostringstream oss;
	oss << u;

	std::istringstream iss(oss.str());

	T res;
	iss >> res;
	return res;
}
