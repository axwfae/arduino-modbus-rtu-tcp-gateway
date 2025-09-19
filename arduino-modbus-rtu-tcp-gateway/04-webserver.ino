#include "web_pages.h"

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";

  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }

  webServer.send(404, "text/plain", message);
  dbgln(F("[web] response 404 file not found"));
}

void web_status() {
  String htmlContent = webpage;
  String function_html = status_page;
  String data_converter =" ";
  String scan_cmd;

  htmlContent.replace("PAGE_REFRESH", "http-equiv=refresh content=2");

  String mdns_name = DEFAULT_mDns_NAME_HEAD;

  byte macBuffer[6];
  ETH.macAddress(macBuffer);

  for (byte i = 0; i < 6; i++) {
    if (macBuffer[i] < 16) data_converter += F("0");
    data_converter += String(macBuffer[i], HEX);
    if(i > 2)
    {
      if (macBuffer[i] < 16) mdns_name += F("0");
      mdns_name +=  String(macBuffer[i], HEX);
    }
    if (i < 5) data_converter += F(":");
  }
  function_html.replace("MAC_ADDRESS", data_converter);
  
  IPAddress devIP = ETH.localIP();
  function_html.replace("IP_ADDRESS",  devIP.toString());

  mdns_name += F(".local");
  function_html.replace("MDNS_NAME", mdns_name);

  byte mod_seconds = byte((seconds) % 60);
  byte mod_minutes = byte((seconds / 60) % 60);
  byte mod_hours = byte((seconds / (60 * 60)) % 24);
  int days = (seconds / (60U * 60U * 24));

  function_html.replace("TIME_D", String(days, DEC));
  function_html.replace("TIME_H", String(mod_hours, DEC));
  function_html.replace("TIME_M", String(mod_minutes, DEC));
  function_html.replace("TIME_S", String(mod_seconds, DEC));
  
  function_html.replace("ETH_T_D", String(data.ethCnt[DATA_TX], DEC));
  function_html.replace("ETH_R_D", String(data.ethCnt[DATA_RX], DEC));
  function_html.replace("ETH_ERROR_TCP", String(data.errorCnt[ERROR_TCP], DEC));
  function_html.replace("ETH_TCP_PORT", String(data_config.tcpPort, DEC));
  function_html.replace("ETH_UDP_PORT", String(data_config.udpPort, DEC));
  function_html.replace("ETH_UDP_EN",(data_config.enableUDP) ? "Enable" : "Disable");
  function_html.replace("RTU_OVER_EN",(data_config.enableRtuOverTcp) ? "Enable" : "Disable");

  function_html.replace("RTU_T_D", String(data.rtuCnt[DATA_TX], DEC));
  function_html.replace("RTU_R_D", String(data.rtuCnt[DATA_RX], DEC));

  function_html.replace("RTU_SLAVE_OK", String(data.errorCnt[SLAVE_OK], DEC));
  function_html.replace("RTU_SLAVE_ERROR_0X", String(data.errorCnt[SLAVE_ERROR_0X], DEC));
  function_html.replace("RTU_SLAVE_ERROR_0A", String(data.errorCnt[SLAVE_ERROR_0A], DEC));
  function_html.replace("RTU_SLAVE_ERROR_0B", String(data.errorCnt[SLAVE_ERROR_0B], DEC));
  function_html.replace("RTU_ERROR_TIMEOUT", String(data.errorCnt[ERROR_TIMEOUT], DEC));
  function_html.replace("RTU_ERROR_RTU", String(data.errorCnt[ERROR_RTU], DEC));

  scan_cmd = "reset_value";
  if(scan_cmd == webServer.arg("cnt_reset"))
  {
    resetStats();
    updateEeprom();
  }

  data_converter = F("<tr><td>Modbus Slaves device<td><td> set uid max = ");
  data_converter += String(data_config.max_slaves, DEC);

  for (byte k = 1; k < data_config.max_slaves; k++) {
    if((scanCounter > 0) && (k > scanCounter)) break;
    for (byte s = 0; s <= SLAVE_ERROR_0B_QUEUE; s++){
      if (getSlaveStatus(k, s) == true || k == scanCounter) 
      {
        data_converter += F("<tr><td><td>uid:");
        data_converter += String(k, DEC);
        data_converter += F("<td>");
        if (k == scanCounter)
        {
          data_converter += F("Scanning...");
          break;
        }
#if 1
        if(SLAVE_OK == s) data_converter += F("Slave ok ..");
#else
        switch(s)
        {
          case SLAVE_OK:
            data_converter += F("Slave Responded");
            break;
          case SLAVE_ERROR_0X:
            data_converter += F("Slave Responded with Error (Codes 1~8)");
            break;
          case SLAVE_ERROR_0A:
            data_converter += F("Gateway Overloaded (Code 10)");
            break;
          case SLAVE_ERROR_0B:
          case SLAVE_ERROR_0B_QUEUE:
            data_converter += F("Slave Failed to Respond (Code 11)");
            break;
        }
#endif        
      }
    }
  }
  data_converter += F("<tr><td><td><td>none");
  data_converter += F("<tr><td><td><td><tr><td><td><td>");
  data_converter += F("<tr><td><td><td> <tr><td><td><td><input type=submit name=rtu_scan value='rescan_device'>");
  
  scan_cmd = "rescan_device";
  if(scan_cmd == webServer.arg("rtu_scan"))
  {
    if(0 == scanCounter)
    {
      memset(&slaveStatus, 0, sizeof(slaveStatus));  // clear all status flags
      scanCounter = 1;
    } 
  }

  function_html.replace("RTU_DEVICE_CHECK", data_converter);

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);
}


