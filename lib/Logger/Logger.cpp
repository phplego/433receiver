#include "Logger.h"


Logger::Logger() 
{

}

void Logger::log(String message)
{
    String prefix = String() + millis() / 1000 + "." + millis() % 1000 + " ";
    this->println(prefix + message);
}

void Logger::println(String message)
{
    this->print(message + "\n");
}

void Logger::print(String message)
{
    unsigned int shrinkSize = FILE_LOG_SIZE / 3;

    if(message.length() + 1 > shrinkSize) 
        return;

    if(this->position + message.length() + 1 >= FILE_LOG_SIZE){
        strcpy(this->buffer, this->buffer + shrinkSize);
        this->position = this->position - shrinkSize; 
    }

    int written = sprintf((char*)this->buffer + this->position, "%s", message.c_str());
    this->position += written;
}


