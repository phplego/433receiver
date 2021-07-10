#include "Logger.h"


Logger::Logger() 
{

}

void Logger::log_no_ln(String message)
{
    char buff [20];
    sprintf(buff, "%03d.%03d ", millis() / 1000, millis() % 1000);
    this->print(String() + buff + message);
}

void Logger::log(String message)
{
    this->log_no_ln(message + "\n");
}

void Logger::println(String message)
{
    this->print(message + "\n");
}

void Logger::print(String message)
{
    unsigned int shrinkSize = LOGGER_BUFFER_SIZE / 3;

    if(message.length() + 1 > shrinkSize) 
        return;

    if(this->position + message.length() + 1 >= LOGGER_BUFFER_SIZE){
        memmove(this->buffer, this->buffer + shrinkSize, LOGGER_BUFFER_SIZE - shrinkSize);
        //strcpy(this->buffer, this->buffer + shrinkSize);
        this->position = this->position - shrinkSize; 
    }

    int written = sprintf((char*)this->buffer + this->position, "%s", message.c_str());
    this->position += written;
}


