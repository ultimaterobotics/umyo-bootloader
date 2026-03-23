
//bootloader configuration file for simple adjustment of the same core for different pinouts of a particular board
//all defines are treated as variables, so all must be present in config

//device name during BLE bootloader advertisement
#define BLE_NAME_STRING "uMyo2 bootloader"
//button pin for triggering bootloading, -1 forces bootloader mode at start
#define CFG_BUTTON_PIN 19
//button pull: 0 - no pull, 0b01 - pulldown, 0b11 - pullup
#define CFG_BUTTON_PULL 0
//invert button behavior, normally button high = pressed
#define CFG_BUTTON_PRESSED_LOW 0
//system ON pin for power controller, -1 if none is present (old uECG board for instance)
#define CFG_SYSON_PIN 4
//LED pin for indicating bootloader state
#define CFG_LED_PIN 8
//default bootloader mode: 1 - BLE, 0 - direct radio
#define CFG_BOOTDEFAULT_BLE 0
//enables selection between BLE / direct modes with button press
#define CFG_BOOT_MODE_SELECT_ENABLE 1
//time the button must be held to enter bootloader, in milliseconds
#define CFG_BOOT_REQUEST_TIME 2000
//time for bootloading timeout if requested but sequence didn't start
#define CFG_BOOT_REQUEST_TIMEOUT 30000
//time for bootloading timeout if not completed for any unhandled reason
#define CFG_BOOTLOADING_TIMEOUT 60000*15


