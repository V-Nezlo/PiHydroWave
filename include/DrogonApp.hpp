#ifndef DROGON_HPP
#define DROGON_HPP

#include <drogon/HttpAppFramework.h>
#include <thread>

class DrogonApp {
public:
	void start()
	{
		thread = std::thread(&DrogonApp::threadFunc, this);
		thread.detach();
	}

private:
	std::thread thread;

	void threadFunc()
	{
		// Запуск в отдельном потоке
		drogon::app()
			.addListener("0.0.0.0", 8848)
			.setThreadNum(1)
			.run();
	}
};

#endif // DROGON_HPP
