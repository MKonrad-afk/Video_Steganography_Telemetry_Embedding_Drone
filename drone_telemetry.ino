#include <math.h> 

// Earth's radius in meters 
#define R_EARTH 6371000.0

// Starting GPS coordinates (latitude, longitude) in decimal degrees for now static 
double lat = 52.2297; 
double lon = 21.0122;

// Simulation parameters
double speed_mps = 5;    
double bearing = 90.0;      

unsigned long lastUpdate = 0; 

double toRad(double deg){
  return deg * PI / 180.0;
}
double toDeg(double rad){
  return rad * 180.0 / PI;
}

// Move the current position along a great circle path
// distance: meters to move along the bearing
void movePosition(double distance) {
  double lat1 = toRad(lat);
  double lon1 = toRad(lon);
  double brng = toRad(bearing);

  double d = distance / R_EARTH;

  // Using the haversine formula / spherical trigonometry to calculate new position
  double lat2 = asin(
    sin(lat1)*cos(d) +
    cos(lat1)*sin(d)*cos(brng)
  );

  double lon2 = lon1 + atan2(
    sin(brng)*sin(d)*cos(lat1),
    cos(d)-sin(lat1)*sin(lat2)
  );

  lat = toDeg(lat2);
  lon = toDeg(lon2);
}

// Format latitude into NMEA ddmm.mmmm format with hemisphere (N/S)
void formatLat(double lat, char *out, char *hemi){
  if(lat >= 0) *hemi = 'N';
  else {
    *hemi = 'S';
    lat = -lat;
  }

  int deg = int(lat);
  double minutes = (lat - deg) * 60;

  char minStr[10];
  dtostrf(minutes, 6, 3, minStr); 
  sprintf(out, "%02d%s", deg, minStr);
}

// Format longitude into NMEA dddmm.mmmm format with hemisphere (E/W)
void formatLon(double lon, char *out, char *hemi){
  if(lon >= 0) *hemi = 'E';
  else {
    *hemi = 'W';
    lon = -lon;
  }

  int deg = int(lon);
  double minutes = (lon - deg) * 60;

  char minStr[10];
  dtostrf(minutes, 6, 3, minStr);
  sprintf(out, "%03d%s", deg, minStr);
}

// Calculate NMEA checksum for a sentence (XOR of all characters between $ and *)
byte checksum(char *str){
  byte cs = 0;
  for(int i=0; str[i]; i++)
    cs ^= str[i];
  return cs;
}

// Send a single NMEA sentence over Serial
// We are using NMEA 0183 format, which is ASCII text over serial
void sendNMEA(){
  char latStr[20];
  char lonStr[20];
  char latH, lonH;

  // Convert decimal coordinates into NMEA formatted strings
  formatLat(lat,latStr,&latH);
  formatLon(lon,lonStr,&lonH);

  // Convert speed from meters per second to knots (NMEA standard)
  double knots = speed_mps * 1.94384;

  char body[120];

  // Build the GPRMC sentence body
  // GPRMC = Recommended Minimum Navigation Information
  // Fields:
  // $GPRMC,<time>,<status>,<lat>,<N/S>,<lon>,<E/W>,<speed>,<course>,<date>,<magvar>,<magvar dir>,<mode>*<checksum>
  //status -> A=active, V=void
  char speedStr[10];
  char courseStr[10];

  dtostrf(knots, 5, 2, speedStr);  
  dtostrf(bearing, 5, 1, courseStr); 

  sprintf(body,
  "GPRMC,120000.00,A,%s,%c,%s,%c,%s,%s,091125,,,A",
  latStr,latH,lonStr,lonH,speedStr,courseStr);

  // Compute checksum for data integrity (required by NMEA 0183)
  byte cs = checksum(body);

  // Send full NMEA sentence over Serial
  Serial.print("$");
  Serial.print(body);
  Serial.print("*");
  if(cs < 16) Serial.print("0");
  Serial.println(cs,HEX);
}

void setup(){
  // 4800 baud (common for GPS modules using NMEA 0183)
  Serial.begin(4800);
}

void loop(){
  // Send GPS updates at 1 Hz
  if(millis() - lastUpdate >= 1000){
    lastUpdate = millis();

    movePosition(speed_mps); 
    sendNMEA();    
  }
}