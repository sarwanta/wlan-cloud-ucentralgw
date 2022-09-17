//
// Created by stephane bourque on 2022-05-18.
//

#include "RADIUS_proxy_server.h"
#include "DeviceRegistry.h"
#include "RADIUS_helpers.h"

namespace OpenWifi {

	const int SMALLEST_RADIUS_PACKET = 20+19+4;
	const int DEFAULT_RADIUS_AUTHENTICATION_PORT = 1812;
	const int DEFAULT_RADIUS_ACCOUNTING_PORT = 1813;
	const int DEFAULT_RADIUS_CoA_PORT = 3799;

	int RADIUS_proxy_server::Start() {

		ConfigFilename_ = MicroService::instance().DataDir()+"/radius_pool_config.json";
		Poco::File	Config(ConfigFilename_);

		enabled_ = MicroService::instance().ConfigGetBool("radius.proxy.enable",false);
		if(!enabled_ && !Config.exists())
			return 0;

		enabled_ = true;

		Poco::Net::SocketAddress	AuthSockAddrV4(Poco::Net::AddressFamily::IPv4,
									   MicroService::instance().ConfigGetInt("radius.proxy.authentication.port",DEFAULT_RADIUS_AUTHENTICATION_PORT));
		AuthenticationSocketV4_ = std::make_unique<Poco::Net::DatagramSocket>(AuthSockAddrV4,true);
		Poco::Net::SocketAddress	AuthSockAddrV6(Poco::Net::AddressFamily::IPv6,
											  MicroService::instance().ConfigGetInt("radius.proxy.authentication.port",DEFAULT_RADIUS_AUTHENTICATION_PORT));
		AuthenticationSocketV6_ = std::make_unique<Poco::Net::DatagramSocket>(AuthSockAddrV6,true);

		Poco::Net::SocketAddress	AcctSockAddrV4(Poco::Net::AddressFamily::IPv4,
									   MicroService::instance().ConfigGetInt("radius.proxy.accounting.port",DEFAULT_RADIUS_ACCOUNTING_PORT));
		AccountingSocketV4_ = std::make_unique<Poco::Net::DatagramSocket>(AcctSockAddrV4,true);
		Poco::Net::SocketAddress	AcctSockAddrV6(Poco::Net::AddressFamily::IPv6,
											  MicroService::instance().ConfigGetInt("radius.proxy.accounting.port",DEFAULT_RADIUS_ACCOUNTING_PORT));
		AccountingSocketV6_ = std::make_unique<Poco::Net::DatagramSocket>(AcctSockAddrV6,true);

		Poco::Net::SocketAddress	CoASockAddrV4(Poco::Net::AddressFamily::IPv4,
												MicroService::instance().ConfigGetInt("radius.proxy.coa.port",DEFAULT_RADIUS_CoA_PORT));
		CoASocketV4_ = std::make_unique<Poco::Net::DatagramSocket>(CoASockAddrV4,true);
		Poco::Net::SocketAddress	CoASockAddrV6(Poco::Net::AddressFamily::IPv6,
												MicroService::instance().ConfigGetInt("radius.proxy.coa.port",DEFAULT_RADIUS_CoA_PORT));
		CoASocketV6_ = std::make_unique<Poco::Net::DatagramSocket>(CoASockAddrV6,true);

		RadiusReactor_.addEventHandler(*AuthenticationSocketV4_,Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
														   *this, &RADIUS_proxy_server::OnAuthenticationSocketReadable));
		RadiusReactor_.addEventHandler(*AuthenticationSocketV6_,Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
																			 *this, &RADIUS_proxy_server::OnAuthenticationSocketReadable));

		RadiusReactor_.addEventHandler(*AccountingSocketV4_,Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
																		  *this, &RADIUS_proxy_server::OnAccountingSocketReadable));
		RadiusReactor_.addEventHandler(*AccountingSocketV6_,Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
																   *this, &RADIUS_proxy_server::OnAccountingSocketReadable));


		RadiusReactor_.addEventHandler(*CoASocketV4_,Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
																			 *this, &RADIUS_proxy_server::OnCoASocketReadable));
		RadiusReactor_.addEventHandler(*CoASocketV6_,Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
																	 *this, &RADIUS_proxy_server::OnCoASocketReadable));

		ParseConfig();

		//	start RADSEC servers...
		StartRADSECServers();
		RadiusReactorThread_.start(RadiusReactor_);

		Utils::SetThreadName(RadiusReactorThread_,"rad:reactor");

		running_ = true;

		return 0;
	}

	void RADIUS_proxy_server::Stop() {
		if(enabled_ && running_) {
			RadiusReactor_.removeEventHandler(
				*AuthenticationSocketV4_,
				Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
					*this, &RADIUS_proxy_server::OnAuthenticationSocketReadable));
			RadiusReactor_.removeEventHandler(
				*AuthenticationSocketV6_,
				Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
					*this, &RADIUS_proxy_server::OnAuthenticationSocketReadable));

			RadiusReactor_.removeEventHandler(
				*AccountingSocketV4_,
				Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
					*this, &RADIUS_proxy_server::OnAccountingSocketReadable));
			RadiusReactor_.removeEventHandler(
				*AccountingSocketV6_,
				Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
					*this, &RADIUS_proxy_server::OnAccountingSocketReadable));

			RadiusReactor_.removeEventHandler(
				*CoASocketV4_,
				Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
					*this, &RADIUS_proxy_server::OnAccountingSocketReadable));
			RadiusReactor_.removeEventHandler(
				*CoASocketV6_,
				Poco::NObserver<RADIUS_proxy_server, Poco::Net::ReadableNotification>(
					*this, &RADIUS_proxy_server::OnAccountingSocketReadable));

			for(auto &[_,radsec_server]:RADSECservers_)
				radsec_server->Stop();

			RadiusReactor_.stop();
			RadiusReactorThread_.join();
			enabled_=false;
			running_=false;
		}
	}

	void RADIUS_proxy_server::StartRADSECServers() {
		for(const auto &pool:PoolList_.pools) {
			for(const auto &entry:pool.authConfig.servers) {
				if(entry.radsec) {
					StartRADSECServer(entry);
				}
			}
		}
	}

	void RADIUS_proxy_server::StartRADSECServer(const GWObjects::RadiusProxyServerEntry &E) {
		RADSECservers_[ Poco::Net::SocketAddress(E.ip,0) ] = std::make_unique<RADSECserver>(RadiusReactor_,E);
	}

	void RADIUS_proxy_server::OnAccountingSocketReadable(const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf) {
		Poco::Net::SocketAddress	Sender;
		RADIUS::RadiusPacket		P;

		auto ReceiveSize = pNf->socket().impl()->receiveBytes(P.Buffer(),P.BufferLen());
		if(ReceiveSize<SMALLEST_RADIUS_PACKET) {
			Logger().warning("Accounting: bad packet received.");
			return;
		}
		P.Evaluate(ReceiveSize);
		auto SerialNumber = P.ExtractSerialNumberFromProxyState();
		if(SerialNumber.empty()) {
			Logger().warning("Accounting: missing serial number.");
			return;
		}
		auto CallingStationID = P.ExtractCallingStationID();
		auto CalledStationID = P.ExtractCalledStationID();

		Logger().information(fmt::format("Accounting Packet received for {}, CalledStationID: {}, CallingStationID:{}",SerialNumber, CalledStationID, CallingStationID));
		DeviceRegistry()->SendRadiusAccountingData(SerialNumber,P.Buffer(),P.Size());
	}

	void RADIUS_proxy_server::OnAuthenticationSocketReadable(const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf) {
		Poco::Net::SocketAddress	Sender;
		RADIUS::RadiusPacket		P;

		auto ReceiveSize = pNf->socket().impl()->receiveBytes(P.Buffer(),P.BufferLen());
		if(ReceiveSize<SMALLEST_RADIUS_PACKET) {
			Logger().warning("Authentication: bad packet received.");
			return;
		}
		P.Evaluate(ReceiveSize);
		auto SerialNumber = P.ExtractSerialNumberFromProxyState();
		if(SerialNumber.empty()) {
			Logger().warning("Authentication: missing serial number.");
			return;
		}
		auto CallingStationID = P.ExtractCallingStationID();
		auto CalledStationID = P.ExtractCalledStationID();

		Logger().information(fmt::format("Authentication Packet received for {}, CalledStationID: {}, CallingStationID:{}",SerialNumber, CalledStationID, CallingStationID));
		DeviceRegistry()->SendRadiusAuthenticationData(SerialNumber,P.Buffer(),P.Size());
	}

	void RADIUS_proxy_server::OnCoASocketReadable(const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf) {
		Poco::Net::SocketAddress	Sender;
		RADIUS::RadiusPacket		P;

		auto ReceiveSize = pNf.get()->socket().impl()->receiveBytes(P.Buffer(),P.BufferLen());
		if(ReceiveSize<SMALLEST_RADIUS_PACKET) {
			Logger().warning("CoA/DM: bad packet received.");
			return;
		}
		P.Evaluate(ReceiveSize);
		auto SerialNumber = P.ExtractSerialNumberTIP();
		if(SerialNumber.empty()) {
			Logger().warning("CoA/DM: missing serial number.");
			return;
		}
		auto CallingStationID = P.ExtractCallingStationID();
		auto CalledStationID = P.ExtractCalledStationID();

		Logger().information(fmt::format("CoA Packet received for {}, CalledStationID: {}, CallingStationID:{}",SerialNumber, CalledStationID, CallingStationID));
		DeviceRegistry()->SendRadiusCoAData(SerialNumber,P.Buffer(),P.Size());
	}

	void RADIUS_proxy_server::SendAccountingData(const std::string &serialNumber, const char *buffer, std::size_t size) {
		RADIUS::RadiusPacket	P((unsigned char *)buffer,size);
		auto Destination = P.ExtractProxyStateDestination();
		auto CallingStationID = P.ExtractCallingStationID();
		auto CalledStationID = P.ExtractCalledStationID();
		Poco::Net::SocketAddress	Dst(Destination);

		std::lock_guard	G(Mutex_);
		bool UseRADSEC = false;
		auto FinalDestination = Route(radius_type::acct, Dst, P, UseRADSEC);
		if(UseRADSEC) {
			Poco::Net::SocketAddress	RSP(FinalDestination.host(),0);
			auto DestinationServer = RADSECservers_.find(RSP);
			if(DestinationServer!=end(RADSECservers_)) {
				DestinationServer->second->SendData(serialNumber, (const unsigned char *)buffer, size);
			}
		} else {
			auto AllSent =
				SendData(Dst.family() == Poco::Net::SocketAddress::IPv4 ? *AccountingSocketV4_
																		: *AccountingSocketV6_,
						 (const unsigned char *)buffer, size, FinalDestination);
			if (!AllSent)
				Logger().error(fmt::format("{}: Could not send Accounting packet packet to {}.",
										   serialNumber, Destination));
			else
				Logger().information(fmt::format(
					"{}: Sending Accounting Packet to {}, CalledStationID: {}, CallingStationID:{}",
					serialNumber, FinalDestination.toString(), CalledStationID, CallingStationID));
		}
	}

	bool RADIUS_proxy_server::SendData( Poco::Net::DatagramSocket & Sock, const unsigned char *buf , std::size_t size, const Poco::Net::SocketAddress &S) {
		return Sock.sendTo(buf, size, S)==(int)size;
	}

	void RADIUS_proxy_server::SendAuthenticationData(const std::string &serialNumber, const char *buffer, std::size_t size) {
		RADIUS::RadiusPacket	P((unsigned char *)buffer,size);
		auto Destination = P.ExtractProxyStateDestination();
		auto CallingStationID = P.ExtractCallingStationID();
		auto CalledStationID = P.ExtractCalledStationID();
		Poco::Net::SocketAddress	Dst(Destination);

		std::lock_guard	G(Mutex_);
		bool UseRADSEC = false;
		auto FinalDestination = Route(radius_type::auth, Dst, P, UseRADSEC);
		if(UseRADSEC) {
			Poco::Net::SocketAddress	RSP(FinalDestination.host(),0);
			auto DestinationServer = RADSECservers_.find(RSP);
			if(DestinationServer!=end(RADSECservers_)) {
				DestinationServer->second->SendData(serialNumber, (const unsigned char *)buffer, size);
			}
		} else {
			auto AllSent =
				SendData(Dst.family() == Poco::Net::SocketAddress::IPv4 ? *AuthenticationSocketV4_
																		: *AuthenticationSocketV6_,
						 (const unsigned char *)buffer, size, FinalDestination);
			if (!AllSent)
				Logger().error(fmt::format("{}: Could not send Authentication packet packet to {}.",
										   serialNumber, Destination));
			else
				Logger().information(fmt::format("{}: Sending Authentication Packet to {}, CalledStationID: {}, CallingStationID:{}",
												 serialNumber, FinalDestination.toString(),
												 CalledStationID, CallingStationID));
		}
	}

	void RADIUS_proxy_server::SendCoAData(const std::string &serialNumber, const char *buffer, std::size_t size) {
		RADIUS::RadiusPacket	P((unsigned char *)buffer,size);
		auto Destination = P.ExtractProxyStateDestination();

		if(Destination.empty()) {
			Destination = "0.0.0.0:0";
		}

		Poco::Net::SocketAddress	Dst(Destination);
		std::lock_guard	G(Mutex_);
		bool UseRADSEC = false;
		auto FinalDestination = Route(radius_type::coa, Dst, P, UseRADSEC);
		if(UseRADSEC) {
			Poco::Net::SocketAddress	RSP(FinalDestination.host(),0);
			auto DestinationServer = RADSECservers_.find(RSP);
			if(DestinationServer!=end(RADSECservers_)) {
				DestinationServer->second->SendData(serialNumber, (const unsigned char *)buffer, size);
			}
		} else {
			auto AllSent = SendData(Dst.family() == Poco::Net::SocketAddress::IPv4 ? *CoASocketV4_
																				   : *CoASocketV6_,
									(const unsigned char *)buffer, size, FinalDestination);
			if (!AllSent)
				Logger().error(fmt::format("{}: Could not send CoA packet packet to {}.",
										   serialNumber, Destination));
			else
				Logger().information(fmt::format("{}: Sending CoA Packet to {}", serialNumber,
												 FinalDestination.toString()));
		}
	}

	void RADIUS_proxy_server::ParseServerList(const GWObjects::RadiusProxyServerConfig & Config, std::vector<Destination> &V4, std::vector<Destination> &V6, bool setAsDefault) {
		uint64_t TotalV4=0, TotalV6=0;

		for(const auto &server:Config.servers) {
			Poco::Net::IPAddress a;
			if(!Poco::Net::IPAddress::tryParse(server.ip,a)) {
				Logger().error(fmt::format("RADIUS-PARSE Config: server address {} is nto a valid address in v4 or v6. Entry skipped.",server.ip));
				continue;
			}
			auto S = Poco::Net::SocketAddress(fmt::format("{}:{}",server.ip,server.port));
			Destination	D{
				.Addr = S,
				.state = 0,
				.step = 0,
				.weight = server.weight,
				.available = true,
				.strategy = Config.strategy,
				.monitor = Config. monitor,
				.monitorMethod = Config.monitorMethod,
				.methodParameters = Config.methodParameters,
				.useAsDefault = setAsDefault,
				.useRADSEC = server.radsec,
				.realms = server.radsecRealms
			};

			if(setAsDefault && D.useRADSEC)
				defaultIsRADSEC_ = true;

			if(S.family()==Poco::Net::IPAddress::IPv4) {
				TotalV4 += server.weight;
				V4.push_back(D);
			} else {
				TotalV6 += server.weight;
				V6.push_back(D);
			}
		}

		for(auto &i:V4) {
			if(TotalV4==0) {
				i.step = 1000;
			} else {
				i.step = 1000 - ((1000 * i.weight) / TotalV4);
			}
		}

		for(auto &i:V6) {
			if(TotalV6==0) {
				i.step = 1000;
			} else {
				i.step = 1000 - ((1000 * i.weight) / TotalV6);
			}
		}
	}

	void RADIUS_proxy_server::ParseConfig() {

		try {
			Poco::File	F(ConfigFilename_);

			std::lock_guard	G(Mutex_);

			if(F.exists()) {
				std::ifstream ifs(ConfigFilename_,std::ios_base::binary);
				Poco::JSON::Parser	P;
				auto RawConfig = P.parse(ifs).extract<Poco::JSON::Object::Ptr>();
				GWObjects::RadiusProxyPoolList	RPC;
				if(RPC.from_json(RawConfig)) {
					ResetConfig();
					PoolList_ = RPC;
					for(const auto &pool:RPC.pools) {
						RadiusPool	NewPool;
						ParseServerList(pool.authConfig, NewPool.AuthV4, NewPool.AuthV6, pool.useByDefault);
						ParseServerList(pool.acctConfig, NewPool.AcctV4, NewPool.AcctV6, pool.useByDefault);
						ParseServerList(pool.coaConfig, NewPool.CoaV4, NewPool.CoaV6, pool.useByDefault);
						Pools_.push_back(NewPool);
					}
				} else {
					Logger().warning(fmt::format("Configuration file '{}' is bad.",ConfigFilename_));
				}
			} else {
				Logger().warning(fmt::format("No configuration file '{}' exists.",ConfigFilename_));
			}
		} catch (const Poco::Exception &E) {
			Logger().log(E);
		} catch (...) {
			Logger().error(fmt::format("Error while parsing configuration file '{}'",ConfigFilename_));
		}
	}

	static bool RealmMatch(const std::string &user_realm, const std::string & realm) {
		if(realm.find_first_of('*') == std::string::npos)
			return user_realm == realm;
		return realm.find(user_realm) != std::string::npos;
	}

	Poco::Net::SocketAddress RADIUS_proxy_server::DefaultRoute(radius_type rtype, const Poco::Net::SocketAddress &RequestedAddress, const RADIUS::RadiusPacket &P,  bool &UseRADSEC) {
		bool IsV4 = RequestedAddress.family()==Poco::Net::SocketAddress::IPv4;

		// find the realm...
		auto UserName = P.UserName();
		if(!UserName.empty()) {
			auto UserTokens = Poco::StringTokenizer(UserName, "@");
			auto UserRealm = ((UserTokens.count() > 1) ? UserTokens[1] : UserName);
			Poco::toLowerInPlace(UserRealm);

			for(const auto &pool:Pools_) {
				for(const auto &server:pool.AuthV4) {
					if(!server.realms.empty()) {
						for(const auto &realm:server.realms) {
							if (RealmMatch(UserRealm,realm)) {
								std::cout << "Realm match..." << std::endl;
								UseRADSEC = true;
								return server.Addr;
							}
						}
					}
				}
			}
		}

		if(defaultIsRADSEC_) {
			UseRADSEC = true;
			return (IsV4 ? Pools_[defaultPoolIndex_].AuthV4[0].Addr : Pools_[defaultPoolIndex_].AuthV6[0].Addr );
		}

		switch(rtype) {
			case radius_type::auth: {
				return ChooseAddress(IsV4 ? Pools_[defaultPoolIndex_].AuthV4
										  : Pools_[defaultPoolIndex_].AuthV6,
									 RequestedAddress);
				}
			case radius_type::acct:
			default: {
				return ChooseAddress(IsV4 ? Pools_[defaultPoolIndex_].AcctV4
										  : Pools_[defaultPoolIndex_].AcctV6,
									 RequestedAddress);
				}
			case radius_type::coa: {
				return ChooseAddress(IsV4 ? Pools_[defaultPoolIndex_].CoaV4
										  : Pools_[defaultPoolIndex_].CoaV6,
									 RequestedAddress);
				}
			}
	}

	Poco::Net::SocketAddress RADIUS_proxy_server::Route([[maybe_unused]] radius_type rtype, const Poco::Net::SocketAddress &RequestedAddress, const RADIUS::RadiusPacket &P, bool &UseRADSEC) {
		std::lock_guard	G(Mutex_);

		if(Pools_.empty()) {
			UseRADSEC = false;
			return RequestedAddress;
		}

		bool IsV4 = RequestedAddress.family()==Poco::Net::SocketAddress::IPv4;
		bool useDefault;
		useDefault = IsV4 ? RequestedAddress.host() == Poco::Net::IPAddress::wildcard(Poco::Net::IPAddress::IPv4) : RequestedAddress.host() == Poco::Net::IPAddress::wildcard(Poco::Net::IPAddress::IPv6) ;

		if(useDefault) {
			return DefaultRoute(rtype, RequestedAddress, P, UseRADSEC);
		}

		auto isAddressInPool = [&](const std::vector<Destination> & D, bool &UseRADSEC) -> bool {
			for(const auto &entry:D)
				if(entry.Addr.host()==RequestedAddress.host()) {
					UseRADSEC = entry.useRADSEC;
					return true;
				}
			return false;
		};

		for(auto &i:Pools_) {
			switch(rtype) {
			case radius_type::coa: {
				if (isAddressInPool((IsV4 ? i.CoaV4 : i.CoaV6), UseRADSEC)) {
					return ChooseAddress(IsV4 ? i.CoaV4 : i.CoaV6, RequestedAddress);
				}
			} break;
			case radius_type::auth: {
				if (isAddressInPool((IsV4 ? i.AuthV4 : i.AuthV6), UseRADSEC)) {
					return ChooseAddress(IsV4 ? i.AuthV4 : i.AuthV6, RequestedAddress);
				}
			} break;
			case radius_type::acct: {
				if (isAddressInPool((IsV4 ? i.AcctV4 : i.AcctV6), UseRADSEC)) {
					return ChooseAddress(IsV4 ? i.AcctV4 : i.AcctV6, RequestedAddress);
				}
			} break;
			}
		}
		return DefaultRoute(rtype, RequestedAddress, P, UseRADSEC);
	}

	Poco::Net::SocketAddress RADIUS_proxy_server::ChooseAddress(std::vector<Destination> &Pool, const Poco::Net::SocketAddress & OriginalAddress) {

		if(Pool.size()==1) {
			return Pool[0].Addr;
		}

		if (Pool[0].strategy == "weighted") {
			bool found = false;
			uint64_t cur_state = std::numeric_limits<uint64_t>::max();
			std::size_t pos = 0, index = 0;
			for (auto &i : Pool) {
				if (!i.available) {
					i.state += i.step;
					continue;
				}
				if (i.state < cur_state) {
					index = pos;
					cur_state = i.state;
					found = true;
				}
				pos++;
			}

			if (!found) {
				return OriginalAddress;
			}

			Pool[index].state += Pool[index].step;
			return Pool[index].Addr;
		} else if (Pool[0].strategy == "round_robin") {
			bool found = false;
			uint64_t cur_state = std::numeric_limits<uint64_t>::max();
			std::size_t pos = 0, index = 0;
			for (auto &i : Pool) {
				if (!i.available) {
					i.state += 1;
					continue;
				}
				if (i.state < cur_state) {
					index = pos;
					cur_state = i.state;
					found = true;
				}
				pos++;
			}

			if (!found) {
				return OriginalAddress;
			}

			Pool[index].state += 1;
			return Pool[index].Addr;
		} else if (Pool[0].strategy == "random") {
			if (Pool.size() > 1) {
				return Pool[std::rand() % Pool.size()].Addr;
			} else {
				return OriginalAddress;
			}
		}
		return OriginalAddress;
	}

	void RADIUS_proxy_server::SetConfig(const GWObjects::RadiusProxyPoolList &C) {
		std::lock_guard	G(Mutex_);
		PoolList_ = C;

		Poco::JSON::Object	Disk;
		C.to_json(Disk);

		std::ofstream ofs(ConfigFilename_, std::ios_base::trunc | std::ios_base::binary );
		Disk.stringify(ofs);
		ofs.close();

		if(!running_) {
			Start();
		}

		ParseConfig();
	}

	void RADIUS_proxy_server::ResetConfig() {
		PoolList_.pools.clear();
		Pools_.clear();
		defaultPoolIndex_=0;
	}

	void RADIUS_proxy_server::DeleteConfig() {
		std::lock_guard	G(Mutex_);

		try {
			Poco::File F(ConfigFilename_);
			if (F.exists())
				F.remove();
		} catch (...) {

		}
		ResetConfig();
		Stop();
	}

	void RADIUS_proxy_server::GetConfig(GWObjects::RadiusProxyPoolList &C) {
		std::lock_guard	G(Mutex_);
		C = PoolList_;
	}

}