#ifndef DROGON_HPP
#define DROGON_HPP

#include "HttpFilter.hpp"
#include "BackRestController.hpp"
#include "BackWebSocket.hpp"

#include <drogon/HttpAppFramework.h>
#include <functional>
#include <thread>

class DrogonApp {
public:
	DrogonApp(std::shared_ptr<BackRestController> aRest, std::shared_ptr<BackWebSocket> aSocket, std::shared_ptr<HardeningFilter> aFilter):
		rest{aRest},
		webSocket{aSocket},
		filter{aFilter}
	{
		Json::CharReaderBuilder builder;
		builder["collectComments"] = false;
		builder["failIfExtra"] = true;
		builder["rejectDupKeys"] = true;
		builder["stackLimit"] = 200;
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
			.addListener("::", 8848)
			.setThreadNum(1)
			// .setDocumentRoot("/etc/www/pihydro/browser")
			.setClientMaxBodySize(16 * 1024)
			.setDocumentRoot("/home/nikita/proj/PiHydroWave/frontend/HydroFrontend/dist/HydroFrontend/browser")
			.registerController(rest)
			.registerController(webSocket)
			.registerFilter(filter)
			.setKeepaliveRequestsNumber(100)
			.setPipeliningRequestsNumber(16)
			.setMaxConnectionNum(64)
			.setMaxConnectionNumPerIP(16)
			.setUploadPath("/tmp/drogon-uploads")
			.run();
	}

	std::shared_ptr<BackRestController> rest;
	std::shared_ptr<BackWebSocket> webSocket;
	std::shared_ptr<HardeningFilter> filter;
};

#endif // DROGON_HPP
