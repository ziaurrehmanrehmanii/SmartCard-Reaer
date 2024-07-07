#include <winscard.h>
#include <iostream>
#include <vector>
#include <string>

void listReaders(SCARDCONTEXT hContext, std::vector<std::wstring>& readers) {
    DWORD readersLen = SCARD_AUTOALLOCATE;
    LPTSTR readersList = NULL;
    LONG retCode = SCardListReaders(hContext, NULL, (LPTSTR)&readersList, &readersLen);

    if (retCode != SCARD_S_SUCCESS) {
        std::wcerr << L"Failed to list readers: " << retCode << std::endl;
        return;
    }

    LPTSTR pReader = readersList;
    while (*pReader != '\0') {
        readers.push_back(pReader);
        pReader += wcslen(pReader) + 1;
    }

    SCardFreeMemory(hContext, readersList);
}

void waitForCardInsertion(SCARDCONTEXT hContext, const std::wstring& readerName) {
    SCARD_READERSTATE rgscState;
    rgscState.szReader = readerName.c_str();
    rgscState.dwCurrentState = SCARD_STATE_EMPTY;

    LONG retCode;
    while (true) {
        retCode = SCardGetStatusChange(hContext, INFINITE, &rgscState, 1);
        if (retCode != SCARD_S_SUCCESS) {
            std::wcerr << L"Failed to get status change: " << retCode << std::endl;
            return;
        }

        if (rgscState.dwEventState & SCARD_STATE_PRESENT) {
            std::wcout << L"Card inserted in reader: " << readerName << std::endl;
            break;
        }
    }
}

void readCard(SCARDCONTEXT hContext, const std::wstring& readerName) {
    SCARDHANDLE hCard;
    DWORD dwActProtocol;
    LONG retCode = SCardConnectW(hContext, readerName.c_str(), SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActProtocol);

    if (retCode != SCARD_S_SUCCESS) {
        std::wcerr << L"Failed to connect to card: " << retCode << std::endl;
        return;
    }

    BYTE recvBuffer[256];
    DWORD recvLength = sizeof(recvBuffer);
    BYTE sendBuffer[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 }; // Example APDU command (Get UID)
    DWORD sendLength = sizeof(sendBuffer);

    retCode = SCardTransmit(hCard, SCARD_PCI_T1, sendBuffer, sendLength, NULL, recvBuffer, &recvLength);
    if (retCode != SCARD_S_SUCCESS) {
        std::wcerr << L"Failed to transmit: " << retCode << std::endl;
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        return;
    }

    std::wcout << L"Card UID: ";
    for (DWORD i = 0; i < recvLength; ++i) {
        std::wcout << std::hex << (int)recvBuffer[i] << L" ";
    }
    std::wcout << std::endl;

    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
}

int main() {
    SCARDCONTEXT hContext;
    LONG retCode = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &hContext);
    if (retCode != SCARD_S_SUCCESS) {
        std::wcerr << L"Failed to establish context: " << retCode << std::endl;
        return 1;
    }

    std::vector<std::wstring> readers;
    listReaders(hContext, readers);

    if (readers.empty()) {
        std::wcout << L"No readers found." << std::endl;
        SCardReleaseContext(hContext);
        return 1;
    }

    std::wcout << L"Available readers:" << std::endl;
    for (size_t i = 0; i < readers.size(); ++i) {
        std::wcout << i + 1 << L": " << readers[i] << std::endl;
    }

    size_t selectedReader;
    std::wcout << L"Select a reader (1-" << readers.size() << "): ";
    std::wcin >> selectedReader;
    if (selectedReader < 1 || selectedReader > readers.size()) {
        std::wcerr << L"Invalid selection." << std::endl;
        SCardReleaseContext(hContext);
        return 1;
    }

    const std::wstring& readerName = readers[selectedReader - 1];

    waitForCardInsertion(hContext, readerName);
    readCard(hContext, readerName);

    SCardReleaseContext(hContext);
    return 0;
}
