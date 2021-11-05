#include <GSM_GPRS.h>

namespace custom_libraries{

GSM_GPRS::GSM_GPRS(uint8_t RX_PIN, uint8_t TX_PIN):SoftwareSerial(RX_PIN,TX_PIN){
    begin(9600);
}

void GSM_GPRS::send_data(String url, String data){
    println("AT");
  delay(200);
 
  println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  Serial.println("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
  delay(200);
  ShowSerialData();
 
  println("AT+SAPBR=3,1,\"APN\",\"internet\"");//APN
  Serial.println("AT+SAPBR=3,1,\"APN\",\"internet\"");//APN
  delay(200);
  ShowSerialData();
 
  println("AT+SAPBR=1,1");
  Serial.println("AT+SAPBR=1,1");
  delay(200);
  ShowSerialData();
 
  println("AT+SAPBR=2,1");
  Serial.println("AT+SAPBR=2,1");
  delay(200);
  ShowSerialData();
 
 
  println("AT+HTTPINIT");
  Serial.println("AT+HTTPINIT");
  delay(200);
  ShowSerialData();
 
  println("AT+HTTPPARA=\"CID\",1");
  Serial.println("AT+HTTPPARA=\"CID\",1");
  delay(300);
  ShowSerialData();
 
 
  println("AT+HTTPPARA=\"URL\",\""+url+"\""); //Server address
  Serial.println("AT+HTTPPARA=\"URL\",\""+url+"\""); //Server address
  delay(200);
  ShowSerialData();
 
  println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  Serial.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(200);
  ShowSerialData();
 
 
  println("AT+HTTPDATA=" + String(data.length()) + ",100000");
  Serial.println("AT+HTTPDATA=" + String(data.length()) + ",100000");
  delay(200);
  ShowSerialData();
 
  println(data);
  Serial.println(data);
  delay(200);
  ShowSerialData();
 
  println("AT+HTTPACTION=1");
  Serial.println("AT+HTTPACTION=1");
  delay(200);
  ShowSerialData();
 
  println("AT+HTTPREAD");
  Serial.println("AT+HTTPREAD");
  delay(200);
  ShowSerialData();
 
  println("AT+HTTPTERM");
 Serial.println("AT+HTTPTERM");
  delay(200);
  ShowSerialData();
}
void GSM_GPRS::ShowSerialData()
{
  while(available() != 0)
    Serial.write(read());
  delay(1000);
 
}
}

