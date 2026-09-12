#pragma once

#include "utility.h"

#include <vector>
#include <windows.h>

// File LN250.h
// This file contains the definitions used to talk to the RS-442 bus
// on the Inertial Guidance
// 

#pragma pack (push)

// Ensure all data is tightly packed
#pragma pack (1)

namespace LN250
{

using std::vector;

// LM-250 specific constants
static const BYTE StartOfHeaderConst = 0xCA;

// Forward declarations
unsigned char CreateChecksum(unsigned char const* pStart, unsigned count);
template<typename Message>
unsigned char CreateMessageChecksum(Message const&msg)
{
    return CreateChecksum(reinterpret_cast<unsigned char const*>(&msg), sizeof(msg));
}


// The High Speed Port messages are in the following format:
// SOH byte
// Message ID byte
// Message Byte Count
// CheckSum 
#pragma warning (push)
#pragma warning (disable : 4355)

struct HighSpeedPortMessageHeader
{
    BYTE    StartOfHeader;  // This is always 0xCA
    BYTE    MessageID;      // 
    BYTE    ByteCount;      // Number of bytes in the message - 
                            //  does not include header
                            // The total length, including those will be 4
    BYTE    Checksum;       // two's compliment of the header (SOH + MessageID+ByteCount+Checksum = 0)

    HighSpeedPortMessageHeader() : StartOfHeader(StartOfHeaderConst) {  }
    HighSpeedPortMessageHeader(BYTE id, BYTE count)
        : StartOfHeader (StartOfHeaderConst)
        , MessageID (id)
        , ByteCount (count)
        , Checksum (CreateChecksum(reinterpret_cast<unsigned char*>(this), 3))
    {
    }

    HighSpeedPortMessageHeader& Init(BYTE id, BYTE count)
    {
        *this = HighSpeedPortMessageHeader (id, count);
        return *this;
    }

    HighSpeedPortMessageHeader Create(BYTE ID, BYTE count)
    {
        HighSpeedPortMessageHeader retv;
        retv.Init(ID, count);
    }
};
#pragma warning (pop)

struct MessageControl
{
    static const int MESSAGE_ID = 0x2f;

    BYTE    MessageID;      // which message are we trying to get?
    BYTE    Switch;         // Off = 0, On = 1
    BYTE    reserved1;      // zero
    BYTE    reserved2;      // zero

    static MessageControl MessageOn(BYTE id)
    {
        MessageControl retv = { id, 1 };
        return retv;
    }

    static MessageControl MessageOff(BYTE id)
    {
        MessageControl retv = { id, 0 };
        return retv;
    }
};

struct HybridInertialData
{
    static const int MESSAGE_ID = 0x32;

    double SystemTimer;             // 1-8 System Timer sec dp fl pt N/A
    double GPSTime;                 // 9-16 GPS Time sec dp fl pt N/A
    WORD   OutputDataValidity;	    // 17,18 Output Data Validity Word N/A discrete N/A
    double HybridLatitude;	        // 19-26 Hybrid Latitude radians dp fl pt N/A
    double HybridLongitude;         // 27-34 Hybrid Longitude radians dp fl pt N/A
    double HybridAltitude;          // 35-42 Hybrid Altitude (HAE) meters dp fl pt N/A 1
    float  HybridNorthVelocity;     // 43-46 Hybrid North Velocity met/sec sp fl pt N/A
    float  HybridEastVelocity;      // 47-50 Hybrid East Velocity met/sec sp fl pt N/A
    float  HybridVerticalVelocity;  // 51-54 Hybrid Vertical Velocity (+up) met/sec sp fl pt N/A
    float  HybridHeadingAngle;      // 55-58 Hybrid Heading Angle radians sp fl pt N/A
    float  HybridPitchAngle;        // 59-62 Hybrid Pitch Angle radians sp fl pt N/A
    float  HybridRollAngle;         // 63-66 Hybrid Roll Angle radians sp fl pt N/A
    float  HybridYawAngle;          // 67-70 Hybrid Yaw Angle radians sp fl pt N/A
    BYTE   HybridFOM;               // 71 Hybrid FOM integer 1

