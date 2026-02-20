#include "RegistrationManager.h"
#include "BinaryData.h" 
#include "AppLogger.h"
#include <string>
#include <vector>
#include <cmath>
#include <fstream> 

#if JUCE_WINDOWS
#include <windows.h>
#endif

#if JUCE_MAC
#include <cstdio>
#endif

void RegistrationManager::checkRegistration()
{
    juce::File licenseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("OnStage").getChildFile("onstage_license.key");

    if (licenseFile.existsAsFile())
    {
        juce::String savedSerial = licenseFile.loadFileAsString().trim();
        if (savedSerial.isNotEmpty())
        {
            if (tryRegister(savedSerial))
            {
                isRegistered = true;
                return;
            }
        }
    }
    isRegistered = false;
}

bool RegistrationManager::tryRegister(const juce::String& serialInput)
{
    try {
        juce::String cleanInput = serialInput.trim();
        if (cleanInput.isEmpty() || !cleanInput.containsOnly("0123456789-")) return false;

        long long inputNum = cleanInput.getLargeIntValue();
        long long expected = calculateExpectedSerial();
        
        LOG_INFO("Input Serial: " + cleanInput);

        if (expected != 0 && inputNum == expected)
        {
            juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("OnStage");
            if (!appData.exists()) appData.createDirectory();
            
            juce::File licenseFile = appData.getChildFile("onstage_license.key");
            licenseFile.replaceWithText(cleanInput);
            
            isRegistered = true;
            return true;
        }
        
        return false;
    }
    catch (...) { 
        LOG_ERROR("Exception in tryRegister");
        return false; 
    }
}

juce::String RegistrationManager::getMachineIDString()
{
    return juce::String(getMachineIDNumber());
}

char RegistrationManager::noteToChar(int note)
{
    if (note >= 0 && note <= 9) return '0' + note;
    switch(note) {
        case 10: return 'a';
        case 11: return '+';
        case 12: return '-';
        case 13: return '*';
        case 14: return '(';
        case 15: return ')';
    }
    return 0; 
}

juce::String RegistrationManager::decodeMidiToString(const void* data, size_t size)
{
    if (size == 0 || data == nullptr) return "";

    try {
        juce::MemoryInputStream inputStream(data, size, false);
        juce::MidiFile midiFile;
        
        if (midiFile.readFrom(inputStream))
        {
            midiFile.convertTimestampTicksToSeconds(); 
            juce::String extractedString = "";
            for (int t = 0; t < midiFile.getNumTracks(); ++t)
            {
                const auto* seq = midiFile.getTrack(t);
                for (int i = 0; i < seq->getNumEvents(); ++i)
                {
                    auto& event = seq->getEventPointer(i)->message;
                    if (event.isNoteOn() && event.getChannel() == 16)
                    {
                        char c = noteToChar(event.getNoteNumber());
                        if (c != 0) extractedString += c;
                    }
                }
            }
            return extractedString;
        }
    }
    catch (...) { 
        LOG_ERROR("MIDI Decode Crashed"); 
    }
    
    return "";
}

long long RegistrationManager::calculateExpectedSerial()
{
    try {
        int machineId = getMachineIDNumber();
        juce::String formula = decodeMidiToString(BinaryData::license_mid, BinaryData::license_midSize);
        
        if (formula.isEmpty()) {
            LOG_ERROR("CRITICAL: Failed to decode formula from MIDI asset.");
            return 0;
        }

        juce::String finalExpr = formula.replace("a", juce::String(machineId));
        long long rawResult = evaluateFormula(finalExpr);
        return rawResult;
    }
    catch (...) { 
        LOG_ERROR("Calculation Exception");
        return 0; 
    }
}

