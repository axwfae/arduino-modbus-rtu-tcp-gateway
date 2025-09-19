/* *******************************************************************
   Modbus TCP/UDP functions

   recvUdp()
   - receives Modbus UDP (or Modbus RTU over UDP) messages
   - calls checkRequest

   recvTcp()
   - receives Modbus TCP (or Modbus RTU over TCP) messages
   - calls checkRequest

   checkRequest()
   - checks Modbus TCP/UDP requests (correct MBAP header, CRC in case of Modbus RTU over TCP/UDP)
   - checks availability of queue
   - stores requests into queue or returns an error

   scanRequest()
   - inserts scan request into queue

   deleteRequest()
   - deletes requests from queue

   clearQueue()
   - clears queue and corresponding counters

   getSlaveStatus(), setSlaveStatus()
   - read from and write to bool array

   ***************************************************************** */

byte masks[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

// Stored in "header.requestType"
#define PRIORITY_REQUEST B10000000  // Request to slave which is not "nonresponding"
#define SCAN_REQUEST B01000000      // Request triggered by slave scanner
#define UDP_REQUEST B00100000       // UDP request
#define TCP_REQUEST B00001000       // TCP request
#define TCP_REQUEST_MASK B00000111  // Stores TCP client number

uint16_t crc;

/**************************************************************************/
/*!
  @brief Receives Modbus UDP (or Modbus RTU over UDP) messages, calls @ref checkRequest()
*/
/**************************************************************************/
void recvUdp() {
  uint16_t msgLength = modbusServer_UDP.parsePacket();
  if (msgLength) {
#ifdef ENABLE_EXTENDED_WEBUI
    data.ethCnt[DATA_RX] += msgLength;
#endif                               /* ENABLE_EXTENDED_WEBUI */
    byte inBuffer[MODBUS_SIZE + 4];  // Modbus TCP frame is 4 bytes longer than Modbus RTU frame
                                     // Modbus TCP/UDP frame: [0][1] transaction ID, [2][3] protocol ID, [4][5] length and [6] unit ID (address)..... no CRC
                                     // Modbus RTU frame: [0] address.....[n-1][n] CRC
    memset(inBuffer, 0, sizeof(inBuffer));
    modbusServer_UDP.read(inBuffer, sizeof(inBuffer));
    while (modbusServer_UDP.available()) modbusServer_UDP.read();
    byte errorCode = checkRequest(inBuffer, msgLength, (uint32_t)modbusServer_UDP.remoteIP(), modbusServer_UDP.remotePort(), UDP_REQUEST);
    if (errorCode) {
      // send back message with error code
      modbusServer_UDP.beginPacket(modbusServer_UDP.remoteIP(), modbusServer_UDP.remotePort());
      if (!data_config.enableRtuOverTcp) {
        modbusServer_UDP.write(inBuffer, 5);
        modbusServer_UDP.write(0x03);
      }
      byte addressPos = 6 * !data_config.enableRtuOverTcp;  // position of slave address in the incoming TCP/UDP message (0 for Modbus RTU over TCP/UDP and 6 for Modbus RTU over TCP/UDP)
      modbusServer_UDP.write(inBuffer[addressPos]);                      // address
      modbusServer_UDP.write(inBuffer[addressPos + 1] + 0x80);           // function + 0x80
      modbusServer_UDP.write(errorCode);
      if (data_config.enableRtuOverTcp) {
        crc = 0xFFFF;
        calculateCRC(inBuffer[addressPos]);
        calculateCRC(inBuffer[addressPos + 1] + 0x80);
        calculateCRC(errorCode);
        modbusServer_UDP.write(lowByte(crc));  // send CRC, low byte first
        modbusServer_UDP.write(highByte(crc));
      }
      modbusServer_UDP.endPacket();
#ifdef ENABLE_EXTENDED_WEBUI
      data.ethCnt[DATA_TX] += 5;
      if (!data_config.enableRtuOverTcp) data.ethCnt[DATA_TX] += 4;
#endif /* ENABLE_EXTENDED_WEBUI */
    }
  }
}

/**************************************************************************/
/*!
  @brief Receives Modbus TCP (or Modbus RTU over TCP) messages, calls @ref checkRequest()
*/
/**************************************************************************/
void recvTcp() {

  uint16_t msgLength = modbusServer_TCP.available();

  if(0 == msgLength) return;

#ifdef ENABLE_EXTENDED_WEBUI
  data.ethCnt[DATA_RX] += msgLength;
#endif                             /* ENABLE_EXTENDED_WEBUI */
  byte inBuffer[MODBUS_SIZE + 4];  // Modbus TCP frame is 4 bytes longer than Modbus RTU frame
                                   // Modbus TCP/UDP frame: [0][1] transaction ID, [2][3] protocol ID, [4][5] length and [6] unit ID (address).....
                                   // Modbus RTU frame: [0] address.....
  memset(inBuffer, 0, sizeof(inBuffer));
  modbusServer_TCP.read(inBuffer, sizeof(inBuffer));
  while (modbusServer_TCP.available()) modbusServer_TCP.read();

#ifdef DEBUG
  dbg(F("[tcp_in] = "));
  debug_hex(inBuffer, msgLength, 1);
#endif

  //byte errorCode = checkRequest(inBuffer, msgLength, {}, client.remotePort(), TCP_REQUEST | client.getSocketNumber());
  byte errorCode = checkRequest(inBuffer, msgLength, {}, modbusServer_TCP.remotePort(), TCP_REQUEST);
  if (errorCode) {
    // send back message with error code
    byte i = 0;
    byte outBuffer[9];
    if (!data_config.enableRtuOverTcp) {
      memcpy(outBuffer, inBuffer, 5);
      outBuffer[5] = 0x03;
      i = 6;
    }
    byte addressPos = 6 * !data_config.enableRtuOverTcp;  // position of slave address in the incoming TCP/UDP message (0 for Modbus RTU over TCP/UDP and 6 for Modbus RTU over TCP/UDP)
    outBuffer[i++] = inBuffer[addressPos];                // address
    outBuffer[i++] = inBuffer[addressPos + 1] + 0x80;     // function + 0x80
    outBuffer[i++] = errorCode;
    if (data_config.enableRtuOverTcp) {
      crc = 0xFFFF;
      calculateCRC(inBuffer[addressPos]);
      calculateCRC(inBuffer[addressPos + 1] + 0x80);
      calculateCRC(errorCode);
      outBuffer[i++] = lowByte(crc);  // send CRC, low byte first
      outBuffer[i++] = highByte(crc);
    }
    modbusServer_TCP.write(outBuffer, i);
#ifdef DEBUG
    dbg(F("[tcp_out_err] = "));
    debug_hex(outBuffer, i, 1);
#endif

#ifdef ENABLE_EXTENDED_WEBUI
    data.ethCnt[DATA_TX] += 5;
    if (!data_config.enableRtuOverTcp) data.ethCnt[DATA_TX] += 4;
#endif /* ENABLE_EXTENDED_WEBUI */
  }
}

void scanRequest() {
  // Insert scan request into queue, allow only one scan request in a queue
  static byte scanCommand[] = { SCAN_FUNCTION_FIRST, 0x00, SCAN_DATA_ADDRESS, 0x00, 0x01 };
  if (scanCounter != 0 && queueHeaders.available() > 1 && queueData.available() > sizeof(scanCommand) + 1 && scanReqInQueue == false) {
    scanReqInQueue = true;
    // Store scan request in request queue
    queueHeaders.push(header_t{
      { 0x00, 0x00 },           // tid[2]
      sizeof(scanCommand) + 1,  // msgLen
      { 0, 0, 0, 0 },           // remIP
      0,                        // remPort
      SCAN_REQUEST,             // requestType
      0,                        // atts
    });
    queueData.push(scanCounter);  // address of the scanned slave
    for (byte i = 0; i < sizeof(scanCommand); i++) {
      queueData.push(scanCommand[i]);
    }
    if (scanCommand[0] == SCAN_FUNCTION_FIRST) {
      scanCommand[0] = SCAN_FUNCTION_SECOND;
    } else {
      scanCommand[0] = SCAN_FUNCTION_FIRST;
      scanCounter++;
    }
    if (scanCounter == data_config.max_slaves + 1) scanCounter = 0;
  }
}

/**************************************************************************/
/*!
  @brief Checks Modbus TCP/UDP requests (correct MBAP header,
  CRC in case of Modbus RTU over TCP/UDP), checks availability of queue,
  stores requests into queue or returns an error.
  @param inBuffer Modbus TCP/UDP requests
  @param msgLength Length of the Modbus TCP/UDP requests
  @param remoteIP Remote IP
  @param remotePort Remote port
  @param requestType UDP or TCP, priority or scan request
  @return Modbus error code to be sent back to the recipient.
*/
/**************************************************************************/
byte checkRequest(byte inBuffer[], uint16_t msgLength, const uint32_t remoteIP, const uint16_t remotePort, byte requestType) {
  byte addressPos = 6 * !data_config.enableRtuOverTcp;  // position of slave address in the incoming TCP/UDP message (0 for Modbus RTU over TCP/UDP and 6 for Modbus RTU over TCP/UDP)
  if (data_config.enableRtuOverTcp) {                   // check CRC for Modbus RTU over TCP/UDP
    if (checkCRC(inBuffer, msgLength) == false) {
      data.errorCnt[ERROR_TCP]++;
      return 0;  // drop request and do not return any error code
    }
  } else {  // check MBAP header structure for Modbus TCP/UDP
    if (inBuffer[2] != 0x00 || inBuffer[3] != 0x00 || inBuffer[4] != 0x00 || inBuffer[5] != msgLength - 6) {
      data.errorCnt[ERROR_TCP]++;
      return 0;  // drop request and do not return any error code
    }
  }
  msgLength = msgLength - addressPos - (2 * data_config.enableRtuOverTcp);  // in Modbus RTU over TCP/UDP do not store CRC
  // check if we have space in request queue
  if (queueHeaders.available() < 1 || queueData.available() < msgLength) {
    setSlaveStatus(inBuffer[addressPos], SLAVE_ERROR_0A, true, false);
    return 0x0A;  // return Modbus error code 10 (Gateway Overloaded)
  }
  // allow only one request to non responding slaves
  if (getSlaveStatus(inBuffer[addressPos], SLAVE_ERROR_0B_QUEUE)) {
    data.errorCnt[SLAVE_ERROR_0B]++;
    return 0x0B;  // return Modbus error code 11 (Gateway Target Device Failed to Respond)
  } else if (getSlaveStatus(inBuffer[addressPos], SLAVE_ERROR_0B)) {
    setSlaveStatus(inBuffer[addressPos], SLAVE_ERROR_0B_QUEUE, true, false);
  } else {
    // Add PRIORITY_REQUEST flag to requests for responding slaves
    requestType = requestType | PRIORITY_REQUEST;
    priorityReqInQueue++;
  }
  if (inBuffer[addressPos] == 0x00) {          // Modbus Broadcast
    requestType = requestType | SCAN_REQUEST;  // Treat broadcast as scan (only one attempt, short timeout, do not expect response)
  }
  // all checkes passed OK, we can store the incoming data in request queue
//  if (requestType & TCP_REQUEST) {
//    socketInQueue[requestType & TCP_REQUEST_MASK]++;
//  }

  // Store in request queue
  queueHeaders.push(header_t{
    { inBuffer[0], inBuffer[1] },  // tid[2] (ignored in Modbus RTU over TCP/UDP)
    byte(msgLength),               // msgLen
    (IPAddress)remoteIP,           // remIP
    (unsigned int)remotePort,      // remPort
    byte(requestType),             // requestType
    0,                             // atts
  });
  for (byte i = 0; i < msgLength; i++) {
    queueData.push(inBuffer[i + addressPos]);
  }
  if (queueData.size() > queueDataSize) queueDataSize = queueData.size();
  if (queueHeaders.size() > queueHeadersSize) queueHeadersSize = queueHeaders.size();
  return 0;
}

void deleteRequest()  // delete request from queue
{
  header_t myHeader = queueHeaders.first();
  if (myHeader.requestType & SCAN_REQUEST) scanReqInQueue = false;
//  if (myHeader.requestType & TCP_REQUEST) socketInQueue[myHeader.requestType & TCP_REQUEST_MASK]--;
  if (myHeader.requestType & PRIORITY_REQUEST) priorityReqInQueue--;
  for (byte i = 0; i < myHeader.msgLen; i++) {
    queueData.shift();
  }
  queueHeaders.shift();
}

void clearQueue() {
  queueHeaders.clear();
  queueData.clear();
  scanReqInQueue = false;
  priorityReqInQueue = false;
//  memset(socketInQueue, 0, sizeof(socketInQueue));
  memset(slaveStatus[SLAVE_ERROR_0B_QUEUE], 0, sizeof(slaveStatus[SLAVE_ERROR_0B_QUEUE]));
  sendMicroTimer.sleep(0);
}

bool getSlaveStatus(const byte slave, const byte status) {
  if (slave >= data_config.max_slaves) return false;  // error
  return ((slaveStatus[status][slave / 8] & masks[slave & 7]) > 0);
}

void setSlaveStatus(const byte slave, byte status, const bool value, const bool isScan) {
  if (slave >= data_config.max_slaves || status > SLAVE_ERROR_0B_QUEUE) return;  // error
  if (value == 0) {
    slaveStatus[status][slave / 8] &= ~masks[slave & 7];
  } else {
    for (byte i = 0; i <= SLAVE_ERROR_0B_QUEUE; i++) {
      slaveStatus[i][slave / 8] &= ~masks[slave & 7];  // set all other flags to false, SLAVE_ERROR_0B_QUEUE is the last slave status
    }
    slaveStatus[status][slave / 8] |= masks[slave & 7];
    if (status != SLAVE_ERROR_0B_QUEUE && isScan == false) data.errorCnt[status]++;  // there is no counter for SLAVE_ERROR_0B_QUEUE, ignor scans in statistics
  }
}
