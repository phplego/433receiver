
#include <Arduino.h>
#include <FS.h>

#define FILE_LOG_SIZE 10000

class Logger
{
public:
    Logger();
    void log(String message);
    void print(String message);
    void println(String message);


    char buffer [FILE_LOG_SIZE] = {0};

protected:
    int position = 0;
};