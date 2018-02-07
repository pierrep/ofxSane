#include "ofxSane.h"

void printStatus(SANE_Status status) {
    ofLogVerbose() << "status: " << sane_strstatus(status);
}

void printDevice(const SANE_Device* device) {
    ofLogVerbose() << "device:" <<
        "\tname = " << device->name <<
        "\tvendor = " << device->vendor <<
        "\tmodel = " << device->model <<
        "\ttype = " << device->type;
}

string typeToString(const SANE_Value_Type& type) {
    switch(type) {
        case SANE_TYPE_BOOL: return "bool";
        case SANE_TYPE_INT: return "int";
        case SANE_TYPE_FIXED: return "fixed";
        case SANE_TYPE_STRING: return "string";
        case SANE_TYPE_BUTTON: return "button";
        case SANE_TYPE_GROUP: return "group";
        default: return "?";
    }
}

string unitToString(const SANE_Unit& unit) {
    switch(unit) {
        case SANE_UNIT_BIT: return "bits";
        case SANE_UNIT_DPI: return "dpi";
        case SANE_UNIT_MICROSECOND: return "microseconds";
        case SANE_UNIT_MM: return "millimeters";
        case SANE_UNIT_NONE: return "unitless";
        case SANE_UNIT_PERCENT: return "percent";
        case SANE_UNIT_PIXEL: return "pixel";
        default: return "?";
    }
}

void printDescriptor(const SANE_Option_Descriptor* descriptor) {
    ofLogVerbose() << descriptor->title << " " <<
        "(" << typeToString(descriptor->type) <<
        "/" << unitToString(descriptor->unit) << ")" <<
        "[" << descriptor->size << "] ";
    switch(descriptor->constraint_type) {
        case SANE_CONSTRAINT_NONE:
            ofLogVerbose() << "any value";
            break;
        case SANE_CONSTRAINT_STRING_LIST:
            ofLogVerbose() << "{string list...}";
            break;
        case SANE_CONSTRAINT_WORD_LIST:
            ofLogVerbose() << "{word list...}";
            break;
        case SANE_CONSTRAINT_RANGE:
            const SANE_Range* range = descriptor->constraint.range;
            ofLogVerbose() << range->min << " to " << range->max;
            if(range->quant != 0)
                ofLogVerbose() << " in " << range->quant;
            break;
    }
    ofLogVerbose();
}

//--------------------------------------------------------------------
ofxSane::ofxSane()
{
    totalDevices = 0;
    bufferSize = 0;
    buffer = NULL;
    scanning = false;
    pixelsPerLine = 0;
    totalLines = 0;
}

//--------------------------------------------------------------------
ofxSane::~ofxSane() {
    if(totalDevices > 0) {
        ofLogNotice() << "closing SANE";
        sane_cancel(handle);
        sane_close(handle);
    }

    sane_exit();

    if(buffer != NULL)
        delete [] buffer;
}

//--------------------------------------------------------------------
void ofxSane::setup() {
    SANE_Int versionCode;

    ofLogNotice() << "Initializing SANE: ";
    printStatus(sane_init(&versionCode, NULL));
    ofLogNotice() << "running SANE v" << SANE_VERSION_MAJOR(versionCode) << "." << SANE_VERSION_MINOR(versionCode);

    ofLogVerbose() << "sane_get_devices: ";
    const SANE_Device** deviceList;
    printStatus(sane_get_devices(&deviceList, true));

    int curDevice;
    for(curDevice = 0; deviceList[curDevice] != NULL; curDevice++) {
        printDevice(deviceList[curDevice]);
    }
    totalDevices = curDevice;
    ofLogNotice() << "Found " << totalDevices << " devices total";

    if(totalDevices > 0) {
        SANE_String_Const name = deviceList[0]->name;
        ofLogVerbose() << "sane_open(" << name << "): ";
        printStatus(sane_open(name, &handle));

        // use sane_get_option_descriptor to get options
        int totalDescriptors;
        sane_control_option (handle, 0, SANE_ACTION_GET_VALUE, &totalDescriptors, 0);
        ofLogVerbose() << totalDescriptors << " descriptors total:";
        for(int i = 0; i < totalDescriptors; i++) {
            const SANE_Option_Descriptor* cur = sane_get_option_descriptor(handle, i);
            ofLogVerbose() << i << ": ";
            printDescriptor(cur);
        }

        SANE_Char mode[] = "Color";
        ofLogVerbose() << "setting mode to " << mode << ": ";
        printStatus(sane_control_option(handle, 2, SANE_ACTION_SET_VALUE, &mode, NULL));

        SANE_Int dpi = 300;
        ofLogVerbose() << "setting dpi to " << dpi << ": ";
        printStatus(sane_control_option(handle, 6, SANE_ACTION_SET_VALUE, &dpi, NULL));

        SANE_Int bitdepth = 8;
        ofLogVerbose() << "setting bitdepth to " << bitdepth << ": ";
        printStatus(sane_control_option(handle, 5, SANE_ACTION_SET_VALUE, &bitdepth, NULL));

        SANE_Int xsize = 215;
        ofLogVerbose() << "setting xsize to " << xsize << " mm" << ": ";
        xsize *= (1 << 16);
        printStatus(sane_control_option(handle, 10, SANE_ACTION_SET_VALUE, &xsize, NULL));
    }
}

//--------------------------------------------------------------------
void ofxSane::scan() {
    if(isThreadRunning())
        stopThread();
    if(totalDevices > 0)
        startThread(false);
}

//--------------------------------------------------------------------
void ofxSane::threadedFunction()
{
    scanning = true;

    ofLogNotice() << "Starting scan...";
    printStatus(sane_start(handle));

    SANE_Parameters parameters;
    printStatus(sane_get_parameters(handle, &parameters));

    ofLogVerbose() << "depth: " << parameters.depth;

    totalLines = parameters.lines;
    ofLogVerbose() << "lines: " << parameters.lines;

    pixelsPerLine = parameters.pixels_per_line;
    ofLogVerbose() << "pixels per line: " << parameters.pixels_per_line;

    bufferSize = parameters.bytes_per_line;
    SANE_Int size;

    ofLogVerbose() << "bufferSize (bytes per line) = " << bufferSize;

    if(buffer == NULL)
        buffer = new unsigned char[bufferSize];

    SANE_Status status = SANE_STATUS_GOOD;

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
    ofLogVerbose() << start << " to " << stop << " with " << lines << " lines";
    float scanLineRate = (lines / (stop - start));
    ofLogVerbose() <<  scanLineRate << " lps";

    ofLogVerbose() << "final status is: ";
    printStatus(status);

    sane_cancel(handle);
    scanning = false;

   ofNotifyEvent(scanCompleteEvent,scanning);
}

//--------------------------------------------------------------------
void ofxSane::stop() {
    scanning = false;
}

//--------------------------------------------------------------------
int ofxSane::getBufferSize() {
    return bufferSize;
}

//--------------------------------------------------------------------
const unsigned char* ofxSane::getBuffer() {
    return buffer;
}

//--------------------------------------------------------------------
size_t ofxSane::getPixelsPerLine()
{
    return static_cast<size_t>(pixelsPerLine);
}

//--------------------------------------------------------------------
size_t ofxSane::getTotalLines()
{
    return static_cast<size_t>(totalLines);
}
