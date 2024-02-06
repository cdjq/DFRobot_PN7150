/*!
 * @file nfcCardEmuSupport.ino
 * @brief nfc 卡模拟示例
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT license (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2024-01-29
 * @url https://github.com/DFRobot/DFRobot_PN7150
*/
#include "DFRobot_PN7150.h"

#define CARDEMU_RAW_EXCHANGE

/* Discovery loop configuration according to the targeted modes of operation */
unsigned char DiscoveryTechnologies[] = {
    MODE_LISTEN | TECH_PASSIVE_NFCA,
    MODE_LISTEN | TECH_PASSIVE_NFCB,
};

/* Mode configuration according to the targeted modes of operation */
unsigned char mode = NXPNCI_MODE_CARDEMU;

DFRobot_PN7150_I2C PN7150;
DFRobot_PN7150_I2C::NxpNci_RfIntf_t RfInterface;

#define ADD 0 // Enlarge NDEF message by adding dummy content
const char NDEF_MESSAGE[14 + ADD] = { 0xC1,   // MB/ME/CF/1/IL/TNF
        0x01,   // TYPE LENGTH
        (0x07 + ADD) >> 24,   // PAYLOAD LENTGH MSB
        (0x07 + ADD) >> 16,   // PAYLOAD LENTGH
        (0x07 + ADD) >> 8,    // PAYLOAD LENTGH
        (0x07 + ADD) & 0xFF,  // PAYLOAD LENTGH LSB
        'T',    // TYPE
  // PAYLOAD
      0x02,   // Status
      'e', 'n', // Language
      'T', 'e', 's', 't' };

void NdefPush_Cb(unsigned char* pNdefRecord, unsigned short NdefRecordSize)
{
  (void)pNdefRecord;
  (void)NdefRecordSize;
  printf("--- NDEF Record sent\n\n");
}

#ifdef CARDEMU_RAW_EXCHANGE
void PICC_ISO14443_4_scenario(void)
{
  unsigned char OK[] = { 0x90, 0x00 };
  unsigned char Cmd[256];
  unsigned char CmdSize;

  while (1) {
    if (PN7150.NxpNci_CardModeReceive(Cmd, &CmdSize) == NFC_SUCCESS) {
      if ((CmdSize >= 2) && (Cmd[0] == 0x00)) {
        switch (Cmd[1]) {
        case 0xA4:
          printf("Select File received\n");
          break;

        case 0xB0:
          printf("Read Binary received\n");
          break;

        case 0xD0:
          printf("Write Binary received\n");
          break;

        default:
          break;
        }

        PN7150.NxpNci_CardModeSend(OK, sizeof(OK));
      }
    } else {
      printf("End of transaction\n");
      return;
    }
  }
}
#endif // ifdef CARDEMU_RAW_EXCHANGE

void setup()
{
  Serial.begin(115200);
  
  /* Register NDEF message to be sent to remote reader */
  T4T_NDEF_EMU_SetMessage((unsigned char*)NDEF_MESSAGE, sizeof(NDEF_MESSAGE), *NdefPush_Cb);

  // Initialize the NFC module
  Serial.print("Connecting Device . . .");
  while (!(PN7150.begin(mode))) {
    Serial.print(" .");
    delay(1000);
  }
  /* Start Discovery */
  while (PN7150.NxpNci_StartDiscovery(DiscoveryTechnologies, sizeof(DiscoveryTechnologies)) != NFC_SUCCESS) {
    Serial.print("Error: cannot start discovery\n");
    delay(1000);
  }
  Serial.println("\nBegin ok!");
}

void loop()
{
  printf("\nWAITING FOR DEVICE DISCOVERY\n");

  /* Wait until a peer is discovered */
  while (PN7150.NxpNci_WaitForDiscoveryNotification(&RfInterface) != NFC_SUCCESS);

  /* Is activated from remote T4T ? */
  if ((RfInterface.Interface == INTF_ISODEP) && ((RfInterface.ModeTech & MODE_MASK) == MODE_LISTEN)) {
    printf(" - LISTEN MODE: Activated from remote Reader\n");
#ifndef CARDEMU_RAW_EXCHANGE
    PN7150.NxpNci_ProcessCardMode(RfInterface);
#else
    PICC_ISO14443_4_scenario();
#endif
    printf("READER DISCONNECTED\n");
  } else {
    printf("WRONG DISCOVERY\n");
  }
}
