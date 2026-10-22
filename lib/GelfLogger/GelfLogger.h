#ifndef GELF_LOGGER_H
#define GELF_LOGGER_H

#include <UDPTransport.h>
#include <Message.h>
#include <Publisher.h>

#include <Secrets.h>



class GelfLogger : public Print  {
private:
    char* graylog_address;
    // UDPTransport transport = UDPTransport("log.mysite.net", 12201);
    UDPTransport transport;
    Publisher publisher;

    Message message;
    

    
public:
    GelfLogger();

    // void log_to_gelf(const char* short_message);

    size_t write(uint8_t) override;
    size_t write(const uint8_t* buffer, size_t size) override;

};

extern GelfLogger GraylogLogger;


#endif // LOAD_SENSOR_STABILIZER_H
