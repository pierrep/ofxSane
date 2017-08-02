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
    unsigned char* line;
    int size;
};

class ofxSane : public ofThread {
private:
    SANE_Handle handle;
    int totalDevices;
    int bufferSize;
    int pixelsPerLine;
    int totalLines;
    bool scanning;
    unsigned char* buffer;
public:
    ofxSane() {
        totalDevices = 0;
        bufferSize = 0;
        buffer = NULL;
        scanning = false;
        pixelsPerLine = 0;
        totalLines = 0;
    }
    ~ofxSane() {
        if(totalDevices > 0) {
            cout << "sane_close" << endl;
            sane_cancel(handle);
            sane_close(handle);
        }
        sane_exit();
        if(buffer != NULL)
            delete [] buffer;
    }
    void setup() {
        SANE_Int versionCode;
        cout << "sane_init: ";
        printStatus(sane_init(&versionCode, NULL));
        cout << "running SANE v" << SANE_VERSION_MAJOR(versionCode) << "." << SANE_VERSION_MINOR(versionCode) << endl;

        cout << "sane_get_devices: ";
        const SANE_Device** deviceList;
        printStatus(sane_get_devices(&deviceList, true));

        int curDevice;
        for(curDevice = 0; deviceList[curDevice] != NULL; curDevice++)
            printDevice(deviceList[curDevice]);
        totalDevices = curDevice;
        cout << totalDevices << " devices total" << endl;

        if(totalDevices > 0) {
            SANE_String_Const name = deviceList[0]->name;
            cout << "sane_open(" << name << "): ";
            printStatus(sane_open(name, &handle));

            // use sane_get_option_descriptor to get options
            int totalDescriptors;
            sane_control_option (handle, 0, SANE_ACTION_GET_VALUE, &totalDescriptors, 0);
            cout << totalDescriptors << " descriptors total:" << endl;
            for(int i = 0; i < totalDescriptors; i++) {
                const SANE_Option_Descriptor* cur = sane_get_option_descriptor(handle, i);
                cout << i << ": ";
                printDescriptor(cur);
            }

            SANE_Char mode[] = "Color";
            cout << "setting mode to " << mode << ": ";
            printStatus(sane_control_option(handle, 2, SANE_ACTION_SET_VALUE, &mode, NULL));

            SANE_Int dpi = 300;
            cout << "setting dpi to " << dpi << ": ";
            printStatus(sane_control_option(handle, 6, SANE_ACTION_SET_VALUE, &dpi, NULL));

            SANE_Int bitdepth = 8;
            cout << "setting bitdepth to " << bitdepth << ": ";
            printStatus(sane_control_option(handle, 5, SANE_ACTION_SET_VALUE, &bitdepth, NULL));

            SANE_Int xsize = 215;
            cout << "setting xsize to " << xsize << " mm" << ": ";
            xsize *= (1 << 16);
            printStatus(sane_control_option(handle, 10, SANE_ACTION_SET_VALUE, &xsize, NULL));
        }
    }
    void scan() {
        if(isThreadRunning())
            stopThread();
        if(totalDevices > 0)
            startThread(false, false);
    }
    void stop() {
        scanning = false;
    }
    ofEvent<lineEventArgs> lineEvent;
    int getBufferSize() {
        return bufferSize;
    }
    const unsigned char* getBuffer() {
        return buffer;
    }
    int getPixelsPerLine()
    {
        return pixelsPerLine;
    }
    int getTotalLines()
    {
        return totalLines;
    }

protected:
    void threadedFunction() {
        scanning = true;

        cout << "sane_start" << endl;
        printStatus(sane_start(handle));

//        int totalDescriptors;
//        sane_control_option (handle, 0, SANE_ACTION_GET_VALUE, &totalDescriptors, 0);
//        cout << totalDescriptors << " descriptors total:" << endl;
//        for(int i = 0; i < totalDescriptors; i++) {
//            const SANE_Option_Descriptor* cur = sane_get_option_descriptor(handle, i);
//            cout << i << ": ";
//            printDescriptor(cur);
//        }


        SANE_Parameters parameters;
        cout << "sane_get_parameters" << endl;
        printStatus(sane_get_parameters(handle, &parameters));

        cout << "depth: " << parameters.depth << endl;

        totalLines = parameters.lines;
        cout << "lines: " << parameters.lines << endl;

        pixelsPerLine = parameters.pixels_per_line;
        cout << "pixels per line: " << parameters.pixels_per_line << endl;

        bufferSize = parameters.bytes_per_line;
        SANE_Int size;

        cout << "bufferSize (bytes per line) = " << bufferSize << endl;

        if(buffer == NULL)
            buffer = new unsigned char[bufferSize];

        SANE_Status status = SANE_STATUS_GOOD;
        cout << "while(status == SANE_STATUS_GOOD)..." << endl;
        float start = ofGetElapsedTimef();
        float lines = 0;
        while(scanning && status == SANE_STATUS_GOOD) {
            status = sane_read(handle, buffer, bufferSize, &size);
            lineEventArgs eventArgs;
            eventArgs.line = buffer;
            eventArgs.size = size;
            ofNotifyEvent(lineEvent, eventArgs);
            lines++;
        }
        float stop = ofGetElapsedTimef();
        cout << start << " to " << stop << " with " << lines << " lines" << endl;
        float scanLineRate = (lines / (stop - start));
        cout <<  scanLineRate << " lps" << endl;

        cout << "final status is: ";
        printStatus(status);

        cout << "sane_cancel" << endl;
        sane_cancel(handle);
    }
};
