//Data elements for EMV transactions.
#ifndef __EMVDATAELS_H
#define __EMVDATAELS_H

#include <stdint.h>

//Known AIDs
typedef struct {
	uint8_t tag[2];
	char description[255];
}  __attribute__((__packed__)) tagdescription_t;

//Known AIDs
extern const uint8_t AID_VISA[5];
extern const uint8_t AID_VISA_DB[7];
extern const uint8_t AID_VISA_C[8]; 
extern const uint8_t AID_VISA_D[8];
extern const uint8_t AID_VISA_E[7];
extern const uint8_t AID_VISA_I[7];
extern const uint8_t AID_VISA_P[7];
extern const uint8_t AID_VISA_ATM[8];
extern const uint8_t AID_MASTERCARD[7];
extern const uint8_t AID_MAESTRO[7];
extern const uint8_t AID_MAESTRO_UK[7];
extern const uint8_t AID_MAESTRO_TEST[5];
extern const uint8_t AID_SELF_SERVICE[6];
extern const uint8_t AID_AMEX[5];      
extern const uint8_t AID_EXPRESSPAY[8];
extern const uint8_t AID_LINK[7];      
extern const uint8_t AID_ALIAS[7];     

//Master data file for PSE
extern const uint8_t DF_PSE[14];
extern const uint8_t SW1_RESPONSE_BYTES[1]; 
extern const uint8_t SW1_WRONG_LENGTH[1]; 
extern const uint8_t SW12_OK[2]; 
extern const uint8_t SW12_NOT_SUPPORTED[2];
extern const uint8_t SW12_NOT_FOUND[2]; 
extern const uint8_t SW12_COND_NOT_SAT[2]; 
extern const uint8_t PIN_BLOCKED[2];
extern const uint8_t PIN_BLOCKED2[2];
extern const uint8_t PIN_WRONG[1];

extern const tagdescription_t EMV_TAG_LIST[62]; //SW1 return values

//AIP bitmasks details
#define AIP_CHIP_SUPPORTED 0x80
#define AIP_MSR_SUPPORTED 0x40

#define AIP_SDA_SUPPORTED 0x40
#define AIP_DDA_SUPPORTED 0x20
#define AIP_CARDHOLDER_VERIFICATION 0x10
#define AIP_TERMINAL_RISK 0x08
#define AIP_ISSUER_AUTH 0x04
#define AIP_CDA_SUPPORTED 0x01
 
//human readable error messages
//file structure, read from AFL
#endif //__EMVDATAELS_H
