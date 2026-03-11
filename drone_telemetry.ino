#include <Arduino.h>
#include <AESLib.h>
#include <math.h>

#define R_EARTH 6371000.0

// Drone simulation parameters
double lat = 52.2297;
double lon = 21.0122;
double speed_mps = 5.0;
double bearing = 90.0;
double altitude = 120.0;

unsigned long lastUpdate = 0;

// AES setup
AESLib aes;
byte aesKey[16]; // AES session key
byte aesIv[16];  // Initialization vector

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

// Generate AES key & IV randomly
void generateAESKey() {
  for(int i=0; i<16; i++){
    aesKey[i] = random(0, 256);
    aesIv[i] = random(0, 256);
  }
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
  randomSeed(analogRead(0));
  // Generate AES key & IV
  generateAESKey();
  Serial.println("Session key generated.");
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

    // Print original NMEA sentence
    Serial.print("Original NMEA: ");
    Serial.println(nmea);

    // Encrypt NMEA
    byte encrypted[128];
    int encLen = encryptNMEA(nmea, encrypted);

    // Print encrypted NMEA in hex
    Serial.print("Encrypted telemetry: ");
    for(int i=0;i<encLen;i++){
      if(encrypted[i]<16) Serial.print("0");
      Serial.print(encrypted[i], HEX);
    }
    Serial.println();
  }
}
