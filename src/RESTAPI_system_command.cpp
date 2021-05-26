//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//
#include "RESTAPI_system_command.h"

#include "Poco/Exception.h"
#include "Poco/JSON/Parser.h"

#include "uCentral.h"
#include "RESTAPI_protocol.h"

/*

    SystemCommandDetails:
      type: object
      properties:
        command:
          type: string
          enum:
            - setloglevel
            - getloglevel
            - stats
        parameters:
          type: array
          items:
            properties:
              name:
                type: string
              value:
                type: string

    SystemCommandResults:
      type: object
      properties:
        command:
          type: string
        result:
          type: integer
        resultTxt:
          type: array
          items:
            type: string

 */
void RESTAPI_system_command::handleRequest(Poco::Net::HTTPServerRequest& Request, Poco::Net::HTTPServerResponse& Response) {

	if(!ContinueProcessing(Request,Response))
		return;

	if(!IsAuthorized(Request,Response))
		return;

	try {

		if(Request.getMethod()==Poco::Net::HTTPRequest::HTTP_POST) {
			Poco::JSON::Parser parser;
			auto Obj = parser.parse(Request.stream()).extract<Poco::JSON::Object::Ptr>();
			Poco::DynamicStruct ds = *Obj;

			if(ds.contains(uCentral::RESTAPI::Protocol::COMMAND)) {
				auto Command = ds[uCentral::RESTAPI::Protocol::COMMAND].toString();
				if(Command==uCentral::RESTAPI::Protocol::SETLOGLEVEL) {
					if(ds.contains(uCentral::RESTAPI::Protocol::PARAMETERS)) {
						auto ParametersBlock = ds[uCentral::RESTAPI::Protocol::PARAMETERS];
						if(ParametersBlock.isArray())
						{
							for(const auto &i : ParametersBlock)
							{
								if(i.isStruct()) {
									auto O = i.toString();
									Poco::JSON::Parser	pp;

									auto TLV = pp.parse(i).extract<Poco::JSON::Object::Ptr>();
									Poco::DynamicStruct Vars = *TLV;
									if (Vars.contains(uCentral::RESTAPI::Protocol::NAME) && Vars.contains(uCentral::RESTAPI::Protocol::VALUE)) {
										auto Name = Vars[uCentral::RESTAPI::Protocol::NAME].toString();
										auto Value = Vars[uCentral::RESTAPI::Protocol::VALUE].toString();
										uCentral::Daemon::SetSubsystemLogLevel(Name,Value);
										Logger_.information(Poco::format("Setting log level for %s at %s",Name,Value));
									}
								}
							}
							OK(Request, Response);
							return;
						}
					}
				} else if (Command==uCentral::RESTAPI::Protocol::GETLOGLEVEL) {
					Logger_.information("GETLOGLEVEL");
				} else if (Command=="stats") {

				}
			}
		}
	} catch ( const Poco::Exception & E ) {
		Logger_.log(E);
	}

	BadRequest(Request, Response);
}
