
#include "Application.hpp"
#include "ArgParser.hpp"
#include "version.hpp"

#include <UtilitaryRS/RsTypes.hpp>

int main(int argc, char *argv[])
{
	auto args = parseArgs(argc, argv);
	RS::DeviceVersion ver = {0, DEVICE_HW_REVISION, DEVICE_SW_MAJOR, DEVICE_SW_MINOR, DEVICE_SW_REVISION, DEVICE_GIT_HASH_SHORT};

	Application app(std::move(args), ver);
	return app.run();
}
