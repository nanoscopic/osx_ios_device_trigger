#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include<set>
std::set<UInt16> prodIds = {
    0x1290,0x1292,0x1294,0x1297,0x129a,0x129c,
    0x129f,0x12a0,0x12a2,0x12a3,0x12a4,0x12a5,
    0x12a6,0x12a8,0x12a9,0x12ab
};
#include<map>
const std::map<UInt16,const char *> prodMap = {
    {0x1290,"iPhone"},
    {0x1292,"iPhone 3G"},
    {0x1294,"iPhone 3GS"},
    {0x1297,"iPhone 4"},
    {0x129a,"iPad"},
    {0x129c,"iPhone 4 CDMA"},
    {0x129f,"iPad 2"},
    {0x12a0,"iPhone 4S"},
    {0x12a2,"iPad 2 3G"},
    {0x12a3,"iPad 2 CDMA"},
    {0x12a4,"iPad 3 wifi"},
    {0x12a5,"iPad 3 CDMA"},
    {0x12a6,"iPad 3 3G"},
    {0x12a8,"iPhone 5-8/X"},
    {0x12a9,"iPad 2"},
    {0x12ab,"iPad 4 Mini"}
};

typedef struct USBDeviceInfo {
    io_object_t             notification;
    IOUSBDeviceInterface942 **interface;
    IOUSBInterfaceInterface942 **iface2; // interface squared
    CFStringRef             name;
    UInt32                  locationID;
} USBDeviceInfo;

static IONotificationPortRef globalDevicePort;
static IONotificationPortRef globalInterfacePort;
static io_iterator_t         globalDeviceIter;
static io_iterator_t         globalInterfaceIter;
static bool gdone = false;

// Recieves kIOGeneralInterest notifications
void DeviceNotification(void *voidDevInfo, io_service_t service, natural_t messageType, void *messageArgument) {
    //kern_return_t kres;
    USBDeviceInfo *devInfo = (USBDeviceInfo *) voidDevInfo;
    if( messageType == kIOMessageServiceIsTerminated ) { // See IOMessage.h for other types
        fprintf(stderr, "Device removed\n");
        fprintf(stderr, "  Name: "); CFShow( devInfo->name );
        fprintf(stderr, "  LocationID: 0x%lx.\n\n", (unsigned long) devInfo->locationID);
        
        // Cleanup
        CFRelease( devInfo->name );
        if( devInfo->interface ) /*kr = */(*devInfo->interface)->Release( devInfo->interface );
        /*kres = */IOObjectRelease( devInfo->notification );
        free(devInfo);
    }
}

IOUSBDevRequest * magicControlRequest() {
    IOUSBDevRequest *req = (IOUSBDevRequest *) malloc( sizeof( IOUSBDevRequest ) );
    req->bmRequestType = 0x40;
    req->bRequest = 82;
    req->wValue = 0x0;
    req->wIndex = 0x2;
    req->wLength = 0x0;
    req->wLenDone = 0x0;
    req->pData = 0x00;
    return req;
}

