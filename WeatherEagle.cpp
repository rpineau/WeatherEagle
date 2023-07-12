//
//  WeatherEagle.cpp
//  CWeatherEagle
//
//  Created by Rodolphe Pineau on 2021-04-13
//  WeatherEagle X2 plugin

#include "WeatherEagle.h"

void threaded_poller(std::future<void> futureObj, CWeatherEagle *WeatherEagleControllerObj)
{
    while (futureObj.wait_for(std::chrono::milliseconds(5000)) == std::future_status::timeout) {
        if(WeatherEagleControllerObj->m_DevAccessMutex.try_lock()) {
            WeatherEagleControllerObj->getData();
            WeatherEagleControllerObj->m_DevAccessMutex.unlock();
        }
        else {
            std::this_thread::yield();
        }
    }
}

CWeatherEagle::CWeatherEagle()
{
    // set some sane values
    m_bIsConnected = false;
    m_ThreadsAreRunning = false;
    m_sIpAddress.clear();
    m_nTcpPort = 0;

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\X2_WeatherEagle.txt";
    m_sPlatform = "Windows";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_WeatherEagle.txt";
    m_sPlatform = "Linux";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/X2_WeatherEagle.txt";
    m_sPlatform = "macOS";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CWeatherEagle] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CWeatherEagle] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

    curl_global_init(CURL_GLOBAL_ALL);
    m_Curl = nullptr;

}

CWeatherEagle::~CWeatherEagle()
{
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [~CWeatherEagle] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(m_bIsConnected) {
        Disconnect();
    }

    curl_global_cleanup();

#ifdef    PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
}

int CWeatherEagle::Connect()
{
    int nErr = SB_OK;
    std::string sDummy;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(m_sIpAddress.empty())
        return ERR_COMMNOLINK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Base url = " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif

    m_Curl = curl_easy_init();

    if(!m_Curl) {
        m_Curl = nullptr;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] CURL init failed" << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    m_bIsConnected = true;
    nErr = eagleEccoConnect();
    if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] eagleEccoConnect failed" << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }
    nErr = getData();
    if (nErr) {
        curl_easy_cleanup(m_Curl);
        m_Curl = nullptr;
        m_bIsConnected = false;
        return nErr;
    }
    if(!m_ThreadsAreRunning) {
        m_exitSignal = new std::promise<void>();
        m_futureObj = m_exitSignal->get_future();
        m_th = std::thread(&threaded_poller, std::move(m_futureObj), this);
        m_ThreadsAreRunning = true;
    }

    return nErr;
}


void CWeatherEagle::Disconnect()
{
    const std::lock_guard<std::mutex> lock(m_DevAccessMutex);

    if(m_bIsConnected) {
        if(m_ThreadsAreRunning) {
#ifdef PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Waiting for threads to exit." << std::endl;
            m_sLogFile.flush();
#endif
            m_exitSignal->set_value();
            m_th.join();
            delete m_exitSignal;
            m_exitSignal = nullptr;
            m_ThreadsAreRunning = false;
        }

        curl_easy_cleanup(m_Curl);
        m_Curl = nullptr;
        m_bIsConnected = false;

#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Disconnect] Disconnected." << std::endl;
        m_sLogFile.flush();
#endif
    }
}

