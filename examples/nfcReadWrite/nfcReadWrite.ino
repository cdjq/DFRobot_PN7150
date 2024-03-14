/*!
 * @file nfcReadWrite.ino
 * @brief read data and write data to nfc card
 * @details  This demo runs on the arduino platform.
           Download this demo to learn how to wirte data to card.
           We can read the data on the card to see if the write is successful.
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT license (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2024-01-29
 * @url https://github.com/DFRobot/DFRobot_PN7150
*/
#include "DFRobot_PN7150.h"

/* Discovery loop configuration according to the targeted modes of operation */
unsigned char DiscoveryTechnologies[] = {
    MODE_POLL | TECH_PASSIVE_NFCA,
    MODE_POLL | TECH_PASSIVE_NFCF,
    MODE_POLL | TECH_PASSIVE_NFCB,
    MODE_POLL | TECH_PASSIVE_15693,
};

/* Mode configuration according to the targeted modes of operation */
unsigned char mode = NXPNCI_MODE_RW;

#define RW_NDEF_WRITING
#define RW_RAW_EXCHANGE

#define PRINT_BUF(x,y,z)  {unsigned int loop; printf(x); for(loop=0;loop<z;loop++) printf("%.2x ", y[loop]); printf("\n");}

DFRobot_PN7150_I2C PN7150;
DFRobot_PN7150_I2C::NxpNci_RfIntf_t RfInterface;

static unsigned char gNdefBuffer[100];
static unsigned short gPendingReception = 0;