void InterfaceAdded(void *refCon, io_iterator_t iterator) {
    kern_return_t kres;
    
    io_service_t  usbInterface;
    while( ( usbInterface = IOIteratorNext( iterator ) ) ) {
        //printf("Interface added\n");
        
        IOCFPlugInInterface **plugin = NULL;
        { // Get plugin interface to communicate to kernel
            SInt32 score;
            kres = IOCreatePlugInInterfaceForService( usbInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin, &score );
            if( ( kIOReturnSuccess != kres ) || !plugin ) {
                //fprintf( stderr, "IOCreatePlugInInterfaceForService returned 0x%08x.\n", kres );
                continue;
            }
        }
        //printf("  Got plugin\n");
        
        IOUSBInterfaceInterface942 **iface2; // interface squared
        {
            kres = (*plugin)->QueryInterface( plugin, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&iface2 );
            if( KERN_SUCCESS != kres ) { fprintf( stderr, "iface2 failed 0x%08x.\n", kres ); }
            else { /*printf("  Got iface2\n");*/ }
        }
        
        IOReturn kr;
        UInt16   devVendor = 0x0000;
        UInt16   devProduct = 0x0000;
        UInt8    intfClass = 0x00;
        UInt8    intfSubClass = 0x00;
        kr = (*iface2)->GetInterfaceClass(iface2, &intfClass);
        kr = (*iface2)->GetInterfaceSubClass(iface2, &intfSubClass);
        kr = (*iface2)->GetDeviceVendor(iface2, &devVendor);
        kr = (*iface2)->GetDeviceProduct(iface2, &devProduct);
        
        if( devVendor != 0x5ac || !prodIds.count( devProduct ) ) {
            (*plugin)->Release(plugin);
            continue;
        }
        printf("Interface added\n");
        printf("  class: %02x\n", intfClass);
        printf("  subclass: %02x\n", intfSubClass);
        printf("  Vendor: %04x\n", devVendor);
        printf("  Product: %04x\n", devProduct);
        
        if( !gdone && intfClass == 0x0a && intfSubClass == 0x00 ) {
            gdone = 1;
            if( KERN_SUCCESS != kres ) { fprintf( stderr, "open interface failed: 0x%08x.\n", kres ); }
            else {
                /*IOUSBDevRequest *req = magicControlRequest();
                
                (*iface2)->ControlRequest( iface2, 0x00, req );
                if( KERN_SUCCESS != kres ) { fprintf( stderr, "control request failed: 0x%08x.\n", kres ); }
                else {
                    printf("  Sent control request\n");
                    
                    io_service_t    USBdevice = 0;
                    IOCFPlugInInterface     **plugin;
                    IOUSBDeviceInterface942     **dev;
                    SInt32             score;
                    kr = (*iface2)->GetDevice(iface2, &USBdevice);
                    if( KERN_SUCCESS != kres ) { fprintf( stderr, "get usbdevice failed: 0x%08x.\n", kres ); }
                    
                    kr = IOCreatePlugInInterfaceForService(USBdevice,
                                                           kIOUSBDeviceUserClientTypeID,
                                                           kIOCFPlugInInterfaceID,
                                                           &plugin,
                                                           &score);
                    if( KERN_SUCCESS != kres ) { fprintf( stderr, "get plugin failed: 0x%08x.\n", kres ); }
                    
                    kr = (*plugin)->QueryInterface(plugin,
                                                   CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID942 ),
                                                   (LPVOID *) &dev);
                    if( KERN_SUCCESS != kres ) { fprintf( stderr, "get device interface failed: 0x%08x.\n", kres ); }
                    
                    (*plugin)->Release(plugin);
                    
                    { // Set configuration 5
                        UInt8 numConfig;
                        kres = (*dev)->GetNumberOfConfigurations( dev, &numConfig );
                        fprintf(stderr,"  Number of configs: %d\n", numConfig );
                        if( numConfig >= 5 ) {
                            IOUSBConfigurationDescriptorPtr configDesc;
                            kres = (*dev)->GetConfigurationDescriptorPtr(dev, 5, &configDesc);
                            
                            kres = (*dev)->SetConfiguration(dev, configDesc->bConfigurationValue);
                        }
                    }
                    (*dev)->Release(dev);
                }
                
                free( req->pData );
                free( req );*/
            }
        }
        
        // Release pluginInterface
        (*plugin)->Release(plugin);
    }
}

