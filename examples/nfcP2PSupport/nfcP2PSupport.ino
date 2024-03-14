/*!
 * @file  nfcP2PSupport.ino
 * @brief example of communication with a remote peer transmitter
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @licence  The MIT License (MIT)
 * @author  [qsjhyy](yihuan.huang@dfrobot.com)
 * @version  V1.0
 * @date  2024-01-18
 * @url  https://github.com/DFRobot/DFRobot_PN7150
 */
#include "DFRobot_PN7150.h"

/* Discovery loop configuration according to the targeted modes of operation */
unsigned char DiscoveryTechnologies[] = {
    MODE_POLL | TECH_PASSIVE_NFCA,
    MODE_POLL | TECH_PASSIVE_NFCF,

    /* Only one POLL ACTIVE mode can be enabled, if both are defined only NFCF applies */
    MODE_POLL | TECH_ACTIVE_NFCA,
    //MODE_POLL | TECH_ACTIVE_NFCF,

    MODE_LISTEN | TECH_PASSIVE_NFCA,
    MODE_LISTEN | TECH_PASSIVE_NFCF,
    MODE_LISTEN | TECH_ACTIVE_NFCA,
    MODE_LISTEN | TECH_ACTIVE_NFCF,
};

/* Mode configuration according to the targeted modes of operation */
unsigned char mode = NXPNCI_MODE_P2P;

DFRobot_PN7150_I2C PN7150;
DFRobot_PN7150_I2C::NxpNci_RfIntf_t RfInterface;

#define PRINT_BUF(x,y,z)  {unsigned int loop; Serial.print(x); for(loop=0;loop<z;loop++) {Serial.print(y[loop], HEX); Serial.print(" ");} Serial.println();}

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

void setup()
{
  Serial.begin(115200);

  /* Register NDEF message to be sent to remote peer */
  P2P_NDEF_SetMessage((unsigned char*)NDEF_MESSAGE, sizeof(NDEF_MESSAGE), *NdefPush_Cb);
  /* Register callback for reception of NDEF message from remote peer */
  P2P_NDEF_RegisterPullCallback(*NdefPull_Cb);

  // Init the sensor
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

  /* Is P2P discovery ? */
  if (RfInterface.Interface == INTF_NFCDEP) {
    if ((RfInterface.ModeTech & MODE_LISTEN) == MODE_LISTEN) printf(" - P2P TARGET MODE: Activated from remote Initiator\n");
    else printf(" - P2P INITIATOR MODE: Remote Target activated\n");

    /* Process with SNEP for NDEF exchange */
    PN7150.NxpNci_ProcessP2pMode(RfInterface);

    printf("PEER LOST\n");
  } else {
    printf("WRONG DISCOVERY\n");
  }

}