    static HybridInertialData Reorder(HybridInertialData const& in)
    {
        HybridInertialData retv;
        
        // Each field needs to be byte reversed
        UCSBUtility::s_reverse(retv.SystemTimer, in.SystemTimer);
        UCSBUtility::s_reverse(retv.GPSTime, in.GPSTime);
        UCSBUtility::s_reverse(retv.OutputDataValidity, in.OutputDataValidity);
        UCSBUtility::s_reverse(retv.HybridLatitude, in.HybridLatitude);
        UCSBUtility::s_reverse(retv.HybridLongitude, in.HybridLongitude);
        UCSBUtility::s_reverse(retv.HybridAltitude, in.HybridAltitude);
        UCSBUtility::s_reverse(retv.HybridNorthVelocity, in.HybridNorthVelocity);
        UCSBUtility::s_reverse(retv.HybridEastVelocity, in.HybridEastVelocity);
        UCSBUtility::s_reverse(retv.HybridVerticalVelocity, in.HybridVerticalVelocity);
        UCSBUtility::s_reverse(retv.HybridHeadingAngle, in.HybridHeadingAngle);
        UCSBUtility::s_reverse(retv.HybridPitchAngle, in.HybridPitchAngle);
        UCSBUtility::s_reverse(retv.HybridRollAngle, in.HybridRollAngle);
        UCSBUtility::s_reverse(retv.HybridYawAngle, in.HybridYawAngle);

        return retv;
    }

    static std::string GetName()
    {
        return "LN-250 Hybrid Inertial Device";
    }

    static GUID GetClassGUID()
    {
        return UCSBUtility::ConvertToGUID("{3EE2B4C5-D1EB-4fec-B72B-DB25F1DF69ED}");
    }

    static std::vector<UCSBUtility::StringPtrPair>  GetClassDescription()
    {
        static UCSBUtility::StringPtrPair definition[] =
        {
            "DPFLOAT_M",	"SystemTimer",				// 1-8 System Timer sec dp fl pt N/A
            "DPFLOAT_M",	"GPSTime",					// 9-16 GPS Time sec dp fl pt N/A
            "SSHORT_M",	    "OutputDataValidity",	    // 17,18 Output Data Validity Word N/A discrete N/A
            "DPFLOAT_M",	"HybridLatitude",	        // 19-26 Hybrid Latitude radians dp fl pt N/A
            "DPFLOAT_M",	"HybridLongitude",			// 27-34 Hybrid Longitude radians dp fl pt N/A
            "DPFLOAT_M",	"HybridAltitude",			// 35-42 Hybrid Altitude (HAE) meters dp fl pt N/A 1
            "SPFLOAT_M",	"HybridNorthVelocity",		// 43-46 Hybrid North Velocity met/sec sp fl pt N/A
            "SPFLOAT_M",	"HybridEastVelocity",		// 47-50 Hybrid East Velocity met/sec sp fl pt N/A
            "SPFLOAT_M",	"HybridVerticalVelocity",	// 51-54 Hybrid Vertical Velocity (+up) met/sec sp fl pt N/A
            "SPFLOAT_M",	"HybridHeadingAngle",		// 55-58 Hybrid Heading Angle radians sp fl pt N/A
            "SPFLOAT_M",	"HybridPitchAngle",			// 59-62 Hybrid Pitch Angle radians sp fl pt N/A
            "SPFLOAT_M",	"HybridRollAngle",			// 63-66 Hybrid Roll Angle radians sp fl pt N/A
            "SPFLOAT_M",	"HybridYawAngle",			// 67-70 Hybrid Yaw Angle radians sp fl pt N/A
            "UCHAR",	    "HybridFOM",		        // 71 Hybrid FOM integer 1
        };

        return UCSBUtility::ConvertToVector(definition);
    }

};

struct ModeCommandData
{
    static const int MESSAGE_ID = 0x30;

    WORD    MessageValidity;
    WORD    ModeCommand;
    DWORD   InitialLatitudeSemicircles;
    DWORD   InitialLongitudeSemicircles;
    SHORT   InitialAltitude;
    SHORT   InitialUTMZone;
    DWORD   InitialUTMNorthings;
    DWORD   InitialUTMEastings;
    WORD    InitialHeadingEntry;
    WORD    RollAxisBoresightCorrection;
    WORD    PitchAxisBoresightCorrection;
    WORD    YawAxisBoresightCorrection;
    WORD    Fore_AftMIMU;
    WORD    LateralMIMU;
    WORD    VerticalMIMU;
    WORD    Fore_AftReference;
    WORD    Lateral_Reference;
    WORD    Vertical_Reference;
    BYTE    BIT_RecordNumber;
    WORD    Year;
    BYTE    Month;
    BYTE    Day;
    DWORD   EnteredTime;
    WORD    Miscellaneous;