// Callback from IOServiceAddMatchingNotification
void DeviceAdded(void *refCon, io_iterator_t iterator) {
    kern_return_t kres;
    io_service_t  usbDevice;
    
    while( ( usbDevice = IOIteratorNext( iterator ) ) ) {
        // Create a blank device info struct
        USBDeviceInfo *devInfo = (USBDeviceInfo *) malloc( sizeof( USBDeviceInfo ) );
        bzero( devInfo, sizeof( USBDeviceInfo ) );
        
        IOCFPlugInInterface **plugin = NULL;
        { // Get plugin interface to communicate to kernel
            SInt32 score;
            kres = IOCreatePlugInInterfaceForService( usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugin, &score );
            if( ( kIOReturnSuccess != kres ) || !plugin ) {
                //fprintf( stderr, "IOCreatePlugInInterfaceForService returned 0x%08x.\n", kres );
                continue;
            }
        }
        
        // Get device interface; Can call routines in IOUSBLib.h against this
        HRESULT res = (*plugin)->QueryInterface( plugin, CFUUIDGetUUIDBytes( kIOUSBDeviceInterfaceID942 ), (LPVOID*) &devInfo->interface );
        
        IOUSBDeviceInterface942 **iface = devInfo->interface;
        IOReturn kr;
        UInt16   devVendor = 0x0000;
        UInt16   devProduct = 0x0000;
        UInt8    snsi = 0x00;
        kr = (*iface)->GetDeviceVendor(iface, &devVendor);
        kr = (*iface)->GetDeviceProduct(iface, &devProduct);
        
        if( devVendor != 0x5ac || !prodIds.count( devProduct ) ) {
            (*plugin)->Release(plugin);
            continue;
        }
        
        printf("Device added\n");
        
        { // Get deviceName
            io_name_t deviceName;
            kres = IORegistryEntryGetName( usbDevice, deviceName );
            if( KERN_SUCCESS != kres ) deviceName[0] = '\0';
            
            CFStringRef deviceNameStr = CFStringCreateWithCString( kCFAllocatorDefault, deviceName, kCFStringEncodingASCII );
            
            fprintf(stderr, "  Name: "); CFShow( deviceNameStr );
            
            devInfo->name = deviceNameStr;
        }
        
        { // Get USB serial number
            kr = (*iface)->USBGetSerialNumberStringIndex( iface, &snsi );
            if( kr != kIOReturnSuccess ) {
                fprintf( stderr, "Get Serial index 0x%08x.\n", kr );
            }
            else {
                IOUSBDevRequest req;
                UInt16 buffer[256];
                char serial[256];
                req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
                req.bRequest = kUSBRqGetDescriptor;
                req.wValue = (kUSBStringDesc << 8) | snsi;
                req.wIndex = 0x0409; //language ID (en-us) for serial number string
                req.pData = buffer;
                req.wLength = 256;//sizeof(buffer);
                kr = (*iface)->DeviceRequest(iface, &req);
                if( kr == kIOReturnSuccess && req.wLenDone > 0 ) {
                    int i, count;
                    // skip first word, and copy the rest to the serial string, changing shorts to bytes.
                    count = (req.wLenDone - 1) / 2;
                    for (i = 0; i < count; i++)
                        serial[i] = buffer[i + 1];
                    serial[i] = 0;
                    fprintf(stderr, "  Serial: %s\n", serial);
                }
            }
        }
        
        // Release pluginInterface
        (*plugin)->Release(plugin);
        
        auto item = prodMap.find( devProduct );
        const char *product = item->second;
        fprintf(stderr, "  Product: %s\n", product );
        
        if( res || devInfo->interface == NULL ) { fprintf(stderr, "Failed to get device interface; res = %d.\n", (int) res); continue; }
        
        { // Fetch the locationID ( identifies the device )
            UInt32 locationID;
            kres = (*devInfo->interface)->GetLocationID( devInfo->interface, &locationID );
            if( KERN_SUCCESS != kres ) { fprintf( stderr, "GetLocationID returned 0x%08x.\n", kres ); continue; }
            else fprintf( stderr, "  LocationID: 0x%lx\n\n", (unsigned long) locationID );
            
            devInfo->locationID = locationID;
        }
        
        // Subscribe to kIOGeneralInterest notifications, calling back DeviceNotification; Pass along devInfo
        kres = IOServiceAddInterestNotification(
                                                globalDevicePort,        // notifyPort
                                                usbDevice,               // notificationType
                                                kIOGeneralInterest,      // matching
                                                DeviceNotification,      // callback
                                                devInfo,                 // callback data
                                                &(devInfo->notification) // notification
                                                );
        if( KERN_SUCCESS != kres ) printf( "IOServiceAddInterestNotification returned 0x%08x.\n", kres );
        
        // Release iterator
        /*kres = */IOObjectRelease( usbDevice );
    }
}

