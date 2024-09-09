#include <iostream>
#include <windows.h>
#include <wbemidl.h>
#include <string>
#include <locale>
#include <codecvt>  // Для преобразования строк
#pragma comment(lib, "wbemuuid.lib")

// Функция для преобразования кода типа памяти в строку
std::string GetMemoryTypeString(UINT memoryType) {
    switch (memoryType) {
    case 20: return "DDR";
    case 21: return "DDR2";
    case 24: return "DDR3";
    case 26: return "DDR4";
    case 12: return "SDRAM";
    case 18: return "DDR SGRAM";
    case 30: return "DDR4";  // SMBIOS может вернуть код 30 для DDR4
    case 29: return "DDR3";  // SMBIOS может вернуть код 29 для DDR3
    default: return "Unknown";
    }
}

// Функция для преобразования std::string в std::wstring
std::wstring StringToWString(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

void GetMemoryInfo() {
    HRESULT hres;

    // Инициализация COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cout << "Failed to initialize COM library. Error code = 0x"
            << std::hex << hres << std::endl;
        return;
    }

    // Инициализация безопасности
    hres = CoInitializeSecurity(
        NULL,
        -1,                          // Use default authentication services
        NULL,                        // Default authentication
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE,  // Default impersonation level
        NULL,                        // No authentication context
        EOAC_NONE,                    // No special options
        NULL                         // Reserved
    );

    if (FAILED(hres)) {
        std::cout << "Failed to initialize security. Error code = 0x"
            << std::hex << hres << std::endl;
        CoUninitialize();
        return;
    }

    // Получение доступа к WMI локально
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres)) {
        std::cout << "Failed to create IWbemLocator object. Error code = 0x"
            << std::hex << hres << std::endl;
        CoUninitialize();
        return;
    }

    IWbemServices* pSvc = NULL;

    // Подключение к пространству имен root\cimv2
    hres = pLoc->ConnectServer(
        BSTR(L"ROOT\\CIMV2"),
        NULL,    // Username (NULL for current user)
        NULL,    // Password (NULL for current user)
        0,       // Locale
        NULL,    // Security flags
        0,       // Authority
        0,       // Context object
        &pSvc    // IWbemServices proxy
    );

    if (FAILED(hres)) {
        std::cout << "Could not connect to WMI. Error code = 0x"
            << std::hex << hres << std::endl;
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // Установка безопасности прокси
    hres = CoSetProxyBlanket(
        pSvc,                        // IWbemServices proxy
        RPC_C_AUTHN_WINNT,           // Authentication service
        RPC_C_AUTHZ_NONE,            // Authorization service
        NULL,                        // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,      // Authentication level
        RPC_C_IMP_LEVEL_IMPERSONATE, // Impersonation level
        NULL,                        // Authentication context for proxy
        EOAC_NONE                    // No special options
    );

    if (FAILED(hres)) {
        std::cout << "Could not set proxy blanket. Error code = 0x"
            << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // Запрос данных о памяти
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        BSTR(L"WQL"),
        BSTR(L"SELECT * FROM Win32_PhysicalMemory"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres)) {
        std::cout << "Query for memory data failed. Error code = 0x"
            << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    int memoryModuleCount = 0;  // Переменная для подсчета плашек памяти

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }

        VARIANT vtProp;

        // Получение объема оперативной памяти (Capacity)
        hr = pclsObj->Get(L"Capacity", 0, &vtProp, 0, 0);
        std::wcout << L"Memory Capacity: " << _wtoi64(vtProp.bstrVal) / (1024 * 1024) << L" MB";
        VariantClear(&vtProp);

        // Получение типа памяти через SMBIOSMemoryType
        hr = pclsObj->Get(L"SMBIOSMemoryType", 0, &vtProp, 0, 0);
        std::wstring memoryType = StringToWString(GetMemoryTypeString(vtProp.uintVal));
        std::wcout << L" (" << memoryType << L")" << std::endl;
        VariantClear(&vtProp);

        memoryModuleCount++;  // Увеличение счетчика плашек

        pclsObj->Release();
    }

    // Вывод количества плашек памяти
    std::wcout << L"Total Memory Modules: " << memoryModuleCount << std::endl;

    // Очистка
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();
}

int main() {
    GetMemoryInfo();
    return 0;
}




