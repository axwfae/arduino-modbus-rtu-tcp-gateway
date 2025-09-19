/**************************************************************************/
/*!
  @brief Initiates HW serial interface which we use for the RS485 line.
*/
/**************************************************************************/
void startSerial() {
#ifdef DEBUG

  dbgln(F(" "));
  
  dbg(F("rtu mode set = "));
  dbg((data_config.baud * 100UL));
  dbg((" "));
  switch(data_config.serialConfig & 0x0c)
  {
    case 0x00: dbg(F("-5")); break;
    case 0x04: dbg(F("-6")); break;
    case 0x08: dbg(F("-7")); break;
    case 0x0c: dbg(F("-8")); break;
  }
  switch(data_config.serialConfig & 0x03)
  {
    case 0x00: dbg(F("-N-")); break;
    case 0x02: dbg(F("-E-")); break;
    case 0x03: dbg(F("-O-")); break;
  }

  if(data_config.serialConfig & 0x20)
  {
    dbgln(F("2 "));
  }
  else
  {
    dbgln(F("1 "));
  }

#endif

  mySerial.begin((data_config.baud * 100UL), data_config.serialConfig, mySerial_rx_pin, mySerial_tx_pin);

#ifdef RS485_CONTROL_PIN
  pinMode(RS485_CONTROL_PIN, OUTPUT);
  digitalWrite(RS485_CONTROL_PIN, RS485_RECEIVE);  // Init Transceiver
#endif                                             /* RS485_CONTROL_PIN */
}

// Number of bits per character (11 in default Modbus RTU settings)
byte bitsPerChar() {
  byte bits =
    1 +                                                         // start bit
    (((data_config.serialConfig & 0x0c) >> 2) + 5) +            // data bits
    (((data_config.serialConfig & 0x20) >> 5) + 1);             // stop bits
  if ((data_config.serialConfig & 0x03) > 1) bits += 1;         // parity bit (if present)
  return bits;
}

/*
  arduino-esp32/blob/master/cores/esp32/HardwareSerial.h
  xx xx xx
   s  d  p

  SERIAL_5N1 = 0x8000010,
  SERIAL_6N1 = 0x8000014,
  SERIAL_7N1 = 0x8000018,
  SERIAL_8N1 = 0x800001c,
  SERIAL_5N2 = 0x8000030,
  SERIAL_6N2 = 0x8000034,
  SERIAL_7N2 = 0x8000038,
  SERIAL_8N2 = 0x800003c,
  SERIAL_5E1 = 0x8000012,
  SERIAL_6E1 = 0x8000016,
  SERIAL_7E1 = 0x800001a,
  SERIAL_8E1 = 0x800001e,
  SERIAL_5E2 = 0x8000032,
  SERIAL_6E2 = 0x8000036,
  SERIAL_7E2 = 0x800003a,
  SERIAL_8E2 = 0x800003e,
  SERIAL_5O1 = 0x8000013,
  SERIAL_6O1 = 0x8000017,
  SERIAL_7O1 = 0x800001b,
  SERIAL_8O1 = 0x800001f,
  SERIAL_5O2 = 0x8000033,
  SERIAL_6O2 = 0x8000037,
  SERIAL_7O2 = 0x800003b,
  SERIAL_8O2 = 0x800003f
*/

// Character timeout in micros
uint32_t charTimeOut() {
  if (data_config.baud <= 192) {
    return (15000UL * bitsPerChar()) / data_config.baud;  // inter-character time-out should be 1,5T
  } else {
    return 750;
  }
}

// Minimum frame delay in micros
uint32_t frameDelay() {
  if (data_config.baud <= 192) {
    return (35000UL * bitsPerChar()) / data_config.baud;  // inter-frame delay should be 3,5T
  } else {
    return 1750;  // 1750 μs
  }
}


