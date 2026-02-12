#pragma comment(lib, "nvapi64.lib")
#pragma comment(lib, "user32.lib")

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include "nvapi.h"
#include "adl_sdk.h"


// ============================================================
// NVIDIA Backend
// ============================================================

// This function calculates the (XOR) checksum of the I2C register
void CalculateI2cChecksum(const NV_I2C_INFO& i2cInfo)
{
    // Calculate the i2c packet checksum and place the
    // value into the packet

    // i2c checksum is the result of xor'ing all the bytes in the
    // packet (except for the last data byte, which is the checksum
    // itself)

    // Packet consists of:

    // The device address...
    BYTE checksum = i2cInfo.i2cDevAddress;

    // Register address...
    for (unsigned int i = 0; i < i2cInfo.regAddrSize; ++i)
    {
        checksum ^= i2cInfo.pbI2cRegAddress[i];
    }

    // And data bytes less last byte for checksum...
    for (unsigned int i = 0; i < i2cInfo.cbSize - 1; ++i)
    {
        checksum ^= i2cInfo.pbData[i];
    }

    // Store calculated checksum in the last byte of i2c packet
    i2cInfo.pbData[i2cInfo.cbSize - 1] = checksum;
}

// This macro initializes the i2cinfo structure
#define  INIT_I2CINFO(i2cInfo, i2cVersion, displayId, isDDCPort,   \
        i2cDevAddr, regAddr, regSize, dataBuf, bufSize, speed)     \
do {                                                               \
    i2cInfo.version         = i2cVersion;                          \
    i2cInfo.displayMask     = displayId;                           \
    i2cInfo.bIsDDCPort      = isDDCPort;                           \
    i2cInfo.i2cDevAddress   = i2cDevAddr;                          \
    i2cInfo.pbI2cRegAddress = (BYTE*) &regAddr;                    \
    i2cInfo.regAddrSize     = regSize;                             \
    i2cInfo.pbData          = (BYTE*) &dataBuf;                    \
    i2cInfo.cbSize          = bufSize;                             \
    i2cInfo.i2cSpeed        = speed;                               \
}while (0)

// This function writes the input_value to the display over the I2C bus by issuing commands and data
BOOL WriteValueToMonitor(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId,BYTE input_value, BYTE command_code, BYTE register_address)
{
    NvAPI_Status nvapiStatus = NVAPI_OK;

    NV_I2C_INFO i2cInfo = { 0 };
    i2cInfo.version = NV_I2C_INFO_VER;
    //
    // The 7-bit I2C address for display = Ox37
    // Since we always use 8bits to address, this 7-bit addr (0x37) is placed on
    // the upper 7 bits, and the LSB contains the Read/Write flag:
    // Write = 0 and Read =1;
    //
    NvU8 i2cDeviceAddr = 0x37;
    NvU8 i2cWriteDeviceAddr = i2cDeviceAddr << 1; //0x6E
    NvU8 i2cReadDeviceAddr = i2cWriteDeviceAddr | 1; //0x6F


    //
    // Now Send a write packet to modify current brightness value to 20 (0x14)
    // The packet consists of the following bytes
    // 0x6E - i2cWriteDeviceAddr
    // Ox?? - register_address
    // 0x84 - 0x80 OR n where n = 4 bytes for "modify a value" request
    // 0x03 - change a value flag
    // 0x?? - command_code
    // Ox00 - input_value high byte
    // 0x?? - input_value low byte
    // 0x?? - checksum, , xor'ing all the above bytes
    //
    BYTE registerAddr[] = { register_address };
    BYTE modifyBytes[] = { 0x84, 0x03, command_code, 0x00, input_value, 0xDD };

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cWriteDeviceAddr,
        registerAddr, sizeof(registerAddr), modifyBytes, sizeof(modifyBytes), 27);
    CalculateI2cChecksum(i2cInfo);

    nvapiStatus = NvAPI_I2CWrite(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CWrite (revise brightness) failed with status %d\n", nvapiStatus);
        return FALSE;
    }

    return TRUE;
}

bool InitNvidia()
{
    NvAPI_Status status = NvAPI_Initialize();
    return (status == NVAPI_OK);
}

