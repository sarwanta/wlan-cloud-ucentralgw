//
// Created by stephane bourque on 2022-05-25.
//

#pragma once

#include "framework/MicroService.h"

namespace OpenWifi {

	inline std::vector<unsigned char> Base64Decode(const std::string &F) {
		std::istringstream ifs(F);
		Poco::Base64Decoder b64in(ifs);
		std::ostringstream ofs;
		Poco::StreamCopier::copyStream(b64in, ofs);
		std::vector<unsigned char> r;
		for(const auto &c:ofs.str()) {
			r.push_back((unsigned char)c);
		}
		return r;
	}

	inline bool ParseWifiScan(Poco::JSON::Object::Ptr &Obj, std::stringstream & Result) {
		if(Obj->has("status")) {
			auto Status = Obj->get("status").extract<Poco::JSON::Object::Ptr>();
			if(Status->has("scan") && Status->isArray("scan")) {
				auto ScanArray = Status->getArray("scan");
				Poco::JSON::Array	ParsedScan;
				for(auto &scan_entry:*ScanArray) {
					auto Entry = scan_entry.extract<Poco::JSON::Object::Ptr>();
					if(Entry->has("ies") && Entry->isArray("ies")) {
						auto ies = Entry->getArray("ies");
						Poco::JSON::Array	new_ies;
						for(auto &ie:*ies) {
							auto ie_obj = ie.extract<Poco::JSON::Object::Ptr>();
							auto ie_type = (uint64_t) ie_obj->get("type");
							auto ie_data = ie_obj->get("data").toString();
							auto data = Base64Decode(ie_data);
							if(ie_type==7) {
								Poco::JSON::Object	new_ie;
								std::string CountryName;
								CountryName += data[0];
								CountryName += data[1];
								new_ie.set("country", CountryName);
								new_ies.add(new_ie);
							} else {
								new_ies.add(ie_obj);
							}
						}
						Entry->set("ies",new_ies);
						ParsedScan.add(Entry);
					} else {
						ParsedScan.add(scan_entry);
					}
				}
				Status->set("scan",ParsedScan);
				Obj->set("status", Status);
			}
		}
		Obj->stringify(Result);
		return false;
	}

} // namespace OpenWifi