static bool eth_connected = false;

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      dbgln(F("ETH Started"));
      //set eth hostname here
      ETH.setHostname("esp32-eth0");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED: 
		  dbgln(F("ETH Connected")); 
		  break;
    case ARDUINO_EVENT_ETH_GOT_IP:
		  dbgln(F(""));
		  dbg(F("ETH Got IP:"));
		  dbgln(esp_netif_get_desc(info.got_ip.esp_netif)); 
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      dbgln(F("ETH Lost IP"));
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      dbgln(F("ETH Disconnected"));
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      dbgln(F("ETH Stopped"));
      eth_connected = false;
      break;
    default: break;
  }
}



/**************************************************************************/
/*!
  @brief Initiates ethernet interface, if DHCP enabled, gets IP from DHCP,
  starts all servers (UDP, web server).
*/
/**************************************************************************/
void startEthernet() {

  Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(ETH_DEV_TYPE, ETH_PHY_ADDR, ETH_DEV_CS, ETH_DEV_IRQ, ETH_DEV_RST, SPI, ETH_SPI_FREQ_MHZ);

#ifdef ENABLE_DHCP
  if(data_config.enableDhcp)
  {
    dbgln(F("eth use dhcp mode."));
  }
  else
#endif  
  {
    dbgln(F("eth use static ip mode."));

    if (false == ETH.config(data_config.ip, data_config.subnet, data_config.gateway))
    {
      dbgln(F("eth failed set static"));
    }
  }

  while(false == eth_connected)
  {
    delay(500);
    dbg(F("."));
  }


  dbgln(F(""));
  dbg(F("code ver = "));
  dbg(VERSION[0]);
  dbg(F("."));
  dbgln(VERSION[1]);

  dbgln(F(""));
  dbg(F("ETH mac address:"));
  byte macBuffer[6];
  ETH.macAddress(macBuffer);
  for (byte i = 0; i < 6; i++) {
    if (macBuffer[i] < 16) dbg(F("0"));
    dbg(String(macBuffer[i], HEX));
    if (i < 5) dbg(F(":"));
  }

  dbgln(F(""));
  dbgln(F(""));
  dbgln(F("ETH connected"));
  dbg(F("IP address: "));
  dbgln(ETH.localIP());
  dbg(F("Subnet mask: "));
  dbgln(ETH.subnetMask());
  dbg(F("Gateway IP: "));
  dbgln(ETH.gatewayIP()); 

#ifdef ENABLE_DHCP
  if(data_config.enableDhcp)
  {
    data_config.ip = ETH.localIP();
    data_config.subnet = ETH.subnetMask();
    data_config.gateway = ETH.gatewayIP();
  }
#endif

#if 0
  if(nullptr != modbusServer_ptr)
  {
    modbusServer_ptr->end();
    delete modbusServer_ptr;
    modbusServer_ptr = nullptr;
  }
#endif

  modbusServer_ptr = new NetworkServer(data_config.tcpPort);
  modbusServer_ptr->begin();

  if(data_config.enableUDP)
    modbusServer_UDP.begin(data_config.udpPort);

#ifdef ENABLE_EXTENDED_WEBUI
  webServer.begin();
  startWeb();
#endif

  String mdns_name =DEFAULT_mDns_NAME_HEAD;
  for (byte i = 3; i < 6; i++) {
    if (macBuffer[i] < 16) mdns_name += F("0");
    mdns_name += String(macBuffer[i], HEX);
  }

  if (MDNS.begin(mdns_name)) {
    dbg(F("mDns ="));
    dbg(mdns_name);
    dbgln(F(".local"));
  }
  else
  {
    dbgln(F("mDns fail..!!"));
  }

  dbgln(F("[arduino] Server available at http://"));
}

/**************************************************************************/
/*!
  @brief Maintains uptime in case of millis() overflow.
*/
/**************************************************************************/
#ifdef ENABLE_EXTENDED_WEBUI
void maintainUptime() {
  uint32_t milliseconds = millis();
  if (last_milliseconds > milliseconds) {
    //in case of millis() overflow, store existing passed seconds
    remaining_seconds = seconds;
  }
  //store last millis(), so that we can detect on the next call
  //if there is a millis() overflow ( millis() returns 0 )
  last_milliseconds = milliseconds;
  //In case of overflow, the "remaining_seconds" variable contains seconds counted before the overflow.
  //We add the "remaining_seconds", so that we can continue measuring the time passed from the last boot of the device.
  seconds = (milliseconds / 1000) + remaining_seconds;
}
#endif /* ENABLE_EXTENDED_WEBUI */

