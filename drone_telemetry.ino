#include <Arduino.h>
#include <AESLib.h>
#include <math.h>
#include "uECC.h"

#define R_EARTH 6371000.0

// Drone simulation parameters
double lat = 52.2297;
double lon = 21.0122;
double speed_mps = 5.0;
double bearing = 90.0;
double altitude = 120.0;

unsigned long lastUpdate = 0;

AESLib aes;
byte aesKey[16];
byte aesIv[16]; 
const struct uECC_Curve_t * curve = uECC_secp256r1();

uint8_t privateKeyA[32], publicKeyA[64];
uint8_t privateKeyB[32], publicKeyB[64];

//for now hardcoded
uint8_t otherPublicKey[64] = {
  0x04,0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,
  0xF1,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
  0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
  0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00,
  0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,
  0x90,0xA0,0xB0,0xC0,0xD0,0xE0,0xF0,0x01,
  0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
  0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xAA,0xBB
};

double toRad(double deg) { return deg * PI / 180.0; }
double toDeg(double rad) { return rad * 180.0 / PI; }

void printHex(byte *data, int len) {
  for(int i = 0; i < len; i++){
    if(data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void movePosition(double distance) {
  double lat1 = toRad(lat);
  double lon1 = toRad(lon);
  double brng = toRad(bearing);
  double d = distance / R_EARTH;

  double lat2 = asin(sin(lat1)*cos(d) + cos(lat1)*sin(d)*cos(brng));
  double lon2 = lon1 + atan2(sin(brng)*sin(d)*cos(lat1), cos(d)-sin(lat1)*sin(lat2));

  lat = toDeg(lat2);
  lon = toDeg(lon2);
}
uint8_t secureRandom() {
  uint8_t r = 0;
  for(int i=0;i<8;i++){
    r <<= 1;
    r |= analogRead(A0) & 1;
  }
  return r;
}
void deriveAESFromECDH() {
  uint8_t sharedSecret[32];

  // Use privateKeyA (our local key) + otherPublicKey (remote key)
  if (!uECC_shared_secret(otherPublicKey, privateKeyA, sharedSecret, curve)) {
    Serial.println("ECDH failed!");
    return;
  }

  // Derive AES key from shared secret
  for (int i = 0; i < 16; i++) {
    aesKey[i] = sharedSecret[i] ^ sharedSecret[i + 16];
  }

  // Generate IV for AES
  for(int i=0;i<16;i++){
    aesIv[i] = secureRandom();
  }

  Serial.println("AES key derived from ECDH");
  Serial.print("AES Key: ");
  printHex(aesKey,16);

  Serial.print("IV: ");
  printHex(aesIv,16);
}

// Format latitude into NMEA ddmm.mmmm
void formatLat(double lat, char *out, char *hemi){
  if(lat >= 0) *hemi='N'; else { *hemi='S'; lat=-lat; }
  int deg=int(lat);
  double minutes=(lat-deg)*60;
  char minStr[10];
  dtostrf(minutes,6,3,minStr);
  sprintf(out,"%02d%s",deg,minStr);
}

// Format longitude into NMEA dddmm.mmmm
void formatLon(double lon, char *out, char *hemi){
  if(lon>=0)*hemi='E'; else { *hemi='W'; lon=-lon; }
  int deg=int(lon);
  double minutes=(lon-deg)*60;
  char minStr[10];
  dtostrf(minutes,6,3,minStr);
  sprintf(out,"%03d%s",deg,minStr);
}

// Encrypt NMEA sentence using AES-128-CBC
int encryptNMEA(char *nmea, byte *out){
  //input, length, output, key, bits, IV
  return aes.encrypt((byte *)nmea, strlen(nmea), out, aesKey, 128, aesIv);
}


void setup() {
  Serial.begin(115200);

  // Generate both keypairs
  uECC_make_key(publicKeyA, privateKeyA, curve);
  uECC_make_key(publicKeyB, privateKeyB, curve);

  // Use B as "hardcoded remote"
  for(int i=0;i<64;i++) otherPublicKey[i] = publicKeyB[i];

  // Derive AES key
  deriveAESFromECDH();
}
void loop() {
  if(millis() - lastUpdate >= 1000){
    lastUpdate = millis();

    movePosition(speed_mps);

    // Build NMEA sentence
    char latStr[20], lonStr[20];
    char latH, lonH;
    formatLat(lat, latStr, &latH);
    formatLon(lon, lonStr, &lonH);

    char speedStr[10], courseStr[10];
    dtostrf(speed_mps*1.94384,5,2,speedStr);
    dtostrf(bearing,5,1,courseStr);

    char nmea[120];
    sprintf(nmea,"$GPRMC,120000.00,A,%s,%c,%s,%c,%s,%s,091125,,,A",
            latStr,latH,lonStr,lonH,speedStr,courseStr);

    // // Print original NMEA sentence
    // Serial.print("Original NMEA: ");
    // Serial.println(nmea);

    // Encrypt NMEA
    byte encrypted[128];
    int encLen = encryptNMEA(nmea, encrypted);

    // Print encrypted NMEA in hex
    for(int i=0;i<encLen;i++){
      if(encrypted[i]<16) Serial.print("0");
      Serial.print(encrypted[i], HEX);
    }
    Serial.println();
  }
}
