#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;


const wchar_t* mutex = L"Local\\YanitzkiMutex";
const char* accountFile = "account.bin";
const char* logFile = "operations.log";

const bool UseMutex = false;

struct BankAccount {
    int accountNumber;
    int balance;
};

void WriteLog(DWORD processId, int startBalance, string operation, int amount, int endBalance, string status) {
    ofstream writeLog(logFile, ios::app);
    if (writeLog.is_open()) {
        writeLog << processId << " | " << startBalance << " | " << operation << " | " << amount << " | " << endBalance << " | "<< status << endl;
        writeLog.close();
    }
}

void LoadAccFile(HANDLE hMutex) {
    DWORD mutexResult = WAIT_OBJECT_0;
    if (UseMutex) {
        mutexResult = WaitForSingleObject(hMutex, 3000);
        if (mutexResult == WAIT_TIMEOUT) {
            cout << "Ошибка: превышено время ожидания мьютекса при создании файла." << endl;
            return;
        }
    }

    ifstream checkFile(accountFile, ios::binary);
    if (!checkFile.good()) {
        checkFile.close();
        BankAccount createAcc = { 10001, 10000 };
        ofstream file(accountFile, ios::binary);
        if (file.is_open()) {
            file.write((char*)&createAcc, sizeof(BankAccount));
            file.close();
        }
    }
    else {
        checkFile.close();
    }

    if (UseMutex && (mutexResult == WAIT_OBJECT_0 || mutexResult == WAIT_ABANDONED)) {
        ReleaseMutex(hMutex);
    }
}

bool ReadAcc(HANDLE hMutex, BankAccount& acc) {
    DWORD mutexResult = WAIT_OBJECT_0;
    if (UseMutex) {
        mutexResult = WaitForSingleObject(hMutex, 3000);
        if (mutexResult == WAIT_TIMEOUT) {
            cout << "Отмена: превышено время ожидания мьютекса." << endl;
            return false;
        }
    }

    bool success = false;
    ifstream file(accountFile, ios::binary);
    if (file.read((char*)&acc, sizeof(BankAccount))) {
        if (mutexResult == WAIT_ABANDONED) {
            cout << "Внимание: получен WAIT_ABANDONED. Проверяем счет..." << endl;
        }
        if (acc.accountNumber == 10001 && acc.balance >= 0) {
            success = true;
        }
        else {
            cout << "Ошибка: данные счета некорректны!" << endl;
        }
    }
    else {
        cout << "Ошибка: не удалось прочитать файл счета." << endl;
    }
    file.close();

    if (UseMutex && (mutexResult == WAIT_OBJECT_0 || mutexResult == WAIT_ABANDONED)) {
        ReleaseMutex(hMutex);
    }
    return success;
}