/**************************************************************************/
/*!
  @brief Synchronizes roll-over of data counters to zero.
*/
/**************************************************************************/
bool rollover() {
  const uint32_t ROLLOVER = 0xFFFFFF00;
  for (byte i = 0; i < ERROR_LAST; i++) {
    if (data.errorCnt[i] > ROLLOVER) {
      return true;
    }
  }
#ifdef ENABLE_EXTENDED_WEBUI
  if (seconds > ROLLOVER) {
    return true;
  }
  for (byte i = 0; i < DATA_LAST; i++) {
    if (data.rtuCnt[i] > ROLLOVER || data.ethCnt[i] > ROLLOVER) {
      return true;
    }
  }
#endif /* ENABLE_EXTENDED_WEBUI */
  return false;
}

/**************************************************************************/
/*!
  @brief Resets error stats, RTU counter and ethernet data counter.
*/
/**************************************************************************/
void resetStats() {
  memset(data.errorCnt, 0, sizeof(data.errorCnt));
#ifdef ENABLE_EXTENDED_WEBUI
  memset(data.rtuCnt, 0, sizeof(data.rtuCnt));
  memset(data.ethCnt, 0, sizeof(data.ethCnt));
  remaining_seconds = -(millis() / 1000);
#endif /* ENABLE_EXTENDED_WEBUI */
}


/**************************************************************************/
/*!
  @brief Write (update) data to Arduino EEPROM.
*/
/**************************************************************************/
void updateEeprom() {
  eepromTimer.sleep(EEPROM_INTERVAL * 60UL * 60UL * 1000UL);  // EEPROM_INTERVAL is in hours, sleep is in milliseconds!
  data.eepromWrites++;                                        // we assume that at least some bytes are written to EEPROM during EEPROM.update or EEPROM.put

  eeprom_w("eeprom_b.bin", (byte *) &data, sizeof(data));

  ee_data_out();
}


//uint32_t lastSocketUse[MAX_SOCK_NUM];
//byte socketInQueue[MAX_SOCK_NUM];
/**************************************************************************/
/*!
  @brief Closes sockets which are waiting to be closed or which refuse to close,
  forwards sockets with data available for further processing by the webserver,
  disconnects (closes) sockets which are too old (idle for too long), opens
  new sockets if needed (and if available).
  From https://github.com/SapientHetero/Ethernet/blob/master/src/socket.cpp
*/
/**************************************************************************/
void manageSockets() {
  if(!modbusServer_TCP)
  {
    if(modbusServer_ptr->hasClient())
    {
      modbusServer_TCP = modbusServer_ptr->available();
      //modbusServer_TCP.flush();
      dbgln("TCP Connected");
    }
  }
   
  if(modbusServer_TCP) recvTcp();  

  if(data_config.enableUDP) recvUdp();

#ifdef ENABLE_EXTENDED_WEBUI    
  recvWeb();
#endif    
}

void debug_hex(byte *hex_data, byte len, bool ln_en)
{
  byte cnt;

  for(cnt = 0; cnt < len; cnt++)
  {
    if (hex_data[cnt] < 16) dbg(F("0"));
    dbg(String(hex_data[cnt], HEX));
    dbg(F(" "));
  }
  if(ln_en) dbgln(F(" "));
}