bool NvidiaWriteValue(int display_index, BYTE input_value, BYTE command_code, BYTE register_address)
{
    NvAPI_Status nvapiStatus = NVAPI_OK;

    // Enumerate display handles
    NvDisplayHandle hDisplay_a[NVAPI_MAX_PHYSICAL_GPUS * NVAPI_MAX_DISPLAY_HEADS] = { 0 };
    for (unsigned int i = 0; nvapiStatus == NVAPI_OK; i++)
    {
        nvapiStatus = NvAPI_EnumNvidiaDisplayHandle(i, &hDisplay_a[i]);

        if (nvapiStatus != NVAPI_OK && nvapiStatus != NVAPI_END_ENUMERATION)
        {
            printf("NvAPI_EnumNvidiaDisplayHandle() failed with status %d\n", nvapiStatus);
            return false;
        }
    }

    // Get GPU associated with display
    NvPhysicalGpuHandle hGpu = NULL;
    NvU32 pGpuCount = 0;
    nvapiStatus = NvAPI_GetPhysicalGPUsFromDisplay(hDisplay_a[display_index], &hGpu, &pGpuCount);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("NvAPI_GetPhysicalGPUFromDisplay() failed with status %d\n", nvapiStatus);
        return false;
    }

    // Get the display id for I2C calls
    NvU32 outputID = 0;
    nvapiStatus = NvAPI_GetAssociatedDisplayOutputId(hDisplay_a[display_index], &outputID);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("NvAPI_GetAssociatedDisplayOutputId() failed with status %d\n", nvapiStatus);
        return false;
    }

    BOOL result = WriteValueToMonitor(hGpu, outputID, input_value, command_code, register_address);
    return (result == TRUE);
}


// ============================================================
// AMD ADL Backend
// ============================================================

// ADL function pointer typedefs
typedef int (*ADL_MAIN_CONTROL_CREATE_FUNC)(ADL_MAIN_MALLOC_CALLBACK, int);
typedef int (*ADL_MAIN_CONTROL_DESTROY_FUNC)();
typedef int (*ADL_ADAPTER_NUMBEROFADAPTERS_GET_FUNC)(int*);
typedef int (*ADL_ADAPTER_ADAPTERINFO_GET_FUNC)(LPAdapterInfo, int);
typedef int (*ADL_DISPLAY_DISPLAYINFO_GET_FUNC)(int, int*, ADLDisplayInfo**, int);
typedef int (*ADL_DISPLAY_DDCBLOCKACCESS_GET_FUNC)(int iAdapterIndex, int iDisplayIndex, int iOption, int iCommandIndex, int iSendMsgLen, char* lpucSendMsgBuf, int* lpulRecvMsgLen, char* lpucRecvMsgBuf);

// ADL global state
static HMODULE hADLModule = NULL;
static ADL_MAIN_CONTROL_CREATE_FUNC         pfn_ADL_Main_Control_Create = NULL;
static ADL_MAIN_CONTROL_DESTROY_FUNC        pfn_ADL_Main_Control_Destroy = NULL;
static ADL_ADAPTER_NUMBEROFADAPTERS_GET_FUNC pfn_ADL_Adapter_NumberOfAdapters_Get = NULL;
static ADL_ADAPTER_ADAPTERINFO_GET_FUNC     pfn_ADL_Adapter_AdapterInfo_Get = NULL;
static ADL_DISPLAY_DISPLAYINFO_GET_FUNC     pfn_ADL_Display_DisplayInfo_Get = NULL;
static ADL_DISPLAY_DDCBLOCKACCESS_GET_FUNC  pfn_ADL_Display_DDCBlockAccess_Get = NULL;

// ADL memory allocation callback (required by ADL)
void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
    return malloc(iSize);
}

void __stdcall ADL_Main_Memory_Free(void** lpBuffer)
{
    if (NULL != *lpBuffer)
    {
        free(*lpBuffer);
        *lpBuffer = NULL;
    }
}