void web_ip() {
  String htmlContent = webpage;
  String function_html = ip_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  if(data_config.enableDhcp)
  {
    function_html.replace("IP_DHCP_SELECT>E", "selected >E");
  }
  else
  {
    function_html.replace("IP_DHCP_SELECT>D", "selected >D");
  }
  function_html.replace("IP_DHCP_SELECT", " ");

  function_html.replace("IP_SIP_A", String(data_config.ip[0],DEC));
  function_html.replace("IP_SIP_B", String(data_config.ip[1],DEC));
  function_html.replace("IP_SIP_C", String(data_config.ip[2],DEC));
  function_html.replace("IP_SIP_D", String(data_config.ip[3],DEC));

  function_html.replace("IP_SUBMASK_A", String(data_config.subnet[0],DEC));
  function_html.replace("IP_SUBMASK_B", String(data_config.subnet[1],DEC));
  function_html.replace("IP_SUBMASK_C", String(data_config.subnet[2],DEC));
  function_html.replace("IP_SUBMASK_D", String(data_config.subnet[3],DEC));

  function_html.replace("IP_GATEWAY_A", String(data_config.gateway[0],DEC));
  function_html.replace("IP_GATEWAY_B", String(data_config.gateway[1],DEC));
  function_html.replace("IP_GATEWAY_C", String(data_config.gateway[2],DEC));
  function_html.replace("IP_GATEWAY_D", String(data_config.gateway[3],DEC));


  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);
}

