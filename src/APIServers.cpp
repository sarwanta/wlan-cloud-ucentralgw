//
// Created by stephane bourque on 2021-10-23.
//

#include "framework/MicroService.h"

#include "RESTAPI/RESTAPI_blacklist.h"
#include "RESTAPI/RESTAPI_blacklist_list.h"
#include "RESTAPI/RESTAPI_command.h"
#include "RESTAPI/RESTAPI_commands.h"
#include "RESTAPI/RESTAPI_default_configuration.h"
#include "RESTAPI/RESTAPI_default_configurations.h"
#include "RESTAPI/RESTAPI_deviceDashboardHandler.h"
#include "RESTAPI/RESTAPI_device_commandHandler.h"
#include "RESTAPI/RESTAPI_device_handler.h"
#include "RESTAPI/RESTAPI_devices_handler.h"
#include "RESTAPI/RESTAPI_file.h"
#include "RESTAPI/RESTAPI_ouis.h"

#include "RESTAPI/RESTAPI_capabilities_handler.h"
#include "RESTAPI/RESTAPI_TelemetryWebSocket.h"
#include "RESTAPI/RESTAPI_webSocketServer.h"

namespace OpenWifi {

    Poco::Net::HTTPRequestHandler * RESTAPI_external_server(const char *Path, RESTAPIHandler::BindingMap &Bindings,
                                                            Poco::Logger & L, RESTAPI_GenericServer & S) {
    	return RESTAPI_Router<
				RESTAPI_devices_handler,
				RESTAPI_device_handler,
				RESTAPI_device_commandHandler,
				RESTAPI_default_configurations,
				RESTAPI_default_configuration,
				RESTAPI_command,
				RESTAPI_commands,
				RESTAPI_ouis,
				RESTAPI_file,
				RESTAPI_system_command,
				RESTAPI_deviceDashboardHandler,
				RESTAPI_webSocketServer,
				RESTAPI_blacklist,
				RESTAPI_blacklist_list,
				RESTAPI_capabilities_handler,
				RESTAPI_TelemetryWebSocket>(Path,Bindings,L, S);
    }

    Poco::Net::HTTPRequestHandler * RESTAPI_internal_server(const char *Path, RESTAPIHandler::BindingMap &Bindings,
                                                            Poco::Logger & L, RESTAPI_GenericServer & S) {
    	return RESTAPI_Router_I<
				RESTAPI_devices_handler,
				RESTAPI_device_handler,
				RESTAPI_device_commandHandler,
				RESTAPI_default_configurations,
				RESTAPI_default_configuration,
				RESTAPI_command,
				RESTAPI_commands,
				RESTAPI_ouis,
				RESTAPI_file, RESTAPI_blacklist,
				RESTAPI_blacklist_list>(Path,Bindings,L, S);
	}
}