bool InitADL()
{
    hADLModule = LoadLibrary(_T("atiadlxx.dll"));
    // A 32 bit calling application on 64 bit OS will fail to LoadLibrary.
    // Try to load the 32 bit library (atiadlxy.dll) instead
    if (hADLModule == NULL)
        hADLModule = LoadLibrary(_T("atiadlxy.dll"));

    if (hADLModule == NULL)
        return false;

    pfn_ADL_Main_Control_Create = (ADL_MAIN_CONTROL_CREATE_FUNC)GetProcAddress(hADLModule, "ADL_Main_Control_Create");
    pfn_ADL_Main_Control_Destroy = (ADL_MAIN_CONTROL_DESTROY_FUNC)GetProcAddress(hADLModule, "ADL_Main_Control_Destroy");
    pfn_ADL_Adapter_NumberOfAdapters_Get = (ADL_ADAPTER_NUMBEROFADAPTERS_GET_FUNC)GetProcAddress(hADLModule, "ADL_Adapter_NumberOfAdapters_Get");
    pfn_ADL_Adapter_AdapterInfo_Get = (ADL_ADAPTER_ADAPTERINFO_GET_FUNC)GetProcAddress(hADLModule, "ADL_Adapter_AdapterInfo_Get");
    pfn_ADL_Display_DisplayInfo_Get = (ADL_DISPLAY_DISPLAYINFO_GET_FUNC)GetProcAddress(hADLModule, "ADL_Display_DisplayInfo_Get");
    pfn_ADL_Display_DDCBlockAccess_Get = (ADL_DISPLAY_DDCBLOCKACCESS_GET_FUNC)GetProcAddress(hADLModule, "ADL_Display_DDCBlockAccess_Get");

    if (pfn_ADL_Main_Control_Create == NULL ||
        pfn_ADL_Main_Control_Destroy == NULL ||
        pfn_ADL_Adapter_NumberOfAdapters_Get == NULL ||
        pfn_ADL_Adapter_AdapterInfo_Get == NULL ||
        pfn_ADL_Display_DisplayInfo_Get == NULL ||
        pfn_ADL_Display_DDCBlockAccess_Get == NULL)
    {
        FreeLibrary(hADLModule);
        hADLModule = NULL;
        return false;
    }

    // Initialize ADL. Second parameter 1 = only active adapters.
    int adlResult = pfn_ADL_Main_Control_Create(ADL_Main_Memory_Alloc, 1);
    if (adlResult != ADL_OK)
    {
        FreeLibrary(hADLModule);
        hADLModule = NULL;
        return false;
    }

    return true;
}

void FreeADL()
{
    if (pfn_ADL_Main_Control_Destroy)
        pfn_ADL_Main_Control_Destroy();
    if (hADLModule)
    {
        FreeLibrary(hADLModule);
        hADLModule = NULL;
    }
}

bool ADLWriteValue(int display_index, BYTE input_value, BYTE command_code, BYTE register_address)
{
    // Get number of adapters
    int iNumberAdapters = 0;
    if (pfn_ADL_Adapter_NumberOfAdapters_Get(&iNumberAdapters) != ADL_OK || iNumberAdapters <= 0)
    {
        printf("No AMD adapters found\n");
        return false;
    }

    // Get adapter info
    LPAdapterInfo lpAdapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo) * iNumberAdapters);
    if (lpAdapterInfo == NULL)
    {
        printf("Memory allocation failed\n");
        return false;
    }
    memset(lpAdapterInfo, 0, sizeof(AdapterInfo) * iNumberAdapters);
    pfn_ADL_Adapter_AdapterInfo_Get(lpAdapterInfo, sizeof(AdapterInfo) * iNumberAdapters);

    // Build flat list of connected+mapped displays
    int targetAdapterIdx = -1;
    int targetDisplayIdx = -1;
    int flatIndex = 0;

    for (int i = 0; i < iNumberAdapters; i++)
    {
        int iAdapterIndex = lpAdapterInfo[i].iAdapterIndex;
        int iNumberDisplays = 0;
        ADLDisplayInfo* lpDisplayInfo = NULL;

        if (pfn_ADL_Display_DisplayInfo_Get(iAdapterIndex, &iNumberDisplays, &lpDisplayInfo, 0) != ADL_OK)
            continue;

        for (int j = 0; j < iNumberDisplays; j++)
        {
            // Only use connected AND mapped displays
            if ((lpDisplayInfo[j].iDisplayInfoValue &
                (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED)) !=
                (ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED))
                continue;

            // Is the display mapped to this adapter?
            if (iAdapterIndex != lpDisplayInfo[j].displayID.iDisplayLogicalAdapterIndex)
                continue;

            if (flatIndex == display_index)
            {
                targetAdapterIdx = iAdapterIndex;
                targetDisplayIdx = lpDisplayInfo[j].displayID.iDisplayLogicalIndex;
            }
            flatIndex++;
        }

        ADL_Main_Memory_Free((void**)&lpDisplayInfo);
    }

    free(lpAdapterInfo);

    if (targetAdapterIdx == -1)
    {
        printf("Display index %d not found (only %d AMD displays detected)\n", display_index, flatIndex);
        return false;
    }

    // Build DDC/CI packet (identical format to NVAPI)
    // 0x6E - I2C write address (0x37 << 1)
    // register_address - sub-address (e.g. 0x51 for VCP, 0x50 for LG custom)
    // 0x84 - 0x80 | 4 (4 bytes follow, excluding checksum)
    // 0x03 - "set VCP" command
    // command_code - VCP code
    // 0x00 - value high byte
    // input_value - value low byte
    // checksum - XOR of all preceding bytes
    unsigned char packet[8] = { 0x6E, register_address, 0x84, 0x03, command_code, 0x00, input_value, 0x00 };

    // Calculate XOR checksum
    unsigned char checksum = 0;
    for (int i = 0; i < 7; i++)
        checksum ^= packet[i];
    packet[7] = checksum;

    int recvLen = 0;
    int adlResult = pfn_ADL_Display_DDCBlockAccess_Get(targetAdapterIdx, targetDisplayIdx, 0, 0, 8, (char*)packet, &recvLen, NULL);

    if (adlResult != ADL_OK)
    {
        printf("ADL_Display_DDCBlockAccess_Get failed with error %d\n", adlResult);
        return false;
    }

    return true;
}


