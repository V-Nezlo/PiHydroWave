#ifndef DROGON_HPP
#define DROGON_HPP

#include "BackRestController.hpp"
#include "BackWebSocket.hpp"

#include <drogon/HttpAppFramework.h>
#include <functional>
#include <thread>

class DrogonApp {
public:
	DrogonApp(std::shared_ptr<BackRestController> aRest, std::shared_ptr<BackWebSocket> aSocket):
		rest{aRest},
		webSocket{aSocket}
	{

	}

	void start()
	{
		thread = std::thread(&DrogonApp::threadFunc, this);
		thread.detach();
	}

	void setTerminator(std::function<void()> aTerminator)
	{
		drogon::app().setTermSignalHandler(aTerminator).setIntSignalHandler(aTerminator);
	}

private:
	std::thread thread;

	void threadFunc()
	{
		// Запуск в отдельном потоке
		drogon::app()
			.addListener("0.0.0.0", 8848)
			.setThreadNum(1)
			// .setDocumentRoot("/etc/www/pihydro/browser")
			.setDocumentRoot("/home/nikita/proj/PiHydroWave/frontend/HydroFrontend/dist/HydroFrontend/browser")
			.registerController(rest)
			.registerController(webSocket)
			.run();
	}

	std::shared_ptr<BackRestController> rest;
	std::shared_ptr<BackWebSocket> webSocket;
};

#endif // DROGON_HPP
