
#include <Arduino.h>
#include <LittleFS.h>


#define LOGGER_BUFFER_SIZE 5000

class Logger
{
public:
    Logger();
    void init();
    void log_no_ln(String message);
    void log(String message);
    void log(String message, String message2);
    void print(String message);
    void ln();
    void println(String message);
    void flush();
    void loop();

    char buffer [LOGGER_BUFFER_SIZE + 1] = {0};
    bool persistant = true; // save log to file (every x seconds)
    String filename = "logger.txt";

protected:
    int position = 0;

};