// ============================================================
// Primary display auto-detect (GPU-agnostic)
// ============================================================

int AutoDetectPrimaryDisplay()
{
    DISPLAY_DEVICE dd;
    ZeroMemory(&dd, sizeof(dd));
    dd.cb = sizeof(dd);

    for (DWORD iDevNum = 0; EnumDisplayDevices(NULL, iDevNum, &dd, 0); iDevNum++)
    {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            printf("Primary display device found: %s\n", dd.DeviceName);
            printf("Using display index %d for primary display\n", (int)iDevNum);
            return (int)iDevNum;
        }
    }

    printf("Primary display not found, defaulting to index 0\n");
    return 0;
}


// ============================================================
// Main
// ============================================================

int main(int argc, char* argv[]) {

    int display_index = 0;
    BYTE input_value = 0;
    BYTE command_code = 0;  //VCP code or equivalent
    BYTE register_address = 0x51;

    // Usage: writeValueToMonitor.exe [display_index] [input_value] [command_code]
    // Uses default register addres 0x51 used for VCP codes
    if (argc == 4) {
        display_index = atoi(argv[1]);
        input_value = (BYTE)strtol(argv[2], NULL, 16);
        command_code = (BYTE)strtol(argv[3], NULL, 16);
    }

    // Usage: writeValueToMonitor.exe [display_index] [input_value] [command_code] [register_address]
    // Uses default register addres 0x51 used for VCP codes
    else if (argc == 5) {
        display_index = atoi(argv[1]);
        input_value = (BYTE)strtol(argv[2], NULL, 16);
        command_code = (BYTE)strtol(argv[3], NULL, 16);
        register_address = (BYTE)strtol(argv[4], NULL, 16);
    }
    else {
        printf("Incorrect Number of arguments!\n\n");

        printf("Arguments:\n");
        printf("display_index   - Index assigned to monitor (0 for first screen)\n");
        printf("input_value     - value to right to screen\n");
        printf("command_code    - VCP code or other\n");
        printf("register_address - Adress to write to, default 0x51 for VCP codes\n\n");

        printf("Usage:\n");
        printf("writeValueToScreen.exe [display_index] [input_value] [command_code]\n");
        printf("OR\n");
        printf("writeValueToScreen.exe [display_index] [input_value] [command_code] [register_address]\n");
        return 1;
    }

    // Auto-detect primary display if display_index is -1
    if (display_index == -1)
    {
        display_index = AutoDetectPrimaryDisplay();
    }

    // Try NVIDIA first
    if (InitNvidia())
    {
        printf("Using NVIDIA GPU\n");
        bool ok = NvidiaWriteValue(display_index, input_value, command_code, register_address);
        if (!ok)
        {
            printf("Changing input failed\n");
        }
        printf("\n");
        return ok ? 0 : 1;
    }

    // Try AMD ADL
    if (InitADL())
    {
        printf("Using AMD GPU\n");
        bool ok = ADLWriteValue(display_index, input_value, command_code, register_address);
        FreeADL();
        if (!ok)
        {
            printf("Changing input failed\n");
        }
        printf("\n");
        return ok ? 0 : 1;
    }

    printf("No supported GPU found (NVIDIA or AMD required)\n");
    return 1;
}