    static ModeCommandData Reorder(ModeCommandData const& in)
    {
        ModeCommandData retv;

        // Each field needs to be byte reversed
        UCSBUtility::s_reverse(retv.MessageValidity, in.MessageValidity);
        UCSBUtility::s_reverse(retv.ModeCommand, in.ModeCommand);
        UCSBUtility::s_reverse(retv.InitialLatitudeSemicircles, in.InitialLatitudeSemicircles);
        UCSBUtility::s_reverse(retv.InitialLongitudeSemicircles, in.InitialLongitudeSemicircles);
        UCSBUtility::s_reverse(retv.InitialAltitude, in.InitialAltitude);
        UCSBUtility::s_reverse(retv.InitialUTMZone, in.InitialUTMZone);
        UCSBUtility::s_reverse(retv.InitialUTMNorthings, in.InitialUTMNorthings);
        UCSBUtility::s_reverse(retv.InitialUTMEastings, in.InitialUTMEastings);
        UCSBUtility::s_reverse(retv.InitialHeadingEntry, in.InitialHeadingEntry);
        UCSBUtility::s_reverse(retv.RollAxisBoresightCorrection, in.RollAxisBoresightCorrection);
        UCSBUtility::s_reverse(retv.PitchAxisBoresightCorrection, in.PitchAxisBoresightCorrection);
        UCSBUtility::s_reverse(retv.YawAxisBoresightCorrection, in.YawAxisBoresightCorrection);
        UCSBUtility::s_reverse(retv.Fore_AftMIMU, in.Fore_AftMIMU);
        UCSBUtility::s_reverse(retv.LateralMIMU, in.LateralMIMU);
        UCSBUtility::s_reverse(retv.VerticalMIMU, in.VerticalMIMU);
        UCSBUtility::s_reverse(retv.Fore_AftReference, in.Fore_AftReference);
        UCSBUtility::s_reverse(retv.Lateral_Reference, in.Lateral_Reference);
        UCSBUtility::s_reverse(retv.Vertical_Reference, in.Vertical_Reference);
        UCSBUtility::s_reverse(retv.BIT_RecordNumber, in.BIT_RecordNumber);
        UCSBUtility::s_reverse(retv.Year, in.Year);
        UCSBUtility::s_reverse(retv.Month, in.Month);
        UCSBUtility::s_reverse(retv.Day, in.Day);
        UCSBUtility::s_reverse(retv.EnteredTime, in.EnteredTime);
        UCSBUtility::s_reverse(retv.Miscellaneous, in.Miscellaneous);

        return retv;
    }

    static std::string GetName()
    {
        return "LN-250 Mode Command Data";
    }

    static GUID GetClassGUID()
    {
        return UCSBUtility::ConvertToGUID("{CF99669A-8974-4897-BB99-645C4F8E39D0}");
    }

    static std::vector<UCSBUtility::StringPtrPair>  GetClassDescription()
    {
        static UCSBUtility::StringPtrPair definition[] =
        {
            "USHORT_M",  "MessageValidity",
            "USHORT_M",  "ModeCommand",
            "ULONG_M",  "InitialLatitudeSemicircles",
            "ULONG_M",  "InitialLongitudeSemicircles",
            "SSHORT_M",  "InitialAltitude",
            "SSHORT_M",  "InitialUTMZone",
            "ULONG_M",  "InitialUTMNorthings",
            "ULONG_M",  "InitialUTMEastings",
            "USHORT_M",  "InitialHeadingEntry",
            "USHORT_M",  "RollAxisBoresightCorrection",
            "USHORT_M",  "PitchAxisBoresightCorrection",
            "USHORT_M",  "YawAxisBoresightCorrection",
            "USHORT_M",  "Fore_AftMIMU",
            "USHORT_M",  "LateralMIMU",
            "USHORT_M",  "VerticalMIMU",
            "USHORT_M",  "Fore_AftReference",
            "USHORT_M",  "Lateral_Reference",
            "USHORT_M",  "Vertical_Reference",
            "UCHAR",    "BIT_RecordNumber",
            "USHORT_M", "Year",
            "UCHAR",    "Month",
            "UCHAR",    "Day",
            "SLONG_M",  "EnteredTime",
            "USHORT_M",  "Miscellaneous",
        };

        return UCSBUtility::ConvertToVector(definition);
    }

};

struct INSStatusData
{
    static const int MESSAGE_ID = 0x30;

    double SystemTimer;
    short  ModeStatusWords[28];


    static INSStatusData Reorder(INSStatusData const& in)
    {
        INSStatusData retv;

        // Each field needs to be byte reversed
        UCSBUtility::s_reverse(retv.SystemTimer, in.SystemTimer);

        const int mswCount = sizeof(in.ModeStatusWords)/sizeof(in.ModeStatusWords);

        for(int i=0; i < mswCount; ++i)
        {
            UCSBUtility::s_reverse(retv.ModeStatusWords[i], in.ModeStatusWords[i]);
        }

        return retv;
    }

