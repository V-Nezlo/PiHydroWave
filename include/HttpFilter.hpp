//
// HttpFilter.hpp
//
//  Created on: Jan 12, 2026
//      Author: N.Zyablov
//

#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/HttpTypes.h>

class HardeningFilter : public drogon::HttpFilter<HardeningFilter, false> {
public:
	void doFilter(
		const drogon::HttpRequestPtr &req, drogon::FilterCallback &&fcb, drogon::FilterChainCallback &&fccb) override
	{
		constexpr size_t kMaxBody = 16 * 1024;
		constexpr size_t kMaxQuery = 1024;
		constexpr size_t kMaxHeaders = 32;

		if (req->getBody().size() > kMaxBody) {
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setStatusCode(drogon::k413RequestEntityTooLarge);
			return fcb(resp);
		}

		if (auto it = req->getParameters().find("q"); it != req->getParameters().end()) {
			if (it->second.size() > kMaxQuery) {
				auto resp = drogon::HttpResponse::newHttpResponse();
				resp->setStatusCode(drogon::k414RequestURITooLarge);
				return fcb(resp);
			}
		}

		if (req->headers().size() > kMaxHeaders) {
			auto resp = drogon::HttpResponse::newHttpResponse();
			resp->setStatusCode(drogon::k400BadRequest);
			return fcb(resp);
		}

		return fccb();
	}
};
