//
// Created by stephane bourque on 2021-03-01.
//

#ifndef UCENTRAL_USTORAGESERVICE_H
#define UCENTRAL_USTORAGESERVICE_H

#include "SubSystemServer.h"

#include "Poco/Data/Session.h"
#include "Poco/Data/SessionPool.h"
#include "Poco/Data/SQLite/SQLite.h"
#include "Poco/Data/SQLite/Connector.h"
#include "Poco/Data/PostgreSQL/Connector.h"
#include "Poco/Data/PostgreSQL/SessionHandle.h"
#include "Poco/Data/MySQL/Connector.h"
#include "Poco/Data/MySQL/SessionHandle.h"
#include "Poco/Data/ODBC/Connector.h"
#include "Poco/Data/ODBC/ODBC.h"

#include "RESTAPI_Objects.h"

namespace uCentral::Storage {

    int Start();
    void Stop();

    bool AddStatisticsData(std::string &SerialNumber, uint64_t CfgUUID, std::string &NewStats);
    bool GetStatisticsData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany, std::vector<uCentralStatistics> &Stats);
    bool DeleteStatisticsData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate );

    bool AddHealthCheckData(std::string &SerialNumber,const uCentralHealthcheck & Check);
    bool GetHealthCheckData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                            std::vector<uCentralHealthcheck> &Checks);
    bool DeleteHealthCheckData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate );

    bool AddLog(std::string & SerialNumber, const uCentralDeviceLog & Log );
    bool AddLog(std::string & SerialNumber, const std::string &Log );

    bool GetLogData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany, std::vector<uCentralDeviceLog> &Stats);
    bool DeleteLogData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate);

    bool UpdateDeviceConfiguration(std::string &SerialNumber, std::string &Configuration);
    bool CreateDevice(uCentralDevice &);
    bool CreateDefaultDevice(const std::string & SerialNumber, const std::string & Capabilities);
    bool GetDevice(std::string &SerialNumber, uCentralDevice &);
    bool DeviceExists(const std::string & SerialNumber);
    bool GetDevices(uint64_t From, uint64_t HowMany, std::vector<uCentralDevice> &Devices);
    bool DeleteDevice(std::string &SerialNumber);
    bool UpdateDevice(uCentralDevice &);
    bool ExistingConfiguration(std::string &SerialNumber, uint64_t CurrentConfig, std::string &NewConfig, uint64_t &);
    bool UpdateDeviceCapabilities(std::string &SerialNumber, std::string &State);
    bool GetDeviceCapabilities(std::string &SerialNumber, uCentralCapabilities &);
    bool DeleteDeviceCapabilities(std::string & SerialNumber);

    bool CreateDefaultConfiguration(std::string & name, const uCentralDefaultConfiguration & DefConfig);
    bool DeleteDefaultConfiguration(const std::string & name);
    bool UpdateDefaultConfiguration(std::string & name, const uCentralDefaultConfiguration & DefConfig);
    bool GetDefaultConfiguration(std::string &name, uCentralDefaultConfiguration & DefConfig);
    bool GetDefaultConfigurations(uint64_t From, uint64_t HowMany, std::vector<uCentralDefaultConfiguration> &Devices);

    std::string SerialToMAC(const std::string & Serial);


    class Service : public SubSystemServer {

    public:
        Service() noexcept;

        friend int uCentral::Storage::Start();
        friend void uCentral::Storage::Stop();

        static Service *instance() {
            if (instance_ == nullptr) {
                instance_ = new Service;
            }
            return instance_;
        }

        friend bool AddStatisticsData(std::string &SerialNumber, uint64_t CfgUUID, std::string &NewStats);
        friend bool GetStatisticsData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                               std::vector<uCentralStatistics> &Stats);
        friend bool DeleteStatisticsData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate );

        friend bool AddHealthCheckData(std::string &SerialNumber, const uCentralHealthcheck & Check);
        friend bool GetHealthCheckData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                                      std::vector<uCentralHealthcheck> &Checks);
        friend bool DeleteHealthCheckData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate );

        friend bool CreateDefaultDevice(const std::string & SerialNumber, const std::string & Capabilities);
        friend bool DeviceExists(const std::string & SerialNumber);

        friend bool UpdateDeviceConfiguration(std::string &SerialNumber, std::string &Configuration);
        friend bool CreateDevice(uCentralDevice &);
        friend bool GetDevice(std::string &SerialNumber, uCentralDevice &);
        friend bool GetDevices(uint64_t From, uint64_t HowMany, std::vector<uCentralDevice> &Devices);
        friend bool DeleteDevice(std::string &SerialNumber);
        friend bool UpdateDevice(uCentralDevice &);
        friend bool ExistingConfiguration(std::string &SerialNumber, uint64_t CurrentConfig, std::string &NewConfig, uint64_t &);
        friend bool UpdateDeviceCapabilities(std::string &SerialNumber, std::string &State);
        friend bool GetDeviceCapabilities(std::string &SerialNumber, uCentralCapabilities &);
        friend bool DeleteDeviceCapabilities(std::string & SerialNumber);
        friend bool GetLogData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                               std::vector<uCentralDeviceLog> &Stats);
        friend bool DeleteLogData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate);
        friend bool AddLog(std::string & SerialNumber, const std::string & Log);
        friend bool AddLog(std::string & SerialNumber, const uCentralDeviceLog & Log );

        friend bool CreateDefaultConfiguration(std::string & name, const uCentralDefaultConfiguration & DefConfig);
        friend bool DeleteDefaultConfiguration(const std::string & name);
        friend bool UpdateDefaultConfiguration(std::string & name, const uCentralDefaultConfiguration & DefConfig);
        friend bool GetDefaultConfiguration(std::string &name, uCentralDefaultConfiguration & DefConfig);
        friend bool GetDefaultConfigurations(uint64_t From, uint64_t HowMany, std::vector<uCentralDefaultConfiguration> &Devices);

    private:

        bool AddLog(std::string & SerialNumber, const uCentralDeviceLog & Log );
        bool AddLog(std::string & SerialNumber, const std::string & Log );
        bool AddStatisticsData(std::string &SerialNumber, uint64_t CfgUUID, std::string &NewStats);
        bool GetStatisticsData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                               std::vector<uCentralStatistics> &Stats);
        bool DeleteStatisticsData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate );

        bool AddHealthCheckData(std::string &SerialNumber, const uCentralHealthcheck & Check);
        bool GetHealthCheckData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                                       std::vector<uCentralHealthcheck> &Checks);
        bool DeleteHealthCheckData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate );

        bool UpdateDeviceConfiguration(std::string &SerialNumber, std::string &Configuration);
        
        bool CreateDevice(uCentralDevice &);
        bool CreateDefaultDevice(const std::string & SerialNumber, const std::string & Capabilities);
        
        bool GetDevice(std::string &SerialNumber, uCentralDevice &);
        bool GetDevices(uint64_t From, uint64_t Howmany, std::vector<uCentralDevice> &Devices);
        bool DeleteDevice(std::string &SerialNumber);
        bool UpdateDevice(uCentralDevice &);
        bool DeviceExists(const std::string & SerialNumber);

        bool ExistingConfiguration(std::string &SerialNumber, uint64_t CurrentConfig, std::string &NewConfig, uint64_t &);

        bool UpdateDeviceCapabilities(std::string &SerialNumber, std::string &State);
        bool GetDeviceCapabilities(std::string &SerialNumber, uCentralCapabilities &);
        bool DeleteDeviceCapabilities(std::string & SerialNumber);

        bool GetLogData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate, uint64_t Offset, uint64_t HowMany,
                        std::vector<uCentralDeviceLog> &Stats);
        bool DeleteLogData(std::string &SerialNumber, uint64_t FromDate, uint64_t ToDate);

        bool CreateDefaultConfiguration(std::string & name, const uCentralDefaultConfiguration & DefConfig);
        bool DeleteDefaultConfiguration(const std::string & name);
        bool UpdateDefaultConfiguration(std::string & name, const uCentralDefaultConfiguration & DefConfig);
        bool GetDefaultConfiguration(std::string &name, uCentralDefaultConfiguration & DefConfig);
        bool GetDefaultConfigurations(uint64_t From, uint64_t HowMany, std::vector<uCentralDefaultConfiguration> &Devices);
        bool FindDefaultConfigurationForModel(const std::string & Model, uCentralDefaultConfiguration & DefConfig );

        int Start() override;
        void Stop() override;
        int Setup_MySQL();
        int Setup_SQLite();
        int Setup_PostgreSQL();
        int Setup_ODBC();

        std::mutex          mutex_;
        static Service      *instance_;
        std::shared_ptr<Poco::Data::SessionPool>            Pool_;
        std::shared_ptr<Poco::Data::SQLite::Connector>      SQLiteConn_;
        std::shared_ptr<Poco::Data::PostgreSQL::Connector>  PostgresConn_;
        std::shared_ptr<Poco::Data::MySQL::Connector>       MySQLConn_;
        std::shared_ptr<Poco::Data::ODBC::Connector>        ODBCConn_;
    };

};  // namespace

#endif //UCENTRAL_USTORAGESERVICE_H