void web_ip_post() { 
  uint16_t web_post_value;
  IPAddress ip_data;
  bool ip_err;

  web_post_value = webServer.arg("ip_dhcp_set").toInt();
  dbg("ip_dhcp_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 0: data_config.enableDhcp = false; break;
    case 1: data_config.enableDhcp = true; break;
    default: data_config.enableDhcp = DEFAULT_DHCP_EN; break;
  }

  ip_err = false;
  web_post_value = webServer.arg("ip_sip_a").toInt();
  dbg("ip_sip_a = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.ip[0] = web_post_value;
  }
  web_post_value = webServer.arg("ip_sip_b").toInt();
  dbg("ip_sip_b = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.ip[1] = web_post_value;
  }
  web_post_value = webServer.arg("ip_sip_c").toInt();
  dbg("ip_sip_c = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.ip[2] = web_post_value;
  }
  web_post_value = webServer.arg("ip_sip_d").toInt();
  dbg("ip_sip_d = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.ip[3] = web_post_value;
  }
  if(ip_err) data_config.ip = DEFAULT_STATIC_IP;

  ip_err = false;
  web_post_value = webServer.arg("ip_submask_a").toInt();
  dbg("ip_submask_a = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.subnet[0] = web_post_value;
  }
  web_post_value = webServer.arg("ip_submask_b").toInt();
  dbg("ip_submask_b = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.subnet[1] = web_post_value;
  }
  web_post_value = webServer.arg("ip_submask_c").toInt();
  dbg("ip_submask_c = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.subnet[2] = web_post_value;
  }
  web_post_value = webServer.arg("ip_submask_d").toInt();
  dbg("ip_submask_d = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.subnet[3] = web_post_value;
  }
  if(ip_err) data_config.subnet = DEFAULT_SUBMASK;

  ip_err = false;
  web_post_value = webServer.arg("ip_geteway_a").toInt();
  dbg("ip_geteway_a = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.gateway[0] = web_post_value;
  }
  web_post_value = webServer.arg("ip_geteway_b").toInt();
  dbg("ip_geteway_b = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.gateway[1] = web_post_value;
  }
  web_post_value = webServer.arg("ip_geteway_c").toInt();
  dbg("ip_geteway_c = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.gateway[2] = web_post_value;
  }
  web_post_value = webServer.arg("ip_geteway_d").toInt();
  dbg("ip_geteway_d = ");
  dbgln(web_post_value);
  if((web_post_value < 0) || (web_post_value > 255))
  {
    ip_err = true;
  }
  else
  {
    data_config.gateway[3] = web_post_value;
  }
  if(ip_err) data_config.gateway = DEFAULT_GATEWAY;

  eeprom_w("eeprom_c.bin", (byte *) &data_config, sizeof(data_config));
  ee_config_out();

  String htmlContent = webpage;
  String function_html = finish_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);

  dbgln("Restarting...");
  delay(50);
  ESP.restart();      
}



void web_tcp() {
  String htmlContent = webpage;
  String function_html = tcp_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  function_html.replace("TCP_TCP_P", String(data_config.tcpPort,DEC));

  if(data_config.enableUDP)
  {
    function_html.replace("TCP_UDP_SELECT>E", "selected >E");
  }
  else
  {
    function_html.replace("TCP_UDP_SELECT>D", "selected >D");
  }
  function_html.replace("TCP_UDP_SELECT", " ");

  function_html.replace("TCP_UDP_P", String(data_config.udpPort,DEC));

  function_html.replace("TCP_TIME_OUT", String(data_config.tcpTimeout,DEC));

  if(data_config.enableRtuOverTcp)
  {
    function_html.replace("TCP_RTU_SELECT>E", "selected >E");
  }
  else
  {
    function_html.replace("TCP_RTU_SELECT>D", "selected >D");
  }
  function_html.replace("TCP_RTU_SELECT", " ");

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);
}