void SignalHandler(int sigraised) { exit(0); }

int main( int argc, const char *argv[] ) {
    //long usbVendor  = 0x0000;
    //long usbProduct = 0x0000;
    
    // Handle ctrl-c
    sig_t oldHandler = signal( SIGINT, SignalHandler );
    if( oldHandler == SIG_ERR ) fprintf( stderr, "Couldn't setup signal handler" );
    
    CFMutableDictionaryRef deviceMatch = IOServiceMatching( kIOUSBDeviceClassName ); // Get IOUSBDevices
    if( deviceMatch == NULL ) { fprintf(stderr, "IOServiceMatching returned NULL\n"); return -1; }
    
    CFMutableDictionaryRef interfaceMatch = IOServiceMatching( kIOUSBInterfaceClassName ); // Get IOUSBInterfaces
    if( interfaceMatch == NULL ) { fprintf(stderr, "IOServiceMatching returned NULL\n"); return -1; }
    
    /*{ // Limit to a specific product/vendor
        CFNumberRef numberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor );
        CFDictionarySetValue( deviceMatch, CFSTR( kUSBVendorID ), numberRef );
        CFDictionarySetValue( interfaceMatch, CFSTR( kUSBVendorID ), numberRef );
        CFRelease( numberRef );
        numberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct );
        CFDictionarySetValue( deviceMatch, CFSTR( kUSBProductID ), numberRef );
        CFDictionarySetValue( interfaceMatch, CFSTR( kUSBProductID ), numberRef );
        CFRelease( numberRef );
    }*/
    
    globalDevicePort = IONotificationPortCreate( kIOMasterPortDefault ); // Create notification port
    globalInterfacePort = IONotificationPortCreate( kIOMasterPortDefault ); // Create notification port
    
    // Tie notification port to CFRunLoop
    CFRunLoopAddSource( CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource( globalDevicePort ), kCFRunLoopDefaultMode );
    CFRunLoopAddSource( CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource( globalInterfacePort ), kCFRunLoopDefaultMode );
    
    kern_return_t kres;
    
    // Subscribe to kIOFirstMatchNotification matching kIOUSBDeviceClassName, callback DeviceAdded
    kres = IOServiceAddMatchingNotification(
     globalDevicePort,          // notifyPort
     kIOFirstMatchNotification, // notificationType
     deviceMatch,               // matching
     DeviceAdded,               // callback
     NULL,                      // callback data
     &globalDeviceIter          // notification
     );
     if( KERN_SUCCESS != kres ) printf( "device match setup failed 0x%08x.\n", kres );
    
    kres = IOServiceAddMatchingNotification(
                                            globalInterfacePort,       // notifyPort
                                            kIOFirstMatchNotification, // notificationType
                                            interfaceMatch,            // matching
                                            InterfaceAdded,            // callback
                                            NULL,                      // callback data
                                            &globalInterfaceIter       // notification
                                            );
    if( KERN_SUCCESS != kres ) printf( "interface match setup failed 0x%08x.\n", kres );
    
    // Iterate once to get already-present devices and arm the notification
    InterfaceAdded( NULL, globalInterfaceIter );
    DeviceAdded( NULL, globalDeviceIter );
    
    CFRunLoopRun(); // Continues until interrupted by signal
    
    return 0;
}