long long RegistrationManager::evaluateFormula(const juce::String& formula)
{
    try {
        std::string expr = formula.removeCharacters(" ").toStdString();
        if (expr.empty()) return 0;

        struct Parser {
            const char* str;
            char peek() { return *str; }
            char next() { return *str ? *str++ : 0; }

            long long parseExpression() {
                long long lhs = parseTerm();
                while (peek() == '+' || peek() == '-') {
                    char op = next();
                    long long rhs = parseTerm();
                    if (op == '+') lhs += rhs; else lhs -= rhs;
                }
                return lhs;
            }
            long long parseTerm() {
                long long lhs = parseFactor();
                while (peek() == '*') {
                    char op = next();
                    long long rhs = parseFactor();
                    if (op == '*') lhs *= rhs;
                }
                return lhs;
            }
            long long parseFactor() {
                if (peek() == '(') {
                    next(); 
                    long long val = parseExpression();
                    if (peek() == ')') next(); 
                    return val;
                }
                std::string numStr;
                while (isdigit(peek())) numStr += next();
                if (numStr.empty()) return 0;
                try { return std::stoll(numStr); } catch (...) { return 0; }
            }
        };
        Parser p { expr.c_str() };
        return p.parseExpression();
    }
    catch (...) { return 0; }
}

int RegistrationManager::getMachineIDNumber()
{
    juce::String hex = getSystemVolumeSerial();
    if (hex.length() < 5) hex = hex.paddedRight('0', 5);
    hex = hex.substring(0, 5);
    
    juce::String numericStr = "";
    for (int i = 0; i < hex.length(); ++i)
    {
        juce::juce_wchar c = hex[i];
        char val = '0';
        switch (c) {
            case 'A': val = '1'; break; case 'B': val = '2'; break; case 'C': val = '3'; break;
            case 'D': val = '4'; break; case 'E': val = '5'; break; case 'F': val = '6'; break;
            case 'G': val = '7'; break; case 'H': val = '8'; break; case 'I': val = '9'; break;
            case 'J': val = '0'; break; case 'K': val = '2'; break; case 'L': val = '3'; break;
            case 'M': val = '4'; break; case 'N': val = '5'; break; case 'O': val = '6'; break;
            case 'P': val = '7'; break; case '1': val = '8'; break; case '2': val = '9'; break; 
            case '3': val = '2'; break; case '4': val = '1'; break; case '5': val = '3'; break; 
            case '6': val = '4'; break; case '7': val = '5'; break; case '8': val = '6'; break; 
            case '9': val = '7'; break; case '0': val = '8'; break;
            case 'Q': val = '8'; break; case 'R': val = '9'; break; case 'S': val = '2'; break;
            case 'T': val = '1'; break; case 'U': val = '2'; break; case 'V': val = '3'; break;
            case 'W': val = '4'; break; case 'X': val = '5'; break; case 'Y': val = '6'; break;
            case 'Z': val = '7'; break; default: val = '0'; break;
        }
        numericStr += val;
    }
    if (numericStr.isEmpty()) return 12345;
    return numericStr.getIntValue();
}

juce::String RegistrationManager::getSystemVolumeSerial()
{
    #if JUCE_WINDOWS
        DWORD serialNum = 0;
        if (GetVolumeInformationW(L"C:\\", nullptr, 0, &serialNum, nullptr, nullptr, nullptr, 0))
        {
            return juce::String::toHexString((int)serialNum).toUpperCase();
        }
        return "00000";
    #elif JUCE_LINUX
        std::ifstream file("/etc/machine-id");
        if (file.is_open()) {
            std::string line;
            if (std::getline(file, line)) {
                return juce::String(line).substring(0, 8).toUpperCase();
            }
        }
        return "LINUX01";
    #else
        // macOS - read IOPlatformUUID directly (stable, not dependent on JUCE version)
        juce::String uuid;
        FILE* pipe = popen("ioreg -rd1 -c IOPlatformExpertDevice | awk '/IOPlatformUUID/{print $3}' | tr -d '\"'", "r");
        if (pipe != nullptr) {
            char buffer[256] = {};
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr)
                uuid = juce::String(buffer).trim().removeCharacters("-");
            pclose(pipe);
        }
        if (uuid.isEmpty())
            uuid = "MACOS001";
        return uuid.substring(0, 8).toUpperCase();
    #endif
}