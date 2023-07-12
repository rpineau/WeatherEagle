
#include "x2weatherstation.h"

X2WeatherStation::X2WeatherStation(const char* pszDisplayName,
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn,
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn,
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;
    m_nPrivateISIndex               = nInstanceIndex;

	m_bLinked = false;
    if (m_pIniUtil) {
        char szIpAddress[128];
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_IP, "127.0.0.1", szIpAddress, 128);
        m_WeatherEagle.setIpAddress(std::string(szIpAddress));
        m_WeatherEagle.setTcpPort(m_pIniUtil->readInt(PARENT_KEY, CHILD_KEY_PORT, 1380));
    }
}

X2WeatherStation::~X2WeatherStation()
{
	//Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();
}

int	X2WeatherStation::queryAbstraction(const char* pszName, void** ppVal)
{
	*ppVal = NULL;

	if (!strcmp(pszName, LinkInterface_Name))
		*ppVal = (LinkInterface*)this;
	else if (!strcmp(pszName, WeatherStationDataInterface_Name))
        *ppVal = dynamic_cast<WeatherStationDataInterface*>(this);
    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);
    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);


	return SB_OK;
}

int X2WeatherStation::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;

    std::stringstream ssTmp;

    if (NULL == ui)
        return ERR_POINTER;
    if ((nErr = ui->loadUserInterface("WeatherEagle.ui", deviceType(), m_nPrivateISIndex))) {
        return nErr;
    }

    if (NULL == (dx = uiutil.X2DX())) {
        return ERR_POINTER;
    }
    X2MutexLocker ml(GetMutex());

    if(m_bLinked) { // we can't change the value for the ip and port if we're connected
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherEagle.getAmbianTemp() << " C";
        dx->setPropertyString("temperature", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::dec << m_WeatherEagle.getHumidity() << " %";
        dx->setPropertyString("humidity", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherEagle.getDewPointTemp() << " C";
        dx->setPropertyString("dewPoint", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherEagle.getBarometricPressure() << " mbar";
        dx->setPropertyString("pressure", "text", ssTmp.str().c_str());
    }

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
    }
    return nErr;
}

void X2WeatherStation::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    std::stringstream ssTmp;
    if (!strcmp(pszEvent, "on_timer") && m_bLinked) {
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherEagle.getAmbianTemp() << " C";
        uiex->setPropertyString("temperature", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::dec << m_WeatherEagle.getHumidity() << " %";
        uiex->setPropertyString("humidity", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherEagle.getDewPointTemp() << " C";
        uiex->setPropertyString("dewPoint", "text", ssTmp.str().c_str());

        std::stringstream().swap(ssTmp);
        ssTmp<< std::fixed << std::setprecision(2) << m_WeatherEagle.getBarometricPressure() << " mbar";
        uiex->setPropertyString("pressure", "text", ssTmp.str().c_str());
    }
}

void X2WeatherStation::driverInfoDetailedInfo(BasicStringInterface& str) const
{
    str = "Eagle Manager X";
}

double X2WeatherStation::driverInfoVersion(void) const
{
	return PLUGIN_VERSION;
}

void X2WeatherStation::deviceInfoNameShort(BasicStringInterface& str) const
{
    str = "Eagle Manager X";
}

void X2WeatherStation::deviceInfoNameLong(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2WeatherStation::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2WeatherStation::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    if(m_bLinked) {
        str = "N/A";
        std::string sFirmware;
        X2MutexLocker ml(GetMutex());
        m_WeatherEagle.getFirmware(sFirmware);
        str = sFirmware.c_str();
    }
    else
        str = "N/A";

}

void X2WeatherStation::deviceInfoModel(BasicStringInterface& str)
{
    deviceInfoNameShort(str);
}

int	X2WeatherStation::establishLink(void)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());
    nErr = m_WeatherEagle.Connect();
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

	return nErr;
}
int	X2WeatherStation::terminateLink(void)
{
    m_WeatherEagle.Disconnect();

	m_bLinked = false;
	return SB_OK;
}


bool X2WeatherStation::isLinked(void) const
{
	return m_bLinked;
}


int X2WeatherStation::weatherStationData(double& dSkyTemp,
                                         double& dAmbTemp,
                                         double& dSenT,
                                         double& dWind,
                                         int& nPercentHumdity,
                                         double& dDewPointTemp,
                                         int& nRainHeaterPercentPower,
                                         int& nRainFlag,
                                         int& nWetFlag,
                                         int& nSecondsSinceGoodData,
                                         double& dVBNow,
                                         double& dBarometricPressure,
                                         WeatherStationDataInterface::x2CloudCond& cloudCondition,
                                         WeatherStationDataInterface::x2WindCond& windCondition,
                                         WeatherStationDataInterface::x2RainCond& rainCondition,
                                         WeatherStationDataInterface::x2DayCond& daylightCondition,
                                         int& nRoofCloseThisCycle //The weather station hardware determined close or not (boltwood hardware says cloudy is not close)
)
{
    int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nSecondsSinceGoodData = 1; // was 900 , aka 15 minutes ?
    dAmbTemp = m_WeatherEagle.getAmbianTemp();
	nPercentHumdity = int(m_WeatherEagle.getHumidity());
	dDewPointTemp = m_WeatherEagle.getDewPointTemp();
    dBarometricPressure = m_WeatherEagle.getBarometricPressure();

	return nErr;
}

WeatherStationDataInterface::x2WindSpeedUnit X2WeatherStation::windSpeedUnit()
{
    WeatherStationDataInterface::x2WindSpeedUnit nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedKph;
    int WeatherEagleUnit;
    std::stringstream tmp;

    WeatherEagleUnit = m_WeatherEagle.getWindSpeedUnit(WeatherEagleUnit);

    switch(WeatherEagleUnit) {
        case KPH:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedKph;
            break;
        case MPS:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedMps;
            break;
        case MPH:
            nUnit = WeatherStationDataInterface::x2WindSpeedUnit::windSpeedMph;
            break;
    }

    return nUnit ;
}
