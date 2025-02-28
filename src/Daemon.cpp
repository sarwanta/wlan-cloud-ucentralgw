//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include "Poco/Environment.h"
#include "Poco/Net/SSLManager.h"
#include "Poco/Util/Application.h"
#include "Poco/Util/Option.h"

#include <framework/ConfigurationValidator.h>
#include <framework/UI_WebSocketClientServer.h>
#include <framework/default_device_types.h>

#include "AP_WS_Server.h"
#include "CommandManager.h"
#include "Daemon.h"
#include "FileUploader.h"
#include "FindCountry.h"
#include "OUIServer.h"
#include "RADIUSSessionTracker.h"
#include "RADIUS_proxy_server.h"
#include "RegulatoryInfo.h"
#include "ScriptManager.h"
#include "SerialNumberCache.h"
#include "SignatureMgr.h"
#include "StorageArchiver.h"
#include "StorageService.h"
#include "TelemetryStream.h"
#include "GenericScheduler.h"
#include "UI_GW_WebSocketNotifications.h"
#include "VenueBroadcaster.h"
#include "AP_WS_ConfigAutoUpgrader.h"
#include "rttys/RTTYS_server.h"

namespace OpenWifi {
	class Daemon *Daemon::instance() {
		static Daemon instance(
			vDAEMON_PROPERTIES_FILENAME, vDAEMON_ROOT_ENV_VAR, vDAEMON_CONFIG_ENV_VAR,
			vDAEMON_APP_NAME, vDAEMON_BUS_TIMER,
			SubSystemVec{GenericScheduler(), StorageService(), SerialNumberCache(), ConfigurationValidator(),
						 UI_WebSocketClientServer(), OUIServer(), FindCountryFromIP(),
						 CommandManager(), FileUploader(), StorageArchiver(), TelemetryStream(),
						 RTTYS_server(), RADIUS_proxy_server(), VenueBroadcaster(), ScriptManager(),
						 SignatureManager(), AP_WS_Server(),
						 RegulatoryInfo(),
						 RADIUSSessionTracker(),
						 AP_WS_ConfigAutoUpgrader()
			});
		return &instance;
	}

	void Daemon::PostInitialization([[maybe_unused]] Poco::Util::Application &self) {
		AutoProvisioning_ = config().getBool("openwifi.autoprovisioning", false);
		DeviceTypes_ = DefaultDeviceTypeList;
		WebSocketProcessor_ = std::make_unique<GwWebSocketClient>(logger());
	}

	[[nodiscard]] std::string Daemon::IdentifyDevice(const std::string &Id) const {
		for (const auto &[DeviceType, Type] : DeviceTypes_) {
			if (Id == DeviceType)
				return Type;
		}
		return "AP";
	}

	void DaemonPostInitialization(Poco::Util::Application &self) {
		Daemon()->PostInitialization(self);
		GWWebSocketNotifications::Register();
	}
} // namespace OpenWifi

int main(int argc, char **argv) {
	int ExitCode;
	try {
		Poco::Net::SSLManager::instance().initializeServer(nullptr, nullptr, nullptr);
		auto App = OpenWifi::Daemon::instance();
		ExitCode = App->run(argc, argv);
		Poco::Net::SSLManager::instance().shutdown();
	} catch (Poco::Exception &exc) {
		ExitCode = Poco::Util::Application::EXIT_SOFTWARE;
		std::cout << exc.displayText() << std::endl;
	} catch (std::exception &exc) {
		ExitCode = Poco::Util::Application::EXIT_TEMPFAIL;
		std::cout << exc.what() << std::endl;
	} catch (...) {
		ExitCode = Poco::Util::Application::EXIT_TEMPFAIL;
		std::cout << "Exception on closure" << std::endl;
	}

	std::cout << "Exitcode: " << ExitCode << std::endl;
	return ExitCode;
}

// end of namespace