void web_tcp_post() { 
  uint16_t web_post_value;

  web_post_value = webServer.arg("tcp_tcp_port").toInt();
  dbg("tcp_tcp_port = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 65535))
  {
    data_config.tcpPort =  DEFAULT_TCP_PORT;
  }
  else
  {
    data_config.tcpPort = web_post_value;
  }

  web_post_value = webServer.arg("tcp_udp_set").toInt();
  dbg("tcp_udp_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 0: data_config.enableUDP = false; break;
    case 1: data_config.enableUDP = true; break;
    default: data_config.enableUDP = DEFAULT_UDP_EN; break;
  }

  web_post_value = webServer.arg("tcp_udp_port").toInt();
  dbg("tcp_udp_port = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 65535))
  {
    data_config.udpPort =  DEFAULT_UDP_PORT;
  }
  else
  {
    data_config.udpPort = web_post_value;
  }

  web_post_value = webServer.arg("tcp_timeout").toInt();
  dbg("tcp_timeout = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 3600))
  {
    data_config.tcpTimeout =  DEFAULT_TCP_TIMEOUT;
  }
  else
  {
    data_config.tcpTimeout = web_post_value;
  }

  web_post_value = webServer.arg("tcp_rtu_set").toInt();
  dbg("tcp_rtu_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 0: data_config.enableRtuOverTcp = false; break;
    case 1: data_config.enableRtuOverTcp = true; break;
    default: data_config.enableRtuOverTcp = DEFAULT_RTU_OVER_TCP; break;
  }

  eeprom_w("eeprom_c.bin", (byte *) &data_config, sizeof(data_config));
  ee_config_out();

  String htmlContent = webpage;
  String function_html = finish_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);

  dbgln("Restarting...");
  delay(50);
  ESP.restart();      
}



void web_rtu() {
  String htmlContent = webpage;
  String function_html = rtu_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  switch(data_config.baud)
  {
    case 3: function_html.replace("RTU_BPS_SELECT>30", "selected >30"); break;
    case 6: function_html.replace("RTU_BPS_SELECT>60", "selected >60"); break;
    case 9: function_html.replace("RTU_BPS_SELECT>90", "selected >90"); break;
    case 12: function_html.replace("RTU_BPS_SELECT>12", "selected >12"); break;
    case 24: function_html.replace("RTU_BPS_SELECT>24", "selected >24"); break;
    case 48: function_html.replace("RTU_BPS_SELECT>48", "selected >48"); break;
    case 96: function_html.replace("RTU_BPS_SELECT>96", "selected >96"); break;
    case 192: function_html.replace("RTU_BPS_SELECT>19", "selected >19"); break;
    case 384: function_html.replace("RTU_BPS_SELECT>38", "selected >38"); break;
    case 576: function_html.replace("RTU_BPS_SELECT>57", "selected >57"); break;
    case 1152: function_html.replace("RTU_BPS_SELECT>11", "selected >11"); break;
  }
  function_html.replace("RTU_BPS_SELECT", " ");

  switch(data_config.serialConfig & 0x0c)
  {
    case 0x00: function_html.replace("RTU_DATA_SELECT>5", "selected >5"); break;
    case 0x04: function_html.replace("RTU_DATA_SELECT>6", "selected >6"); break;
    case 0x08: function_html.replace("RTU_DATA_SELECT>7", "selected >7"); break;
    case 0x0c: function_html.replace("RTU_DATA_SELECT>8", "selected >8"); break;
  }
  function_html.replace("RTU_DATA_SELECT", " ");

  switch(data_config.serialConfig & 0x03)
  {
    case 0x00: function_html.replace("RTU_PARITY_SELECT>N", "selected >N"); break;
    case 0x02: function_html.replace("RTU_PARITY_SELECT>E", "selected >E"); break;
    case 0x03: function_html.replace("RTU_PARITY_SELECT>O", "selected >O"); break;
  }
  function_html.replace("RTU_PARITY_SELECT", " ");

  if(data_config.serialConfig & 0x20)
  {
    function_html.replace("RTU_STOP_SELECT>2", "selected >2");
  }
  else
  {
    function_html.replace("RTU_STOP_SELECT>1", "selected >1");
  }
  function_html.replace("RTU_STOP_SELECT", " ");

  function_html.replace("RTU_SMA_VAL", String(data_config.max_slaves,DEC));
  function_html.replace("RTU_IFD_VAL", String(data_config.frameDelay,DEC));
  function_html.replace("RTU_RT_VAL", String(data_config.serialTimeout,DEC));
  function_html.replace("RTU_ATTEMPTS_VAL", String(data_config.serialAttempts,DEC));

  if(data_config.enableBootScan)
  {
    function_html.replace("RTU_BS_SELECT>E", "selected >E");
  }
  else
  {
    function_html.replace("RTU_BS_SELECT>D", "selected >D");
  }
  function_html.replace("RTU_BS_SELECT", " ");  

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);
}

void web_rtu_post() { 
  uint16_t web_post_value;

  web_post_value = webServer.arg("rtu_bps_set").toInt();
  dbg("rtu_bps_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 0: data_config.baud = 3; break;
    case 1: data_config.baud = 6; break;
    case 2: data_config.baud = 9; break;
    case 3: data_config.baud = 12; break;
    case 4: data_config.baud = 24; break;
    case 5: data_config.baud = 48; break;
    case 6: data_config.baud = 96; break;
    case 7: data_config.baud = 192; break;
    case 8: data_config.baud = 384; break;
    case 9: data_config.baud = 576; break;
    case 10: data_config.baud = 1152; break;
    default: data_config.baud = 96; break;
  }

  data_config.serialConfig &= ~0x3f;

  web_post_value = webServer.arg("rtu_data_set").toInt();
  dbg("rtu_data_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 5: data_config.serialConfig |= 0x00; break;
    case 6: data_config.serialConfig |= 0x04; break;
    case 7: data_config.serialConfig |= 0x08; break;
    case 8: data_config.serialConfig |= 0x0c; break;
    default: data_config.serialConfig |= 0x0c; break;
  }

  web_post_value = webServer.arg("rtu_parity_set").toInt();
  dbg("rtu_parity_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 0: data_config.serialConfig |= 0x00; break;
    case 2: data_config.serialConfig |= 0x02; break;
    case 3: data_config.serialConfig |= 0x03; break;
    default: data_config.serialConfig |= 0x00; break;
  }

  web_post_value = webServer.arg("rtu_stop_set").toInt();
  dbg("rtu_stop_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 1: data_config.serialConfig |= 0x10; break;
    case 3: data_config.serialConfig |= 0x30; break;
    default: data_config.serialConfig |= 0x10; break;
  }

  web_post_value = webServer.arg("rtu_sma").toInt();
  dbg("rtu_sma = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 247))
  {
    data_config.max_slaves =  MAX_SLAVES;
  }
  else
  {
    data_config.max_slaves = web_post_value;
  }

  web_post_value = webServer.arg("rtu_ifd").toInt();
  dbg("rtu_ifd = ");
  dbgln(web_post_value);
  if((web_post_value < 5) || (web_post_value > 250))
  {
    data_config.frameDelay =  DEFAULT_FRAME_DELAY;
  }
  else
  {
    data_config.frameDelay = web_post_value;
  }

  web_post_value = webServer.arg("rtu_rt").toInt();
  dbg("rtu_rt = ");
  dbgln(web_post_value);
  if((web_post_value < 50) || (web_post_value > 5000))
  {
    data_config.serialTimeout =  DEFAULT_RESPONSE_TIMEPOUT;
  }
  else
  {
    data_config.serialTimeout = web_post_value;
  }

  web_post_value = webServer.arg("rtu_attempts").toInt();
  dbg("rtu_attempts = ");
  dbgln(web_post_value);
  if((web_post_value < 1) || (web_post_value > 5))
  {
    data_config.serialAttempts =  DEFAULT_ATTEMPTS;
  }
  else
  {
    data_config.serialAttempts = web_post_value;
  }

  web_post_value = webServer.arg("rtu_boot_scan_set").toInt();
  dbg("rtu_boot_scan_set = ");
  dbgln(web_post_value);
  switch(web_post_value)
  {
    case 0: data_config.enableBootScan = false; break;
    case 1: data_config.enableBootScan = true; break;
    default: data_config.enableBootScan = DEFAULT_BOOTSCAN_EN; break;
  }

  eeprom_w("eeprom_c.bin", (byte *) &data_config, sizeof(data_config));
  ee_config_out();

  String htmlContent = webpage;
  String function_html = finish_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);

  dbgln("Restarting...");
  delay(50);
  ESP.restart();      
}



void web_tools() {
  String htmlContent = webpage;
  String function_html = tools_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);
}

