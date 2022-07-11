/* MWT - Arduino API */
/* Designed by: jET, HHMI */
/* Author: Lakshmi Narayan PhD */
/* Principal Investigator: Marta Zlatic  */
/* Date created: January 31st 2018 */
/* Date last modified: January 31st 2018 */

int nByte, byte1, byte2, byte3, byte4, spineCount, i;

void setup() {
  Serial.setTimeout(100);
  Serial1.setTimeout(1);
  Serial.begin(115200);
  Serial1.begin(250000);
}

void loop() 
 {
  nByte = Serial1.available();
  if (nByte > 3)
  {
    byte1 = Serial1.read();
    byte2 = Serial1.read();
    byte3 = Serial1.read();
    byte4 = Serial1.read();
    spineCount = (byte1 * 256 * 256 * 256) + (byte2 * 256 * 256) + (byte3 * 256) + byte4;
    //spineCount = byte4; 
    if (spineCount > 0)
    {
      i=0;
      while(i < spineCount * 4)
      {
        if(Serial1.available()>0) 
        {
          Serial1.read();
          i++;
        }
      }
    }
    Serial.println(spineCount);
  }
}