int CWeatherEagle::eagleEccoConnect()
{
    int nErr = PLUGIN_OK;
    json jResp;
    std::string response_string;
    int nTimeout = 0;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [eagleEccoConnect] Called." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = doGET("/connectecco", response_string);
    if (nErr) {
        return nErr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    while(nTimeout < MAX_CONNECT_TIMEOUT) {
        nErr = doGET("/getecco", response_string);
        if(nErr) {
            return nErr;
        }

        // process response_string
        try {
            jResp = json::parse(response_string);
            if(jResp.at("result").get<std::string>() == "OK") {
                if(jResp.at("ecco").get<std::string>() == "Connected") {
                    break;
                }
                else {
                    nTimeout++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            }
            else {
                nTimeout++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
        catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [eagleEccoConnect] json exception : " << e.what() << " - " << e.id << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [eagleEccoConnect] json exception response : " << response_string << std::endl;
            m_sLogFile.flush();
#endif
        }

    }
    if(nTimeout>=MAX_CONNECT_TIMEOUT)
        return COMMAND_FAILED;
    return nErr;
}

void CWeatherEagle::getFirmware(std::string &sFirmware)
{
    sFirmware.assign(m_sFirmware);
}


int CWeatherEagle::getWindSpeedUnit(int &nUnit)
{
    int nErr = PLUGIN_OK;
    nUnit = KPH;
    return nErr;
}


int CWeatherEagle::doGET(std::string sCmd, std::string &sResp)
{
    int nErr = PLUGIN_OK;
    CURLcode res;
    std::string response_string;
    std::string header_string;

    if(!m_bIsConnected)
        return NOT_CONNECTED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Called." << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Doing get on " << sCmd << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] Full get url " << (m_sBaseUrl+sCmd) << std::endl;
    m_sLogFile.flush();
#endif

    res = curl_easy_setopt(m_Curl, CURLOPT_URL, (m_sBaseUrl+sCmd).c_str());
    if(res != CURLE_OK) { // if this fails no need to keep going
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] curl_easy_setopt Error = " << res << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    curl_easy_setopt(m_Curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(m_Curl, CURLOPT_POST, 0L);
    curl_easy_setopt(m_Curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_Curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(m_Curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(m_Curl, CURLOPT_HEADERDATA, &header_string);
    curl_easy_setopt(m_Curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(m_Curl, CURLOPT_CONNECTTIMEOUT, 3); // 3 seconds timeout on connect

    // Perform the request, res will get the return code
    res = curl_easy_perform(m_Curl);
    // Check for errors
    if(res != CURLE_OK) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] curl_easy_perform Error = " << res << std::endl;
        m_sLogFile.flush();
#endif
        if(res == CURLE_COULDNT_CONNECT)
            return ERR_COMMNOLINK;
        return ERR_CMDFAILED;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] response = " << response_string << std::endl;
    m_sLogFile.flush();
#endif

    sResp.assign(cleanupResponse(response_string,'\n'));

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [doGET] sResp = " << sResp << std::endl;
    m_sLogFile.flush();
#endif
    return nErr;
}

size_t CWeatherEagle::writeFunction(void* ptr, size_t size, size_t nmemb, void* data)
{
    ((std::string*)data)->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

std::string CWeatherEagle::cleanupResponse(const std::string InString, char cSeparator)
{
    std::string sSegment;
    std::vector<std::string> svFields;

    if(!InString.size()) {
#ifdef PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [cleanupResponse] InString is empty." << std::endl;
        m_sLogFile.flush();
#endif
        return InString;
    }


    std::stringstream ssTmp(InString);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        if(sSegment.find("<!-") == -1)
            svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        return std::string("");
    }

    sSegment.clear();
    for( std::string s : svFields)
        sSegment.append(trim(s,"\n\r "));
    return sSegment;
}


#pragma mark - Getter / Setter
double CWeatherEagle::getAmbianTemp()
{
    return m_dTemp;
}

double CWeatherEagle::getHumidity()
{
    return m_dPercentHumdity;
}

double CWeatherEagle::getDewPointTemp()
{
    return m_dDewPointTemp;
}

double CWeatherEagle::getBarometricPressure()
{
    return m_dBarometricPressure;
}


int CWeatherEagle::getData()
{
    int nErr = PLUGIN_OK;
    json jResp;
    std::string response_string;
    std::string WeatherEagleError;

    if(!m_bIsConnected || !m_Curl)
        return ERR_COMMNOLINK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] Called." << std::endl;
    m_sLogFile.flush();
#endif
    if(m_sFirmware.empty()) {
        // do http GET request to local server to get firmware info
        nErr = doGET("/getinfo", response_string);
        if(!nErr) {
            // process response_string
            try {
                jResp = json::parse(response_string);
                if(jResp.at("result").get<std::string>() == "OK") {
                    m_sFirmware = jResp.at("firmwareversion").get<std::string>();
                }
                else {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
                    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] getinfo error : " << jResp << std::endl;
                    m_sLogFile.flush();
#endif
                }
            }
            catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] json exception : " << e.what() << " - " << e.id << std::endl;
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] json exception response : " << response_string << std::endl;
                m_sLogFile.flush();
#endif
            }
        }
    }


    // do http GET request to local server environmental data
    nErr = doGET("/getecco", response_string);
    if(nErr) {
        return nErr;
    }

    // process response_string
    try {
        jResp = json::parse(response_string);
        if(jResp.at("result").get<std::string>() == "OK") {
            if(jResp.at("ecco").get<std::string>() == "Connected") {
                m_dTemp = jResp.at("temp").get<double>();
                m_dPercentHumdity = jResp.at("hum").get<double>();
                m_dBarometricPressure = jResp.at("pressure").get<double>();
                m_dDewPointTemp = jResp.at("dew").get<double>();
            }
        }
        else {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] getecco error : " << jResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] json exception response : " << response_string << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dTemp                : " << m_dTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dPercentHumdity      : " << m_dPercentHumdity << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dBarometricPressure  : " << m_dBarometricPressure << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_dDewPointTemp        : " << m_dDewPointTemp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getData] m_sFirmware            : " << m_sFirmware << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}


#pragma mark - Getter / Setter


void CWeatherEagle::getIpAddress(std::string &IpAddress)
{
    IpAddress = m_sIpAddress;
}

void CWeatherEagle::setIpAddress(std::string IpAddress)
{
    m_sIpAddress = IpAddress;
    if(m_nTcpPort!=80 && m_nTcpPort!=443) {
        m_sBaseUrl = "http://"+m_sIpAddress+":"+std::to_string(m_nTcpPort);
    }
    else if (m_nTcpPort==443) {
        m_sBaseUrl = "https://"+m_sIpAddress;
    }
    else {
        m_sBaseUrl = "http://"+m_sIpAddress;
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setIpAddress] New base url : " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif

}

void CWeatherEagle::getTcpPort(int &nTcpPort)
{
    nTcpPort = m_nTcpPort;
}

void CWeatherEagle::setTcpPort(int nTcpPort)
{
    m_nTcpPort = nTcpPort;
    if(m_nTcpPort!=80) {
        m_sBaseUrl = "http://"+m_sIpAddress+":"+std::to_string(m_nTcpPort);
    }
    else {
        m_sBaseUrl = "http://"+m_sIpAddress;
    }
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setTcpPort] New base url : " << m_sBaseUrl << std::endl;
    m_sLogFile.flush();
#endif
}

std::string& CWeatherEagle::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CWeatherEagle::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CWeatherEagle::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}

std::string CWeatherEagle::findField(std::vector<std::string> &svFields, const std::string& token)
{
    for(int i=0; i<svFields.size(); i++){
        if(svFields[i].find(token)!= -1) {
            return svFields[i];
        }
    }
    return std::string();
}


#ifdef PLUGIN_DEBUG
void CWeatherEagle::log(const std::string sLogLine)
{
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [log] " << sLogLine << std::endl;
    m_sLogFile.flush();

}

const std::string CWeatherEagle::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif

