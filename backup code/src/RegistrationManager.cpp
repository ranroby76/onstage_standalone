#include "RegistrationManager.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

#if JUCE_LINUX
#include <fstream>
#include <string>
#endif

#if JUCE_MAC
#include <cstdio>
#endif

void RegistrationManager::checkRegistration() {
    juce::File licenseFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("Fanan").getChildFile("OnStage").getChildFile("onstage_license.key");
    
    if (licenseFile.existsAsFile()) {
        juce::String savedSerial = licenseFile.loadFileAsString().trim();
        if (savedSerial.isNotEmpty()) {
            if (tryRegister(savedSerial)) {
                registered = true;
                return;
            }
        }
    }
    registered = false;
}

bool RegistrationManager::tryRegister(const juce::String& serialInput) {
    try {
        juce::String cleanInput = serialInput.trim();
        if (cleanInput.isEmpty() || !cleanInput.containsOnly("0123456789-")) 
            return false;
        
        // Handle negative numbers
        cleanInput = cleanInput.removeCharacters(" ");
        long long inputNum = cleanInput.getLargeIntValue();
        long long expected = calculateExpectedSerial();
        
        if (inputNum == expected) {
            // Save license file
            juce::File appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                 .getChildFile("Fanan").getChildFile("OnStage");
            if (!appData.exists()) 
                appData.createDirectory();
            
            juce::File licenseFile = appData.getChildFile("onstage_license.key");
            licenseFile.replaceWithText(cleanInput);
            
            registered = true;
            return true;
        }
        
        return false;
    }
    catch (...) {
        return false;
    }
}

juce::String RegistrationManager::getMachineIDString() {
    return juce::String(getMachineIDNumber());
}

int RegistrationManager::getMachineIDNumber() {
    juce::String hex = getSystemVolumeSerial();
    if (hex.length() < 5) 
        hex = hex.paddedRight('0', 5);
    hex = hex.substring(0, 5);
    
    juce::String numericStr = "";
    for (int i = 0; i < hex.length(); ++i) {
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
    
    if (numericStr.isEmpty()) 
        return 12345;
    
    return numericStr.getIntValue();
}

long long RegistrationManager::calculateExpectedSerial() {
    long long id = getMachineIDNumber();
    
    // OnStage serial formula: (((((ID+8401)*2)+1289)*2)-9090)
    long long result = id + 8401LL;
    result = result * 2;
    result = result + 1289LL;
    result = result * 2;
    result = result - 9090LL;
    return result;
}

juce::String RegistrationManager::getSystemVolumeSerial() {
#if JUCE_WINDOWS
    DWORD serialNum = 0;
    if (GetVolumeInformationW(L"C:\\", nullptr, 0, &serialNum, nullptr, nullptr, nullptr, 0)) {
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

void RegistrationManager::updateDemoMode() {
    if (registered) {
        demoSilenceActive.store(false);
        return;
    }
    
    // Demo mode: 3 seconds silence every 15 seconds
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    if (lastSilenceToggleTime == 0.0) {
        lastSilenceToggleTime = now;
        inSilencePeriod = false;
    }
    
    double elapsed = now - lastSilenceToggleTime;
    
    if (!inSilencePeriod) {
        // Normal operation for 15 seconds
        if (elapsed >= 15.0) {
            inSilencePeriod = true;
            lastSilenceToggleTime = now;
            demoSilenceActive.store(true);
        }
    } else {
        // Silence for 3 seconds
        if (elapsed >= 3.0) {
            inSilencePeriod = false;
            lastSilenceToggleTime = now;
            demoSilenceActive.store(false);
        }
    }
}