void NdefPull_Cb(unsigned char* pNdefMessage, unsigned short ReceivedSize, unsigned int NdefMessageSize)
{
  unsigned char* pNdefRecord = pNdefMessage;
  NdefRecord_t NdefRecord;
  unsigned char save;

  if (pNdefMessage == NULL) {
    printf("--- Provisioned buffer size too small or NDEF message empty \n");
    return;
  }

  if (gPendingReception) {
    memcpy(&gNdefBuffer[NdefMessageSize - gPendingReception], pNdefMessage, ReceivedSize);
    gPendingReception -= ReceivedSize;
    if (gPendingReception > 0) {
      printf("--- Receiving Ndef fragmented message, pending reception of %d bytes\n", gPendingReception);
      return;
    } else {
      printf("--- Received complete Ndef fragmented message\n");
      pNdefRecord = gNdefBuffer;
    }
  } else if (NdefMessageSize > ReceivedSize) {
    if (sizeof(gNdefBuffer) < NdefMessageSize) {
      printf("--- Provisioned buffer size too small \n");
      return;
    }
    memcpy(gNdefBuffer, pNdefMessage, ReceivedSize);
    gPendingReception = NdefMessageSize - ReceivedSize;
    printf("--- Receiving Ndef fragmented message (size = %d bytes), pending reception of %d bytes\n", NdefMessageSize, gPendingReception);
    return;
  }

  while (pNdefRecord != NULL) {
    NdefRecord = DetectNdefRecordType(pNdefRecord);

    printf("--- NDEF record received:\n");

    switch (NdefRecord.recordType) {
    case MEDIA_VCARD:
    {
      save = NdefRecord.recordPayload[NdefRecord.recordPayloadSize];
      NdefRecord.recordPayload[NdefRecord.recordPayloadSize] = '\0';
      printf("   vCard:\n%s\n", NdefRecord.recordPayload);
      NdefRecord.recordPayload[NdefRecord.recordPayloadSize] = save;
    }
    break;

    case WELL_KNOWN_SIMPLE_TEXT:
    {
      save = NdefRecord.recordPayload[NdefRecord.recordPayloadSize];
      NdefRecord.recordPayload[NdefRecord.recordPayloadSize] = '\0';
      printf("   Text record: %s\n", &NdefRecord.recordPayload[NdefRecord.recordPayload[0] + 1]);
      NdefRecord.recordPayload[NdefRecord.recordPayloadSize] = save;
    }
    break;

    case WELL_KNOWN_SIMPLE_URI:
    {
      save = NdefRecord.recordPayload[NdefRecord.recordPayloadSize];
      NdefRecord.recordPayload[NdefRecord.recordPayloadSize] = '\0';
      printf("   URI record: %s%s\n", ndef_helper_UriHead(NdefRecord.recordPayload[0]), &NdefRecord.recordPayload[1]);
      NdefRecord.recordPayload[NdefRecord.recordPayloadSize] = save;
    }
    break;

    case MEDIA_HANDOVER_WIFI:
    {
      unsigned char index = 0, i;

      printf("--- Received WIFI credentials:\n");
      if ((NdefRecord.recordPayload[index] == 0x10) && (NdefRecord.recordPayload[index + 1] == 0x0e)) index += 4;
      while (index < NdefRecord.recordPayloadSize) {
        if (NdefRecord.recordPayload[index] == 0x10) {
          if (NdefRecord.recordPayload[index + 1] == 0x45) { printf("- SSID = "); for (i = 0;i < NdefRecord.recordPayload[index + 3];i++) printf("%c", NdefRecord.recordPayload[index + 4 + i]); printf("\n"); } else if (NdefRecord.recordPayload[index + 1] == 0x03) printf("- Authenticate Type = %s\n", ndef_helper_WifiAuth(NdefRecord.recordPayload[index + 5]));
          else if (NdefRecord.recordPayload[index + 1] == 0x0f) printf("- Encryption Type = %s\n", ndef_helper_WifiEnc(NdefRecord.recordPayload[index + 5]));
          else if (NdefRecord.recordPayload[index + 1] == 0x27) { printf("- Network key = "); for (i = 0;i < NdefRecord.recordPayload[index + 3];i++) printf("#"); printf("\n"); }
          index += 4 + NdefRecord.recordPayload[index + 3];
        } else continue;
      }
    }
    break;

    case WELL_KNOWN_HANDOVER_SELECT:
      printf("   Handover select version %d.%d\n", NdefRecord.recordPayload[0] >> 4, NdefRecord.recordPayload[0] & 0xF);
      break;

    case WELL_KNOWN_HANDOVER_REQUEST:
      printf("   Handover request version %d.%d\n", NdefRecord.recordPayload[0] >> 4, NdefRecord.recordPayload[0] & 0xF);
      break;

    case MEDIA_HANDOVER_BT:
      PRINT_BUF("   BT Handover payload = ", NdefRecord.recordPayload, NdefRecord.recordPayloadSize);
      break;

    case MEDIA_HANDOVER_BLE:
      PRINT_BUF("   BLE Handover payload = ", NdefRecord.recordPayload, NdefRecord.recordPayloadSize);
      break;

    case MEDIA_HANDOVER_BLE_SECURE:
      PRINT_BUF("   BLE secure Handover payload = ", NdefRecord.recordPayload, NdefRecord.recordPayloadSize);
      break;

    default:
      printf("   Unsupported NDEF record, cannot parse\n");
      break;
    }
    pNdefRecord = GetNextRecord(pNdefRecord);
  }

  printf("\n");
}

#if defined RW_NDEF_WRITING
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
  Serial.print("--- NDEF Record sent\n\n");
}
#endif // if defined P2P_SUPPORT || defined CARDEMU_SUPPORT
 
#ifdef RW_RAW_EXCHANGE
void PCD_MIFARE_scenario(void)
{
#define BLK_NB_MFC      4
#define KEY_MFC         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
#define DATA_WRITE_MFC  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff

  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  /* Authenticate sector 1 with generic keys */
  unsigned char Auth[] = { 0x40, BLK_NB_MFC / 4, 0x10, KEY_MFC };
  /* Read block 4 */
  unsigned char Read[] = { 0x10, 0x30, BLK_NB_MFC };
  /* Write block 4 */
  unsigned char WritePart1[] = { 0x10, 0xA0, BLK_NB_MFC };
  unsigned char WritePart2[] = { 0x10, DATA_WRITE_MFC };

  /* Authenticate */
  status = PN7150.NxpNci_ReaderTagCmd(Auth, sizeof(Auth), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Authenticate sector %d failed with error 0x%02x\n", Auth[1], Resp[RespSize - 1]);
    return;
  }
  printf(" Authenticate sector %d succeed\n", Auth[1]);

  /* Read block */
  status = PN7150.NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Read block %d failed with error 0x%02x\n", Read[2], Resp[RespSize - 1]);
    return;
  }
  printf(" Read block %d:", Read[2]); PRINT_BUF(" ", (Resp + 1), RespSize - 2);

  /* Write block */
  status = PN7150.NxpNci_ReaderTagCmd(WritePart1, sizeof(WritePart1), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Write block %d failed with error 0x%02x\n", WritePart1[2], Resp[RespSize - 1]);
    return;
  }
  status = PN7150.NxpNci_ReaderTagCmd(WritePart2, sizeof(WritePart2), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Write block %d failed with error 0x%02x\n", WritePart1[2], Resp[RespSize - 1]);
    return;
  }
  printf(" Block %d written\n", WritePart1[2]);

  /* Read block */
  status = PN7150.NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Read failed with error 0x%02x\n", Resp[RespSize - 1]);
    return;
  }
  printf(" Read block %d:", Read[2]); PRINT_BUF(" ", (Resp + 1), RespSize - 2);
}

void PCD_ISO15693_scenario(void)
{
#define BLK_NB_ISO15693     8
#define DATA_WRITE_ISO15693 0x11, 0x22, 0x33, 0x44

  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  unsigned char ReadBlock[] = { 0x02, 0x20, BLK_NB_ISO15693 };
  unsigned char WriteBlock[] = { 0x02, 0x21, BLK_NB_ISO15693, DATA_WRITE_ISO15693 };

  status = PN7150.NxpNci_ReaderTagCmd(ReadBlock, sizeof(ReadBlock), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x00)) {
    printf(" Read block %d failed with error 0x%02x\n", ReadBlock[2], Resp[RespSize - 1]);
    return;
  }
  printf(" Read block %d:", ReadBlock[2]); PRINT_BUF(" ", (Resp + 1), RespSize - 2);

  /* Write */
  status = PN7150.NxpNci_ReaderTagCmd(WriteBlock, sizeof(WriteBlock), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Write block %d failed with error 0x%02x\n", WriteBlock[2], Resp[RespSize - 1]);
    return;
  }
  printf(" Block %d written\n", WriteBlock[2]);

  /* Read back */
  status = PN7150.NxpNci_ReaderTagCmd(ReadBlock, sizeof(ReadBlock), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0x00)) {
    printf(" Read block %d failed with error 0x%02x\n", ReadBlock[2], Resp[RespSize - 1]);
    return;
  }
  printf(" Read block %d:", ReadBlock[2]); PRINT_BUF(" ", (Resp + 1), RespSize - 2);
}

void PCD_ISO14443_3A_scenario(void)
{
#define BLK_NB_ISO14443_3A      5
#define DATA_WRITE_ISO14443_3A  0x11, 0x22, 0x33, 0x44

  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  /* Read block */
  unsigned char Read[] = { 0x30, BLK_NB_ISO14443_3A };
  /* Write block */
  unsigned char Write[] = { 0xA2, BLK_NB_ISO14443_3A, DATA_WRITE_ISO14443_3A };

  /* Read */
  status = PN7150.NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Read block %d failed with error 0x%02x\n", Read[1], Resp[RespSize - 1]);
    return;
  }
  printf(" Read block %d:", Read[1]); PRINT_BUF(" ", Resp, 4);
  /* Write */
  status = PN7150.NxpNci_ReaderTagCmd(Write, sizeof(Write), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Write block %d failed with error 0x%02x\n", Write[1], Resp[RespSize - 1]);
    return;
  }
  printf(" Block %d written\n", Write[1]);

  /* Read back */
  status = PN7150.NxpNci_ReaderTagCmd(Read, sizeof(Read), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 1] != 0)) {
    printf(" Read block %d failed with error 0x%02x\n", Read[1], Resp[RespSize - 1]);
    return;
  }
  printf(" Read block %d:", Read[1]); PRINT_BUF(" ", Resp, 4);
}