    static std::string GetName()
    {
        return "LN-250 INS Status";
    }

    static GUID GetClassGUID()
    {
        return UCSBUtility::ConvertToGUID("{CF99669A-8974-4897-BB99-645C4F8E39D0}");
    }

    static std::vector<UCSBUtility::StringPtrPair>  GetClassDescription()
    {
        static UCSBUtility::StringPtrPair definition[] =
        {
            "DPFLOAT_M",	"SystemTimer",				// 1-8 System Timer sec dp fl pt N/A
            "SSHORT_M",	    "ModeStatusWord",	        // 9,10 
            "SSHORT_M",	    "BitSummary1",	            // 11,12 
            "SSHORT_M",	    "BitSummary2",	            // 13,14 
            "SSHORT_M",	    "SystemProcessor1",	        // 15,16 
            "SSHORT_M",	    "SystemProcessor2",	        // 17,18 
            "SSHORT_M",	    "SPSerialInterface",	    // 19, 20
            "SSHORT_M",	    "SPUARTInterface1",	        // 21, 22
            "SSHORT_M",	    "SPUARTInterface2",	        // 23, 24
            "SSHORT_M",	    "SPUARTInterface3",	        // 25, 26
            "SSHORT_M",	    "LSSI_RS_422Interface",	    // 27, 28
            "SSHORT_M",	    "AMUX1Status",	            // 29, 30
            "SSHORT_M",	    "AMUX12Status",	            // 31, 32
            "SSHORT_M",	    "AMUX2Status",	            // 33, 34
            "SSHORT_M",	    "SPNVMStatus1",	            // 35, 36
            "SSHORT_M",	    "SPNVMStatus2",	            // 37, 38
            "SSHORT_M",	    "SPDetectedEGRFailures",	// 39, 40
            "SSHORT_M",	    "EGRStatusWord1",	        // 41, 42
            "SSHORT_M",	    "EGRStatusWord2",	        // 43, 44
            "SSHORT_M",	    "UARTDataFailure",	        // 45, 46
            "SSHORT_M",	    "LD_TEC_ControlFailure",	// 47, 48
            "SSHORT_M",	    "Gyro_Analog_Failure1",	    // 49, 50
            "SSHORT_M",	    "GyroLoopControlFailure",	// 51, 52
            "SSHORT_M",	    "SiAC_a4Failure",	        // 53, 54
            "SSHORT_M",	    "GyroTempFailure",	        // 55, 56
            "SSHORT_M",	    "AccelTempFailure",	        // 57, 58
            "SSHORT_M",	    "Temp_IntensityFailure",	// 59, 60
            "SSHORT_M",	    "GPSTI_nterface",	        // 61, 62
            "SSHORT_M",	    "GAS_Interface",	        // 63, 64
        };

        return UCSBUtility::ConvertToVector(definition);
    }

};

// Dummy function to validate assertions at compile time
inline void Unused()
{
    STATIC_ASSERT(sizeof(HybridInertialData) == 71);
    STATIC_ASSERT(sizeof(ModeCommandData) == 55);
    STATIC_ASSERT(sizeof(INSStatusData) == 64);
}

inline unsigned char CreateChecksum(const unsigned char * pStart, unsigned count)
{
    if (!pStart)
    {
        throw "CreateChecksum got a NULL pointer, there should be no path for that";
    }
    
    unsigned char sum = 0;
    for (unsigned char const* pEnd = pStart + count; pStart < pEnd; ++pStart)
    {
        sum += *pStart;
    }

    return static_cast<unsigned char>(0 - sum);
}

// Now chunk the data
// look for the first SOH that is a valid header
// in order to be a valid header the four bytes starting with CA must
// sum to zero

// Then check that the payload checksum is correct

template<typename CIT, typename ProcessFn>
void ChunkInputData(CIT begin, CIT end, ProcessFn& interpretData)
{
    unsigned numDeadBytes = 0;

    for (; begin < end; ++begin)
    {
        // Are we at the start of a header?
        // if no, skip to the next byte (useless count here)
        if (*begin != StartOfHeaderConst)
        {
            ++numDeadBytes;
            continue;
        }

        if ((end - begin) < 5)
        {
            return;
        }
        HighSpeedPortMessageHeader const* pData = reinterpret_cast<HighSpeedPortMessageHeader const*>(begin);

        // if the checksum isn't correct, move on to the next byte
        BYTE checksum = CreateMessageChecksum(*pData);
        if (checksum != 0)
        {
            continue;
        }

        // We can now skip the header
        begin += 4;

        // Check to see if the rest of the data exists
        // don't forget the checksum byte
        if ((pData->ByteCount + 1) > (end - begin))
        {
            // we are done
            return;
        }

        BYTE const* pPayload = begin;

        // processing can skip the payload

        checksum = CreateChecksum(pPayload, pData->ByteCount + 1);
        if (checksum != 0)
        {
            // Bad packet - if it is because of dropped bytes
            // then we don't want to skip because we might miss the
            // start of the next packet
            continue;
        }

        // Adjust for the packet size
        // Remember there is a checksum byte
        // that will be taken care of by the ++begin in the loop
        begin += pData->ByteCount;

        // This would be the same data passed to an interpret function
        interpretData(pData->MessageID, pData->ByteCount, pPayload);
    }
}

template<typename processFnType>
struct InputBuffer
{
    InputBuffer(processFnType processFn) : highWaterMark(0), 
        m_processFn(processFn)
    {

    }

