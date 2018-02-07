#pragma once

#include "sane/sane.h"
#include "ofMain.h"

void printStatus(SANE_Status status);
void printDevice(const SANE_Device* device);
string typeToString(const SANE_Value_Type& type);
string unitToString(const SANE_Unit& unit);
void printDescriptor(const SANE_Option_Descriptor* descriptor);

class lineEventArgs : public ofEventArgs {
public:
    size_t size;
    unsigned char* line;
};

class ofxSane : public ofThread {

public:
    ofxSane();
    ~ofxSane();
    void setup();
    void scan();
    void stop();
    int getBufferSize();
    const unsigned char* getBuffer();
    size_t getPixelsPerLine();
    size_t getTotalLines();

    ofEvent<lineEventArgs> lineEvent;
    ofEvent<bool> scanCompleteEvent;

protected:
    void threadedFunction();

private:
    SANE_Handle handle;
    int totalDevices;
    int bufferSize;
    int pixelsPerLine;
    int totalLines;
    bool scanning;
    unsigned char* buffer;
};
