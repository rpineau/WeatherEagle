//
//  WeatherEagle.h
//  CWeatherEagle
//
//  Created by Rodolphe Pineau on 2023-05-14
//  WeatherEagle X2 plugin

#ifndef __WeatherEagle__
#define __WeatherEagle__
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>

#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif

#ifdef SB_WIN_BUILD
#include <time.h>
#endif


#ifndef SB_WIN_BUILD
#include <curl/curl.h>
#else
#include "win_includes/curl.h"
#endif

#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <cmath>
#include <future>
#include <mutex>


#include "../../licensedinterfaces/sberrorx.h"

#include "json.hpp"
using json = nlohmann::json;

#define PLUGIN_VERSION      1.0

// #define PLUGIN_DEBUG 3

#define SERIAL_BUFFER_SIZE 256
#define MAX_TIMEOUT 500
#define MAX_READ_WAIT_TIMEOUT 25
#define NB_RX_WAIT 10

#define MAX_CONNECT_TIMEOUT 5

// error codes
enum WeatherEagleErrors {PLUGIN_OK=0, NOT_CONNECTED, CANT_CONNECT, BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT, PARSE_FAILED};

enum WeatherEagleWindUnits {KPH=0, MPS, MPH};

class CWeatherEagle
{
public:
    CWeatherEagle();
    ~CWeatherEagle();

    int         Connect();
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }
    void        getFirmware(std::string &sFirmware);

    int         getWindSpeedUnit(int &nUnit);
    double      getAmbientTemp();

    std::mutex  m_DevAccessMutex;
    int         getData();

    static size_t writeFunction(void* ptr, size_t size, size_t nmemb, void* data);

    void getIpAddress(std::string &IpAddress);
    void setIpAddress(std::string IpAddress);

    void getTcpPort(int &nTcpPort);
    void setTcpPort(int nTcpPort);

    double getAmbianTemp();
    double getHumidity();
    double getDewPointTemp();
    double getBarometricPressure();
    double getExteSensorTemp(int nIndex);

#ifdef PLUGIN_DEBUG
    void  log(const std::string sLogLine);
#endif

protected:

    bool            m_bIsConnected;
    std::string     m_sFirmware;
    std::string     m_sModel;
    double          m_dFirmwareVersion;

    CURL            *m_Curl;
    std::string     m_sBaseUrl;

    std::string     m_sIpAddress;
    int             m_nTcpPort;

    bool                m_ThreadsAreRunning;
    std::promise<void> *m_exitSignal;
    std::future<void>   m_futureObj;
    std::thread         m_th;

    // WeatherEagle variables
    std::atomic<double> m_dTemp;
    std::atomic<double> m_dPercentHumdity;
    std::atomic<double> m_dDewPointTemp;
    std::atomic<double> m_dBarometricPressure;
    std::atomic<double> m_dExtTemp5;
    std::atomic<double> m_dExtTemp6;
    std::atomic<double> m_dExtTemp7;

    bool            m_bSafe;

    int             eagleEccoConnect();

    int             doGET(std::string sCmd, std::string &sResp);
    std::string     cleanupResponse(const std::string InString, char cSeparator);
    int             getModelName();
    int             getFirmwareVersion();

    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);
    std::string     findField(std::vector<std::string> &svFields, const std::string& token);


#ifdef PLUGIN_DEBUG
    // timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sLogfilePath;
    std::string m_sPlatform;
#endif

};

#endif