    void AddData(BYTE const* pBegin, BYTE const* pEnd)
    {
        data.insert(data.end(), pBegin, pEnd);
        if (data.size() > 0)
        {
            LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, *this);
            UpdateData();
        }
    }

    void operator () (BYTE messageID, BYTE byteCount, BYTE const* pPayload)
    {
        BYTE const* pBegin = &data[0];
        highWaterMark = pPayload + byteCount - pBegin + 1;
        m_processFn(messageID, byteCount, pPayload);
    }

private :
    void UpdateData()
    {
        if (highWaterMark <= data.size())
        {
            data = vector<BYTE>(data.begin() + highWaterMark, data.end());
            highWaterMark = 0;
        }
    }

    InputBuffer(InputBuffer const&);
    InputBuffer operator = (InputBuffer const&);

private :
    vector<BYTE>    data;
    DWORD           highWaterMark;
    processFnType   m_processFn;
};

typedef bool(*ProcessFn)(BYTE messageID, BYTE byteCount, BYTE const* pPayload);


// This must only call WriteFile once
// if not, multiple threads could be interleaved
template<typename T>
inline std::vector<BYTE> SendLNMessage(HANDLE hFile, T const& t, BYTE blockType)
{
    HighSpeedPortMessageHeader header(blockType, sizeof(t));
    
    DWORD size = sizeof(header) + sizeof(t) + 1;
    std::vector<BYTE> output;
    output.reserve(size);

    DWORD written = 0;
    
    // Write block header
//    WriteFile(hFile, &header, sizeof(header), &written, NULL);
    UCSBUtility::AddStructure(output, header);

    // Write data
//    WriteFile(hFile, &t, sizeof(t), &written, NULL);
    UCSBUtility::AddStructure(output, t);

    unsigned char checksum = LN250::CreateMessageChecksum(t);

    // Write checksum
//    WriteFile(hFile, &checksum, sizeof(checksum), &written, NULL);
    UCSBUtility::AddStructure(output, checksum);
    WriteFile(hFile, &output[0], output.size(), &written, NULL);

    return output;
}

template<typename Message>
std::vector<BYTE> SendLNMessage(HANDLE hPort, Message const& msg)
{
    // Create the output stream
    HighSpeedPortMessageHeader header(Message::MESSAGE_ID, sizeof(msg));

    vector<BYTE> stream;
    stream.resize(sizeof(header) + sizeof(msg) + 1);
    memcpy(&stream[0], &header, sizeof(header));
    memcpy(&stream[sizeof(header)], &msg, sizeof(msg));
    stream[sizeof(header) + sizeof(msg)] = CreateMessageChecksum(msg);
    
    DWORD numBytes = 0;
    WriteFile(hPort, &stream[0], stream.size(), &numBytes, NULL);
    if (numBytes != stream.size())
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to write %d bytes...%d bytes written\n", stream.size(), numBytes);
    }

    return stream;
}

inline std::vector<BYTE> SendLNHeader(HANDLE hPort, BYTE messageID)
{
    // Create the output stream
    HighSpeedPortMessageHeader header(messageID, 0);

    vector<BYTE> stream;
    stream.resize(sizeof(header) + 1);
    memcpy(&stream[0], &header, sizeof(header));
    stream[sizeof(header)] = 0;
    
    DWORD numBytes = 0;
    WriteFile(hPort, &stream[0], stream.size(), &numBytes, NULL);
    if (numBytes != stream.size())
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to write %d bytes...%d bytes written\n", stream.size(), numBytes);
    }

    return stream;
}

};

#pragma pack (pop)