void web_tools_post() { 

  resetStats();
  data_config = DEFAULT_CONFIG;
  eeprom_w("eeprom_b.bin", (byte *) &data, sizeof(data));
  eeprom_w("eeprom_c.bin", (byte *) &data_config, sizeof(data_config));

  ee_data_out();
  ee_config_out();

  String htmlContent = webpage;
  String function_html = finish_page;
  String data_converter =" ";

  htmlContent.replace("PAGE_REFRESH", " ");

  htmlContent.replace("FUNCTION_DATA", function_html);
  
  webServer.send(200, "text/html", htmlContent);

  dbgln("Restarting...");
  delay(50);
  ESP.restart();      
}



void startWeb() {
  webServer.on("/", HTTP_GET, web_status);
  webServer.on("/status.htm", HTTP_GET, web_status);
  webServer.on("/status.htm", HTTP_POST, web_status);  

  webServer.on("/ip.htm", HTTP_GET, web_ip);
  webServer.on("/ip_post", HTTP_POST, web_ip_post);

  webServer.on("/tcp.htm", HTTP_GET, web_tcp);
  webServer.on("/tcp_post", HTTP_POST, web_tcp_post);

  webServer.on("/rtu.htm", HTTP_GET, web_rtu);
  webServer.on("/rtu_post", HTTP_POST, web_rtu_post);

  webServer.on("/tools.htm", HTTP_GET, web_tools);
  webServer.on("/tools_post", HTTP_POST, web_tools_post);

  webServer.onNotFound(handleNotFound);
}

void recvWeb() {
  webServer.handleClient();
}