void PCD_ISO14443_4_scenario(void)
{
  bool status;
  unsigned char Resp[256];
  unsigned char RespSize;
  unsigned char SelectPPSE[] = { 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 };

  status = PN7150.NxpNci_ReaderTagCmd(SelectPPSE, sizeof(SelectPPSE), Resp, &RespSize);
  if ((status == NFC_ERROR) || (Resp[RespSize - 2] != 0x90) || (Resp[RespSize - 1] != 0x00)) {
    printf(" Select PPSE failed with error %02x %02x\n", Resp[RespSize - 2], Resp[RespSize - 1]);
    return;
  }
  printf(" Select NDEF Application succeed\n");
}
#endif // ifdef RW_RAW_EXCHANGE

void task_nfc_reader(DFRobot_PN7150_I2C::NxpNci_RfIntf_t RfIntf)
{
  /* For each discovered cards */
  while (1) {

#ifndef RW_RAW_EXCHANGE
    /* Process NDEF message read */
    PN7150.NxpNci_ProcessReaderMode(RfIntf, PN7150.READ_NDEF);
#ifdef RW_NDEF_WRITING
    RW_NDEF_SetMessage((unsigned char*)NDEF_MESSAGE, sizeof(NDEF_MESSAGE), *NdefPush_Cb);
    /* Process NDEF message write */
    PN7150.NxpNci_ReaderReActivate(&RfIntf);
    PN7150.NxpNci_ProcessReaderMode(RfIntf, PN7150.WRITE_NDEF);
#endif // ifdef RW_NDEF_WRITING
#else // ifndef RW_RAW_EXCHANGE
    /* What's the detected card type ? */
    switch (RfIntf.Protocol) {
    case PROT_T2T:
      PCD_ISO14443_3A_scenario();
      break;
    case PROT_ISODEP:
      PCD_ISO14443_4_scenario();
      break;
    case PROT_T5T:
      PCD_ISO15693_scenario();
      break;
    case PROT_MIFARE:
      PCD_MIFARE_scenario();
      break;
    default:
      break;
    }
#endif // ifndef RW_RAW_EXCHANGE

    /* If more cards (or multi-protocol card) were discovered (only same technology are supported) select next one */
    if (RfIntf.MoreTags) {
      if (PN7150.NxpNci_ReaderActivateNext(&RfIntf) == NFC_ERROR) break;
    }
    /* Otherwise leave */
    else break;
  }

  /* Wait for card removal */
  PN7150.NxpNci_ProcessReaderMode(RfIntf, PN7150.PRESENCE_CHECK);

  Serial.print("CARD REMOVED\n");

  /* Restart discovery loop */
  PN7150.NxpNci_StopDiscovery();
  while (PN7150.NxpNci_StartDiscovery(DiscoveryTechnologies,sizeof(DiscoveryTechnologies)));
}

void setup()
{
  Serial.begin(115200);
  
  /* Register callback for reception of NDEF message from remote cards */
  RW_NDEF_RegisterPullCallback(*NdefPull_Cb);

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

  /* Reset reception index */
  gPendingReception = false;

  /* Wait until a peer is discovered */
  while (PN7150.NxpNci_WaitForDiscoveryNotification(&RfInterface) != NFC_SUCCESS);

  if ((RfInterface.ModeTech & MODE_MASK) == MODE_POLL) {
    task_nfc_reader(RfInterface);
  } else {
    printf("WRONG DISCOVERY\n");
  }
}
