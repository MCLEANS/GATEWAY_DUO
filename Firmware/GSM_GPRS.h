#ifndef _GSM_GPRS_
#define _GSM_GPRS_

#include <Arduino.h>
#include "SoftwareSerial.h"
#include <string.h>

namespace custom_libraries{

class GSM_GPRS : public SoftwareSerial{
    private:
    private:
    public:
    public:
        GSM_GPRS(uint8_t RX_PIN,uint8_t TX_PIN);
        void send_data(String url,String data);
        void ShowSerialData();

};

}



#endif //_GSM_GPRS_