void CashPlusMinus(HANDLE hMutex, bool isDeposit, int amount) {
    DWORD processId = GetCurrentProcessId();
    string operation = isDeposit ? "Пополнение" : "Снятие";

    if (amount <= 0) {
        cout << "Сумма операции должна быть больше 0" << endl;
        return;
    }

    DWORD mutexResult = WAIT_OBJECT_0;
    if (UseMutex) {
        mutexResult = WaitForSingleObject(hMutex, 3000);
        if (mutexResult == WAIT_TIMEOUT) {
            cout << "[PID " << processId << "] Ошибка: время ожидания мьютекса вышло." << endl;
            WriteLog(processId, -1, operation, amount, -1, "Отмена: время ожидания вышло");
            return;
        }
    }

    BankAccount acc = { 0, 0 };
    fstream file(accountFile, ios::binary | ios::in | ios::out);
    if (!file.is_open() || !file.read((char*)&acc, sizeof(BankAccount))) {
        cout << processId << "| Ошибка: не удалось прочитать файл счета" << endl;
        if (UseMutex && (mutexResult == WAIT_OBJECT_0 || mutexResult == WAIT_ABANDONED)) {
            ReleaseMutex(hMutex);
        }
        return;
    }

    if (mutexResult == WAIT_ABANDONED) {
        cout << processId << "| Внимание: мьютекс был брошен. Проверяем данные" << endl;
    }

    if (acc.accountNumber != 10001 || acc.balance < 0) {
        cout << processId << "| Ошибка: некорректные данные счета." << endl;
        WriteLog(processId, acc.balance, operation, amount, acc.balance, "Ошибка данных");
        file.close();
        if (UseMutex && (mutexResult == WAIT_OBJECT_0 || mutexResult == WAIT_ABANDONED)) {
            ReleaseMutex(hMutex);
        }
        return;
    }

    int startBalance = acc.balance;

    if (!UseMutex) {
        Sleep(200);
    }

    if (isDeposit) {
        if (acc.balance > 2147483647 - amount) {
            cout << processId << "| Ошибка: превышен лимит типа int" << endl;
            WriteLog(processId, startBalance, operation, amount, startBalance, "Ошибка: переполнение int");
        }
        else {
            acc.balance += amount;
            file.seekp(0, ios::beg);
            file.write((char*)&acc, sizeof(BankAccount));

            cout << processId << "| Пополнение на " << amount << " руб. | Баланс: " << acc.balance << " руб." << endl;
            WriteLog(processId, startBalance, operation, amount, acc.balance, "Успешно");
        }
    }
    else {
        if (acc.balance < amount) {
            cout << processId << "| Ошибка снятия " << amount << " руб.: недостаточно средств (Баланс: " << acc.balance << " руб.)." << endl;
            WriteLog(processId, startBalance, operation, amount, startBalance, "Ошибка: недостаточно средств");
        }
        else {
            acc.balance -= amount;
            file.seekp(0, ios::beg);
            file.write((char*)&acc, sizeof(BankAccount));

            cout << processId << "| Снятие " << amount << " руб. | Баланс: " << acc.balance << " руб." << endl;
            WriteLog(processId, startBalance, operation, amount, acc.balance, "Успешно");
        }
    }

    file.close();

    if (UseMutex && (mutexResult == WAIT_OBJECT_0 || mutexResult == WAIT_ABANDONED)) {
        ReleaseMutex(hMutex);
    }
}

void TwentyOperations(HANDLE hMutex) {
    cout << "\nЗапуск 20 случайных операций (PID: " << GetCurrentProcessId() << ")" << endl;
    for (int i = 0; i < 20; i++) {
        bool isDeposit = (rand() % 2);
        int amount = 100 + (rand() % 901);
        CashPlusMinus(hMutex, isDeposit, amount);

        int delay = 100 + (rand() % 401);
        Sleep(delay);
    }
    cout << "Случайные операции завершены" << endl;
}

int main() {
    setlocale(LC_ALL, "rus");
    srand(time(NULL) + GetCurrentProcessId());

    HANDLE hMutex = CreateMutex(NULL, FALSE, mutex);
    if (hMutex == NULL) {
        cout << "Ошибка: не удалось создать мьютекс. Код: " << GetLastError() << endl;
        return 1;
    }

    LoadAccFile(hMutex);

    int choice = -1;
    while (choice != 0) {
        cout << "\nБанковский терминал (PID: " << GetCurrentProcessId() << ")" << endl;
        cout << "1. Просмотр текущего баланса" << endl;
        cout << "2. Пополнение счёта" << endl;
        cout << "3. Снятие денежных средств" << endl;
        cout << "4. Автоматическое выполнение 20 случайных операций" << endl;
        cout << "0. Выход" << endl;
        cout << "Выберите действие: ";
        cin >> choice;

        if (choice == 1) {
            BankAccount acc;
            if (ReadAcc(hMutex, acc)) {
                cout << "Счёт №" << acc.accountNumber << " | Текущий баланс: " << acc.balance << " руб." << endl;
            }
        }
        else if (choice == 2) {
            int amount;
            cout << "Введите сумму пополнения: ";
            cin >> amount;
            CashPlusMinus(hMutex, true, amount);
        }
        else if (choice == 3) {
            int amount;
            cout << "Введите сумму снятия: ";
            cin >> amount;
            CashPlusMinus(hMutex, false, amount);
        }
        else if (choice == 4) {
            TwentyOperations(hMutex);
        }
        else if (choice != 0) {
            cout << "Неизвестная команда." << endl;
        }
    }

    CloseHandle(hMutex);
    return 0;
}