void ee_data_out()
{
  dbgln(F(""));
  dbgln(F("=== def ==="));
  dbg(F("eepromWrites ="))
  dbgln(data.eepromWrites);
  
  dbg(F("major ="))
  dbgln(data.major);
  
  dbgln(F("=== cnt ==="));

  dbg(F("SLAVE_OK ="))
  dbgln(data.errorCnt[SLAVE_OK]);
  dbg(F("SLAVE_ERROR_0X ="))
  dbgln(data.errorCnt[SLAVE_ERROR_0X]);
  dbg(F("SLAVE_ERROR_0A ="))
  dbgln(data.errorCnt[SLAVE_ERROR_0A]);
  dbg(F("SLAVE_ERROR_0B ="))
  dbgln(data.errorCnt[SLAVE_ERROR_0B]);
  dbg(F("SLAVE_ERROR_0B_QUEUE ="))
  dbgln(data.errorCnt[SLAVE_ERROR_0B_QUEUE]);
  dbg(F("ERROR_TIMEOUT ="))
  dbgln(data.errorCnt[ERROR_TIMEOUT]);
  dbg(F("ERROR_RTU ="))
  dbgln(data.errorCnt[ERROR_RTU]);
  dbg(F("ERROR_TCP ="))
  dbgln(data.errorCnt[ERROR_TCP]);

  dbg(F("rtuCnt DATA_TX ="))
  dbgln(data.rtuCnt[DATA_TX]);
  dbg(F("rtuCnt DATA_RX ="))
  dbgln(data.rtuCnt[DATA_RX]);
  dbg(F("ethCnt DATA_TX ="))
  dbgln(data.ethCnt[DATA_TX]);
  dbg(F("ethCnt DATA_RX ="))
  dbgln(data.ethCnt[DATA_RX]);

  dbgln(F("=== end ==="));
}


void ee_config_out()
{
  dbgln(F(""));
  dbgln(F("=== config ==="));
  dbg(F("enableDhcp ="))
  dbgln(data_config.enableDhcp);
  dbg(F("ip ="))
  dbgln(data_config.ip);
  dbg(F("subnet ="))
  dbgln(data_config.subnet);
  dbg(F("gateway ="))
  dbgln(data_config.gateway);
  dbg(F("tcpPort ="))
  dbgln(data_config.tcpPort);
  dbg(F("enableUDP ="))
  dbgln(data_config.enableUDP);
  dbg(F("udpPort ="))
  dbgln(data_config.udpPort);
  dbg(F("tcpTimeout ="))
  dbgln(data_config.tcpTimeout);
  dbg(F("enableRtuOverTcp ="))
  dbgln(data_config.enableRtuOverTcp);
  dbg(F("baud ="))
  dbgln(data_config.baud);
  dbg(F("serialConfig ="))
  dbgln(data_config.serialConfig);
  dbg(F("frameDelay ="))
  dbgln(data_config.frameDelay);
  dbg(F("serialTimeout ="))
  dbgln(data_config.serialTimeout);
  dbg(F("serialAttempts ="))
  dbgln(data_config.serialAttempts);
  dbg(F("enableBootScan ="))
  dbgln(data_config.enableBootScan);
  dbg(F("max_slaves ="))
  dbgln(data_config.max_slaves);
  dbgln(F("=== end ==="));
}

bool eeprom_r(String pFilename, byte *pData, word wSize)
{
  File fsSaveFile;

  if (!pFilename.startsWith("/")) {
    pFilename = "/" + pFilename;
  }

  fsSaveFile = SPIFFS.open(pFilename.c_str(), FILE_READ);
  if (fsSaveFile)
  {
    fsSaveFile.readBytes((char *)pData, wSize);
    fsSaveFile.close();
    return true;
  }
  else
  {
    dbg(pFilename);
    dbgln(F(" read fail.."));
    fsSaveFile.close();
    return false;
  }
}

bool eeprom_w(String pFilename, byte *pData, word wSize)
{
  File fsSaveFile;

  if (!pFilename.startsWith("/")) {
    pFilename = "/" + pFilename;
  }

  fsSaveFile = SPIFFS.open(pFilename.c_str(), FILE_WRITE);
  if (fsSaveFile)
  {
    fsSaveFile.write(pData, wSize);
    fsSaveFile.close();
    return true;
  }
  else
  {
    dbg(pFilename);
    dbgln(F(" write  fail.."));
    fsSaveFile.close();
    return false;
  }
}
