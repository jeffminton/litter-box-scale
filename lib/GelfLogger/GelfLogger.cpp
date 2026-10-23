#include "GelfLogger.h"


GelfLogger::GelfLogger()
    : graylog_address(const_cast<char*>(GRAYLOG_ADDRESS))
    , transport(UDPTransport(graylog_address, 12201))
    , publisher(Publisher(&transport))
    , message(Message(""))
{
    // graylog_address = GRAYLOG_ADDRESS;

    // transport = UDPTransport(graylog_address, 12201);
    // publisher = Publisher(&transport);

    // We will use this just to avoid creating many messages in our functions
    // message = Message("");
    
}

// int GelfLogger::log_to_gelf(const char* short_message) {
//     message.reset(short_message);
//     message.set("host", host_name);
//     message.set("millis", millis());
//     publisher.publish(&message);
//     return 0;
// }

// Print func
size_t GelfLogger::write(uint8_t m) {
    return write(&m, 1);
}

// Println / Printf / Write func
size_t GelfLogger::write(const uint8_t* buffer, size_t size) {
  Serial.printf("GelfLogger write Start\n");
  if (buffer == nullptr || size == 0)
    return 0;

  char* null_terminated_buffer = new char[size + 1];
  memcpy(null_terminated_buffer, buffer, size);
  null_terminated_buffer[size] = '\0';

  const char* message_text = null_terminated_buffer;
  message.reset(message_text);
  message.set("host", host_name);
  message.set("millis", millis());
  Serial.printf("GelfLogger write Before Publish host_name: %s, message: %s\n", host_name, message_text);
  publisher.publish(&message);
  Serial.printf("GelfLogger write After Publish\n");

  delete[] null_terminated_buffer;
  Serial.printf("GelfLogger write Done\n");
  return size;
}

void GelfLogger::set_hostname(char* host_name_in) {
  host_name = host_name_in;
}

GelfLogger GraylogLogger;
