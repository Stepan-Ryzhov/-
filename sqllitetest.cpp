#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <Windows.h>
#include "sqlite/sqlite3.h"

using namespace std;

// Структура для хранения последних параметров поиска и имен файлов
struct LastSearch {
    int year = -1;
    string city;
    string street;
    string last_year_file;   // Имя файла для года
    string last_street_file; // Имя файла для улицы
    string last_city_file;   // Имя файла для города
} last_search;

// Класс для управления подключением к базе данных SQLite
class SQLiteDB {
    sqlite3* db;
public:
    SQLiteDB(const string& name) {
        if (sqlite3_open(name.c_str(), &db) != SQLITE_OK) {
            cerr << u8"Ошибка SQLite: " << sqlite3_errmsg(db) << endl;
            exit(1);
        }
    }
    ~SQLiteDB() { sqlite3_close(db); }
    sqlite3* get() const { return db; }
};

// Класс для управления подготовленным запросом SQLite
class SQLiteStmt {
    sqlite3_stmt* stmt;
public:
    SQLiteStmt(sqlite3* db, const string& sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << u8"Ошибка SQLite: " << sqlite3_errmsg(db) << endl;
            exit(1);
        }
    }
    ~SQLiteStmt() { sqlite3_finalize(stmt); }
    sqlite3_stmt* get() const { return stmt; }
};

// Функция для получения выбора пользователя из меню с проверкой ввода
int getMenuChoice(const string& prompt) {
    int choice;
    cout << prompt;
    cin >> choice;
    while (cin.fail() || cin.peek() != '\n') {
        cout << u8"Некорректный выбор\nПовторите ввод: ";
        cin.clear();
        cin.ignore(10000, '\n');
        cin >> choice;
    }
    return choice;
}

// Проверка, что строка содержит только русские буквы
bool isRussianLettersOnly(const string& str) {
    if (str.empty()) return false;
    for (int i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        if (c == ' ') {
            i++;
            continue;
        }
        if (i + 1 >= str.length()) return false; 
        unsigned char next = str[i + 1];

        if (c == 0xD0 && ((next >= 0x90 && next <= 0xBF) || next == 0x81)) {
            i += 2; 
        }
        else if (c == 0xD1 && ((next >= 0x80 && next <= 0x8F) || next == 0x91)) {
            i += 2; 
        }
        else {
            return false; 
        }
    }
    return true;
}

bool firstTrue(const string& str) {
    unsigned char byte1 = static_cast<unsigned char>(str[0]);
    unsigned char byte2 = (str.size() > 1) ? static_cast<unsigned char>(str[1]) : 0;

    bool firstCharValid = false;
    if (byte1 == 0xD0) {
        // Проверяем А-Я (кроме Ё)
        if (byte2 >= 0x90 && byte2 <= 0xAF) {
            firstCharValid = true;
        }
        // Проверяем Ё отдельно
        else if (byte2 == 0x81) {
            firstCharValid = true;
        }
    }
    if (!firstCharValid) {
        return false;
    }
    return true;
}

// Проверка, что фамилия содержит русский буквы и дефис
bool isCorrectSecondname(const string& str) {
    if (str.empty()) return false;
    for (int i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        // Дефис разрешён, но не в начале и не в конце
        if (c == 0x2D) {
            if (i == 0 || i == str.length() - 1) {  // Дефис в начале или конце — ошибка
                return false;
            }
            i++;  // Переходим к следующему символу
            continue;
        }
        if (i + 1 >= str.length()) return false;
        unsigned char next = str[i + 1];

        if (c == 0xD0 && ((next >= 0x90 && next <= 0xBF) || next == 0x81)) {
            i += 2;
        }
        else if (c == 0xD1 && ((next >= 0x80 && next <= 0x8F) || next == 0x91)) {
            i += 2;
        }
        else {
            return false;
        }
    }
    return true;
}

// Проверка, что строка содержит только цифры
bool isDigitsOnly(const string& str) {
    for (char c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return !str.empty();
}

// Проверка, что строка является корректным именем файла
bool isValidFilename(const string& str) {
    if (str.empty()) return false;
    for (int i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        if (c == 0x2E) {
            i += 1;
        }
        else if (c == 0x5F) {
            i += 1;
        }
        else if (c >= 0x30 && c <= 0x39) {
            i += 1;
        }
        else if ((c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A)) {
            i += 1;
        }
        else if (i + 1 < str.length()) { 
            unsigned char next = str[i + 1];
            if (c == 0xD0 && ((next >= 0x90 && next <= 0xBF) || next == 0x81)) {
                i += 2; 
            }
            else if (c == 0xD1 && ((next >= 0x80 && next <= 0x8F) || next == 0x91)) {
                i += 2; 
            }
            else {
                return false; 
            }
        }
        else {
            return false; 
        }
    }
    return true;
}

// Вывод заголовка таблицы в файл
void printTableHeader(ostream& out) {
    out << left << setw(2) << u8"ID" << " | " << left << setw(27) << u8"Фамилия" << " | "
        << left << setw(13) << u8"Имя" << " | " << left << setw(23) << u8"Отчество" << " | "
        << left << setw(10) << u8"Год рождения" << " | " << left << setw(33) << u8"Адрес" << " | "
        << left << setw(15) << u8"Место" << endl;
    out << u8"--------------------------------------------------------------------------------------------------------------------------" << endl;
}

// Вывод строки данных в файл
void printRow(ostream& out, int id, const string& familiya, const string& imya, const string& otchestvo, int godrozh, const string& adres, const string& mesto) {
    int lenFamiliya = 0;
    if (familiya != "") {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(familiya.c_str());
        while (*ptr) {
            if ((*ptr & 0xC0) != 0x80) lenFamiliya++;
            ptr++;
        }
    }

    int lenImya = 0;
    if (imya != "") {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(imya.c_str());
        while (*ptr) {
            if ((*ptr & 0xC0) != 0x80) lenImya++;
            ptr++;
        }
    }

    int lenOtchestvo = 0;
    if (otchestvo != "") {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(otchestvo.c_str());
        while (*ptr) {
            if ((*ptr & 0xC0) != 0x80) lenOtchestvo++;
            ptr++;
        }
    }

    int lenAdres = 0;
    if (adres != "") {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(adres.c_str());
        while (*ptr) {
            if ((*ptr & 0xC0) != 0x80) lenAdres++;
            ptr++;
        }
    }

    int lenMesto = 0;
    if (mesto != "") {
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(mesto.c_str());
        while (*ptr) {
            if ((*ptr & 0xC0) != 0x80) lenMesto++;
            ptr++;
        }
    }

    // Длина года рождения в символах
    int lenGodrozh = to_string(godrozh).length();
    out << left << setw(2) << id << " | " << familiya << string(20 - lenFamiliya, ' ') << " | "
        << imya << string(10 - lenImya, ' ') << " | " << otchestvo << string(15 - lenOtchestvo, ' ') << " | "
        << left << setw(12) << godrozh << " | " << adres << string(28 - lenAdres, ' ') << " | "
        << mesto << string(15 - lenMesto, ' ') << endl;
}

// Вывод базы данных в консоль
void print(sqlite3_stmt* stmt) {
    // Заголовок с шириной столбцов (в символах)
    cout << left << setw(2) << u8"ID" << " | "
        << left << setw(25) << u8"Фамилия" << " | "
        << left << setw(13) << u8"Имя" << " | "
        << left << setw(23) << u8"Отчество" << " | "
        << left << setw(12) << u8"Год рождения" << " | "
        << left << setw(35) << u8"Адрес" << " | "
        << left << setw(15) << u8"Место" << endl;
    cout << u8"------------------------------------------------------------------------------------------------------------------------" << endl;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char* familiya = sqlite3_column_text(stmt, 1);
        const unsigned char* imya = sqlite3_column_text(stmt, 2);
        const unsigned char* otchestvo = sqlite3_column_text(stmt, 3);
        int godrozh = sqlite3_column_int(stmt, 4);
        const unsigned char* adres = sqlite3_column_text(stmt, 5);
        const unsigned char* mesto = sqlite3_column_text(stmt, 6);

        // Подсчёт длины каждой строки в символах (UTF-8)
        int lenFamiliya = 0;
        if (familiya) {
            const unsigned char* ptr = familiya;
            while (*ptr) {
                if ((*ptr & 0xC0) != 0x80) lenFamiliya++;
                ptr++;
            }
        }

        int lenImya = 0;
        if (imya) {
            const unsigned char* ptr = imya;
            while (*ptr) {
                if ((*ptr & 0xC0) != 0x80) lenImya++;
                ptr++;
            }
        }

        int lenOtchestvo = 0;
        if (otchestvo) {
            const unsigned char* ptr = otchestvo;
            while (*ptr) {
                if ((*ptr & 0xC0) != 0x80) lenOtchestvo++;
                ptr++;
            }
        }

        int lenAdres = 0;
        if (adres) {
            const unsigned char* ptr = adres;
            while (*ptr) {
                if ((*ptr & 0xC0) != 0x80) lenAdres++;
                ptr++;
            }
        }

        int lenMesto = 0;
        if (mesto) {
            const unsigned char* ptr = mesto;
            while (*ptr) {
                if ((*ptr & 0xC0) != 0x80) lenMesto++;
                ptr++;
            }
        }

        // Длина года рождения в символах
        int lenGodrozh = to_string(godrozh).length();

        // Вывод с учётом ширины столбцов
        cout << left << setw(2) << id << " | "
            << (familiya ? reinterpret_cast<const char*>(familiya) : "") << string(18 - lenFamiliya, ' ') << " | "
            << (imya ? reinterpret_cast<const char*>(imya) : "") << string(10 - lenImya, ' ') << " | "
            << (otchestvo ? reinterpret_cast<const char*>(otchestvo) : "") << string(15 - lenOtchestvo, ' ') << " | "
            << godrozh << string(12 - lenGodrozh, ' ') << " | "
            << (adres ? reinterpret_cast<const char*>(adres) : "") << string(30 - lenAdres, ' ') << " | "
            << (mesto ? reinterpret_cast<const char*>(mesto) : "") << string(15 - lenMesto, ' ') << endl;
    }
}

// Запись результата запроса в файл
void write(const string& f_name, sqlite3_stmt* stmt, bool append) {
    ofstream file(f_name, append ? ios::app : ios::out);
    if (!file) {
        cerr << u8"Не удалось открыть файл: " << f_name << endl;
        return;
    }
    if (!append) printTableHeader(file);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        string familiya = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        string imya = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        string otchestvo = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        string adres = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        string mesto = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        printRow(file, sqlite3_column_int(stmt, 0), familiya, imya, otchestvo,
            sqlite3_column_int(stmt, 4), adres, mesto);
    }
    cout << u8"\nРезультат сохранен в файл: " << f_name;
}

// Запись одного пользователя в файл
void writeUserToFile(const string& f_name, int id, const string& familiya, const string& imya,
    const string& otchestvo, int godrozh, const string& adres, const string& mesto, bool append) {
    ofstream file(f_name, append ? ios::app : ios::out);
    if (!file) {
        cerr << u8"Не удалось открыть файл: " << f_name << endl;
        return;
    }
    if (!append) printTableHeader(file);
    printRow(file, id, familiya, imya, otchestvo, godrozh, adres, mesto);
}

// Запрос имени файла для записи с проверкой
void saveToFile(const string& default_name, sqlite3_stmt* stmt, bool append) {
    int v = getMenuChoice(u8"\nВыберите функцию для дальнейшей работы: \n-------------------------------------------------\n1) Использовать имя файла по умолчанию\n\n2) Задать собственное имя файла для записи\n-------------------------------------------------\nВведите цифру подпункта меню: ");
    string filename, temp;
    if (v == 1) {
        filename = default_name;
    }
    else if (v == 2) {
        do {
            cout << u8"Введите название файла для записи данных: ";
            cin.ignore(10000, '\n');
            getline(cin, temp);
            if (!isValidFilename(temp)) {
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                cout << u8"Имя файла должно содержать только буквы, цифры, подчеркивание или точку!\n";
            }
        } while (!isValidFilename(temp));
        filename = temp + ".txt";
    }
    else {
        cout << u8"Некорректный выбор!\n";
        return;
    }
    
    if (default_name == "year_sort.txt") {
        last_search.last_year_file = filename;
    }
    else if (default_name == "adres_sort.txt") {
        last_search.last_street_file = filename;
    }
    else if (default_name == "city_sort.txt") {
        last_search.last_city_file = filename;
    }
    write(filename, stmt, append);
}

// Удаление пользователя по ID из базы данных и связанных файлов
void deleteUserById(sqlite3* db, int id) {
    // Получаем данные пользователя перед удалением
    SQLiteStmt select_stmt(db, "SELECT familiya, imya, otchestvo, godrozh, adres, mesto FROM users WHERE id = ?;");
    sqlite3_bind_int(select_stmt.get(), 1, id);

    string familiya, imya, otchestvo, adres, mesto;
    int godrozh = -1;
    bool user_exists = false;

    if (sqlite3_step(select_stmt.get()) == SQLITE_ROW) {
        familiya = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt.get(), 0));
        imya = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt.get(), 1));
        otchestvo = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt.get(), 2));
        godrozh = sqlite3_column_int(select_stmt.get(), 3);
        adres = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt.get(), 4));
        mesto = reinterpret_cast<const char*>(sqlite3_column_text(select_stmt.get(), 5));
        user_exists = true;
    }

    // Удаляем из базы данных
    SQLiteStmt delete_stmt(db, "DELETE FROM users WHERE id = ?;");
    sqlite3_bind_int(delete_stmt.get(), 1, id);
    if (sqlite3_step(delete_stmt.get()) != SQLITE_DONE) {
        cerr << u8"Ошибка удаления из базы данных: " << sqlite3_errmsg(db) << endl;
        return;
    }

    if (!user_exists) {
        cout << u8"Пользователь с ID " << id << u8" не найден в базе данных." << endl;
        return;
    }

    cout << u8"Данные успешно удалены из базы данных." << endl;

    // Удаляем из файлов поиска, если они существуют
    vector<pair<string, string>> search_files = {
        {last_search.last_year_file.empty() ? "year_sort.txt" : last_search.last_year_file, to_string(godrozh)},
        {last_search.last_street_file.empty() ? "adres_sort.txt" : last_search.last_street_file, adres},
        {last_search.last_city_file.empty() ? "city_sort.txt" : last_search.last_city_file, mesto}
    };

    for (int i = 0; i < search_files.size(); ++i) {
        string filename = search_files[i].first; 
        string key = search_files[i].second;     

        ifstream in_file(filename);
        if (!in_file) {
            continue; 
        }

        vector<string> lines;
        string line;
        bool header_written = false;

        // Читаем файл построчно и сохраняем все строки, кроме той, что соответствует удаляемому ID
        while (getline(in_file, line)) {
            if (line.empty() || line.find("ID") != string::npos || line.find("---") != string::npos) {
                lines.push_back(line); // Сохраняем заголовок или разделитель
                header_written = (line.find("ID") != string::npos);
                continue;
            }

            try {
                stringstream ss(line);
                string token;
                getline(ss, token, '|');
                int current_id = stoi(token);

                if (current_id != id) {
                    lines.push_back(line); // Сохраняем строку, если ID не совпадает
                }
            }
            catch (const exception& e) {
                cerr << u8"Ошибка парсинга строки в файле " << filename << ": " << e.what() << endl;
                lines.push_back(line); // Сохраняем строку как есть в случае ошибки
            }
        }
        in_file.close();

        // Перезаписываем файл без удаленного пользователя
        ofstream out_file(filename, ios::trunc);
        if (!out_file) {
            cerr << u8"Не удалось открыть файл для записи: " << filename << endl;
            continue;
        }

        for (const auto& l : lines) {
            out_file << l << endl;
        }
        out_file.close();
    }
}

// Структура для хранения данных пользователя при сортировке
struct User {
    int id;
    string familiya, imya, otchestvo;
    int godrozh;
    string adres, mesto;
};

// Функция сравнения для сортировки
bool compareByField(const User& a, const User& b, int field, bool ascending) {
    switch (field) {
    case 0: return ascending ? a.familiya < b.familiya : a.familiya > b.familiya;
    case 1: return ascending ? a.imya < b.imya : a.imya > b.imya;
    case 2: return ascending ? a.otchestvo < b.otchestvo : a.otchestvo > b.otchestvo;
    case 3: return ascending ? a.godrozh < b.godrozh : a.godrozh > b.godrozh;
    case 4: return ascending ? a.adres < b.adres : a.adres > b.adres;
    case 5: return ascending ? a.mesto < b.mesto : a.mesto > b.mesto;
    default: return false;
    }
}

// Функция очистки строки от пробелов
string trim(const string& str) {
    int first = str.find_first_not_of(' ');
    if (first == string::npos) return ""; // Если строка состоит только из пробелов
    int last = str.find_last_not_of(' ');
    return str.substr(first, last - first + 1);
}

// Сортировка базы данных или файла
void sort_smth(const string& table_name) {
    SQLiteDB db(table_name);
    int db_or_txt = getMenuChoice(u8"\nВыберите параметр для сортировки: \n-------------------------------------------------\n1) Отсортировать базу данных\n\n2) Отсортировать файл по названию\n-------------------------------------------------\nВведите цифру подпункта меню: ");

    if (db_or_txt == 1) {
        int field = getMenuChoice(u8"\nВыберите параметр для сортировки: \n-------------------------------------------------\n1) Фамилия\n\n2) Имя\n\n3) Отчество\n\n4) Год рождения\n\n5) Домашний адрес\n\n6) Место рождения\n-------------------------------------------------\nВыберете подпункт меню: ");
        if (field < 1 || field > 6) {
            cout << u8"Некорректный выбор!" << endl;
            return;
        }
        int order = getMenuChoice(u8"\nВыберите тип сортировки для дальнейшей работы: \n-------------------------------------------------\n1) По возрастанию\n\n2) По убыванию\n-------------------------------------------------\nВведите цифру подпункта меню: ");
        if (order != 1 && order != 2) {
            cout << u8"Некорректный выбор!" << endl;
            return;
        }
        string column[] = { "familiya", "imya", "otchestvo", "godrozh", "adres", "mesto" };
        string ord = (order == 1) ? "ASC" : "DESC";
        SQLiteStmt stmt(db.get(), "SELECT * FROM users ORDER BY " + column[field - 1] + " " + ord + ";");
        print(stmt.get());
        if (getMenuChoice(u8"\nВыберите функцию для дальнейшей работы: \n-------------------------------------------------\n1) Записать отсортированную базу данных в файл\n\n2) Продолжить без сохранения\n-------------------------------------------------\nВведите цифру подпункта меню: ") == 1) {
            saveToFile("sorted_" + column[field - 1] + ".txt", stmt.get(), false);
        }
    }
    else if (db_or_txt == 2) {
        string filename;
        do {
            cout << u8"Введите имя файла для сортировки: ";
            cin.ignore(10000, '\n');
            getline(cin, filename);
            if (!isValidFilename(filename)) {
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                cout << u8"Имя файла должно содержать только буквы, цифры, подчеркивание или точку!\n";
            }
        } while (!isValidFilename(filename));
        filename += ".txt";
        ifstream file(filename);
        if (!file) {
            cout << u8"Ошибка открытия файла: " << filename << endl;
            return;
        }

        vector<User> users;
        string line;
        while (getline(file, line)) {
            if (line.empty() || line.find("ID") != string::npos || line.find("---") != string::npos) continue;
            User u;
            try {
                stringstream ss(line);
                string token;
                getline(ss, token, '|'); u.id = stoi(token);
                getline(ss, token, '|'); u.familiya = trim(token);
                getline(ss, token, '|'); u.imya = trim(token);
                getline(ss, token, '|'); u.otchestvo = trim(token);
                getline(ss, token, '|'); u.godrozh = stoi(token);
                getline(ss, token, '|'); u.adres = trim(token);
                getline(ss, token, '|'); u.mesto = trim(token);
                users.push_back(u);
            }
            catch (const exception& e) {
                cerr << u8"Ошибка парсинга строки: " << line << endl;
                continue;
            }
        }
        file.close();

        if (users.empty()) {
            cout << u8"Файл пуст или содержит некорректные данные!" << endl;
            return;
        }

        int field = getMenuChoice(u8"\nВыберите параметр для сортировки: \n-------------------------------------------------\n1) Фамилия\n\n2) Имя\n\n3) Отчество\n\n4) Год рождения\n\n5) Домашний адрес\n\n6) Место рождения\n-------------------------------------------------\nВведите цифру подпункта меню: ");
        if (field < 1 || field > 6) {
            cout << u8"Некорректный выбор!" << endl;
            return;
        }
        int order = getMenuChoice(u8"\nВыберите тип сортировки: \n-------------------------------------------------\n1) По возрастанию\n\n2) По убыванию\n-------------------------------------------------\nВведите цифру подпункта меню: ");
        if (order != 1 && order != 2) {
            cout << u8"Некорректный выбор!" << endl;
            return;
        }
        sort(users.begin(), users.end(), [field, order](const User& a, const User& b) {
            return compareByField(a, b, field - 1, order == 1);
            });

        cout << u8"\nОтсортированные данные:" << endl;
        cout << left << setw(2) << u8"ID" << " | " << left << setw(19) << u8"Фамилия" << " | "
            << left << setw(13) << u8"Имя" << " | " << left << setw(23) << u8"Отчество" << " | "
            << left << setw(10) << u8"Год рождения" << " | " << left << setw(40) << u8"Адрес" << " | "
            << left << setw(15) << u8"Место" << endl;
        cout << u8"------------------------------------------------------------------------------------------------------------------------" << endl;

        for (const auto& u : users) {
            
            int lenFamiliya = 0;
            if (u.familiya != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.familiya.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenFamiliya++;
                    ptr++;
                }
            }

            int lenImya = 0;
            if (u.imya != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.imya.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenImya++;
                    ptr++;
                }
            }

            int lenOtchestvo = 0;
            if (u.otchestvo != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.otchestvo.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenOtchestvo++;
                    ptr++;
                }
            }

            int lenAdres = 0;
            if (u.adres != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.adres.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenAdres++;
                    ptr++;
                }
            }

            int lenMesto = 0;
            if (u.mesto != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.mesto.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenMesto++;
                    ptr++;
                }
            }

            // Длина года рождения в символах
            int lenGodrozh = to_string(u.godrozh).length();

            // Вывод с учётом ширины столбцов
            cout << left << setw(2) << u.id << " | "
                << (u.familiya.empty() ? "" : u.familiya) << string(12 - lenFamiliya, ' ') << " | "
                << (u.imya.empty() ? "" : u.imya) << string(10 - lenImya, ' ') << " | "
                << (u.otchestvo.empty() ? "" : u.otchestvo) << string(15 - lenOtchestvo, ' ') << " | "
                << u.godrozh << string(12 - lenGodrozh, ' ') << " | "
                << (u.adres.empty() ? "" : u.adres) << string(35 - lenAdres, ' ') << " | "
                << (u.mesto.empty() ? "" : u.mesto) << string(15 - lenMesto, ' ') << endl;
        }

        if (getMenuChoice(u8"\nВыберите функцию для дальнейшей работы: \n-------------------------------------------------\n1) Записать отсортированные данные в файл\n\n2) Продолжить без сохранения\n-------------------------------------------------\nВведите цифру подпункта меню: ") == 1) {
            string out_file, temp;
            do {
                cout << u8"Введите имя файла для сохранения: ";
                cin.ignore(10000, '\n');
                getline(cin, out_file);
                if (!isValidFilename(out_file)) {
                    cin.sync();
                    keybd_event(VK_RETURN, 0, 0, 0);
                    keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    cout << u8"мя файла должно содержать только буквы, цифры, подчеркивание или точку!\n";
                }
            } while (!isValidFilename(out_file));
            out_file += ".txt";
            ofstream outfile(out_file);
            if (!outfile) {
                cout << u8"Ошибка создания файла: " << out_file << endl;
                return;
            }
            printTableHeader(outfile);
            for (const auto& u : users) {
                printRow(outfile, u.id, u.familiya, u.imya, u.otchestvo, u.godrozh, u.adres, u.mesto);
            }
            cout << u8"Данные успешно сохранены в файл " << out_file << endl;
        }
    }
    else {
        cout << u8"Некорректный выбор!\n";
    }
}

// Обработка операций с базой данных
void work_db(int c, const string& table_name) {
    SQLiteDB db(table_name);
    string query, param, default_file;
    bool use_int = false;

    switch (c) {
    case 1:
        query = "SELECT * FROM users;";
        default_file = "all_users.txt";
        break;
    case 2:
        do {
            cout << u8"Введите название улицы: ";
            cin.ignore(10000, '\n');
            getline(cin, param);
            if (param.empty()) {
                cout << u8"Название улицы не может быть пустым!\n";
            }
        } while (param.empty());
        query = "SELECT * FROM users WHERE adres LIKE ?;";
        param = "%" + param + "%";
        default_file = "adres_sort.txt";
        last_search.street = param.substr(1, param.length() - 2);
        break;
    case 3:
        do {
            cout << u8"Введите год рождения: ";
            cin >> param;
            if (!isDigitsOnly(param) || param.length() != 4 ||
                (stoi(param) < 1900 || stoi(param) > 2025)) {
                cout << u8"Год рождения должен быть четырехзначным числом от 1900 до 2025!\n";
            }
        } while (!isDigitsOnly(param) || param.length() != 4 ||
            (stoi(param) < 1900 || stoi(param) > 2025));
        query = "SELECT * FROM users WHERE godrozh = ?;";
        default_file = "year_sort.txt";
        use_int = true;
        last_search.year = stoi(param);
        break;
    case 4:
        do {
            cout << u8"Введите город рождения: ";
            cin.ignore(10000, '\n');
            getline(cin, param);
            if (!isRussianLettersOnly(param)) {
                cout << u8"Город рождения должен содержать только буквы!\n";
            }
        } while (!isRussianLettersOnly(param));
        query = "SELECT * FROM users WHERE mesto = ?;";
        default_file = "city_sort.txt";
        last_search.city = param;
        break;
    case 5:
        sort_smth(table_name);
        return;
    case 7: {
        SQLiteStmt stmt(db.get(), "SELECT * FROM users;");
        print(stmt.get());
        string id_str;
        int id;
        do {
            cout << u8"\nВведите ID пользователя для удаления: ";
            cin >> id_str;
            if (!isDigitsOnly(id_str) || stoi(id_str) <= 0) {
                cout << u8"ID должен быть положительным числом!\n";
            }
        } while (!isDigitsOnly(id_str) || stoi(id_str) <= 0);
        id = stoi(id_str);
        deleteUserById(db.get(), id);
        return;
    }
    case 8: {
        string filename;
        do {
            cout << u8"Введите имя файла для вывода в консоль: ";
            cin.ignore(10000, '\n');
            getline(cin, filename);
            if (!isValidFilename(filename)) {
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                cout << u8"Имя файла должно содержать только буквы, цифры, подчеркивание или точку!\n";
            }
        } while (!isValidFilename(filename));
        filename += ".txt";
        ifstream file(filename);
        if (!file) {
            cout << u8"Ошибка открытия файла: " << filename << endl;
            return;
        }

        vector<User> users;
        string line;
        while (getline(file, line)) {
            if (line.empty() || line.find("ID") != string::npos || line.find("---") != string::npos) continue;
            User u;
            try {
                stringstream ss(line);
                string token;
                getline(ss, token, '|'); u.id = stoi(token);
                getline(ss, token, '|'); u.familiya = trim(token);
                getline(ss, token, '|'); u.imya = trim(token);
                getline(ss, token, '|'); u.otchestvo = trim(token);
                getline(ss, token, '|'); u.godrozh = stoi(token);
                getline(ss, token, '|'); u.adres = trim(token);
                getline(ss, token, '|'); u.mesto = trim(token);
                users.push_back(u);
            }
            catch (const exception& e) {
                cerr << u8"Ошибка парсинга строки: " << line << endl;
                continue;
            }
        }
        file.close();

        if (users.empty()) {
            cout << u8"Файл пуст или содержит некорректные данные!" << endl;
            return;
        }

        cout << u8"\nДанные из файла:" << endl;
        cout << left << setw(2) << u8"ID" << " | " << left << setw(27) << u8"Фамилия" << " | "
            << left << setw(13) << u8"Имя" << " | " << left << setw(23) << u8"Отчество" << " | "
            << left << setw(10) << u8"Год рождения" << " | " << left << setw(33) << u8"Адрес" << " | "
            << left << setw(15) << u8"Место" << endl;
        cout << u8"------------------------------------------------------------------------------------------------------------------------" << endl;


        for (const auto& u : users) {

            int lenFamiliya = 0;
            if (u.familiya != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.familiya.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenFamiliya++;
                    ptr++;
                }
            }

            int lenImya = 0;
            if (u.imya != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.imya.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenImya++;
                    ptr++;
                }
            }

            int lenOtchestvo = 0;
            if (u.otchestvo != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.otchestvo.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenOtchestvo++;
                    ptr++;
                }
            }

            int lenAdres = 0;
            if (u.adres != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.adres.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenAdres++;
                    ptr++;
                }
            }

            int lenMesto = 0;
            if (u.mesto != "") {
                const unsigned char* ptr = reinterpret_cast<const unsigned char*>(u.mesto.c_str());
                while (*ptr) {
                    if ((*ptr & 0xC0) != 0x80) lenMesto++;
                    ptr++;
                }
            }

            // Длина года рождения в символах
            int lenGodrozh = to_string(u.godrozh).length();

            // Вывод с учётом ширины столбцов
            cout << left << setw(2) << u.id << " | "
                << (u.familiya.empty() ? "" : u.familiya) << string(20 - lenFamiliya, ' ') << " | "
                << (u.imya.empty() ? "" : u.imya) << string(10 - lenImya, ' ') << " | "
                << (u.otchestvo.empty() ? "" : u.otchestvo) << string(15 - lenOtchestvo, ' ') << " | "
                << u.godrozh << string(12 - lenGodrozh, ' ') << " | "
                << (u.adres.empty() ? "" : u.adres) << string(28 - lenAdres, ' ') << " | "
                << (u.mesto.empty() ? "" : u.mesto) << string(15 - lenMesto, ' ') << endl;
        }
        return;
    }
    default:
        cout << u8"Неверный выбор." << endl;
        return;
    }

    SQLiteStmt stmt(db.get(), query);

    if (c == 2 || c == 4) {
        sqlite3_bind_text(stmt.get(), 1, param.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
            cout << u8"\nНе найдены данные, удовлетворяющие введенному критерию!";
            return;
        }
    }
    else if (c == 3) {
        sqlite3_bind_int(stmt.get(), 1, stoi(param));
    }
    print(stmt.get()); 

    if (c != 1) {
        if (getMenuChoice(u8"\nВыберите функцию для дальнейшей работы: \n-------------------------------------------------\n1) Записать базу данных по найденному параметру в файл\n\n2) Продолжить работу с базой данных без сохранения\n-------------------------------------------------\nВведите цифру подпункта меню: ") == 1) {
            SQLiteStmt new_stmt(db.get(), query);
            if (c == 2 || c == 4) {
                sqlite3_bind_text(new_stmt.get(), 1, param.c_str(), -1, SQLITE_STATIC);
            }
            else if (c == 3) {
                sqlite3_bind_int(new_stmt.get(), 1, stoi(param));
            }
            saveToFile(default_file, new_stmt.get(), false);
        }
    }
}

// Создание или дополнение базы данных
void create_db(const string& name, bool append) {
    SQLiteDB db(name);
    const char* createTableSQL = "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, familiya TEXT NOT NULL, "
        "imya TEXT NOT NULL, otchestvo TEXT NOT NULL, godrozh INTEGER NOT NULL, "
        "adres TEXT NOT NULL, mesto TEXT NOT NULL);";
    if (sqlite3_exec(db.get(), createTableSQL, nullptr, nullptr, nullptr) != SQLITE_OK) {
        cerr << u8"Ошибка создания таблицы: " << sqlite3_errmsg(db.get()) << endl;
        return;
    }

    int count = getMenuChoice(u8"Укажите кол-во вводимых избирателей: ");
    for (int i = 0; i < count; ++i) {
        string familiya, imya, otchestvo, prev_adres, mesto, godrozh_str, adres = "";
        int godrozh;

        // Ввод фамилии
        do {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << u8"Введите фамилию: ";
            getline(cin, familiya); 
            if (!isCorrectSecondname(familiya) or !firstTrue(familiya)) {
                cout << u8"Фамилия должна содержать только русские буквы или дефис и начинаться с заглавной буквы!";
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            }
            else {
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                break;
            }
        } while (true);

        // Ввод имени
        do {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << u8"Введите имя: ";
            getline(cin, imya);
            if (!isRussianLettersOnly(imya) or !firstTrue(imya)) {
                cout << u8"Имя должно содержать только русские буквы и начинаться с заглавной буквы!";
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            }
            else {
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                break;
            }
        } while (true);

        // Ввод отчества
        do {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << u8"Введите отчество: ";
            getline(cin, otchestvo);
            if (!isRussianLettersOnly(otchestvo) or !firstTrue(otchestvo)) {
                cout << u8"Отчество должно содержать только русские буквы и начинаться с заглавной буквы!";
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            }
            else {
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                cin.ignore(100, '\n');
                break;
            }
        } while (true);

        // Ввод года рождения
        do {
            cout << u8"Введите год рождения: ";
            cin.clear();
            getline(cin, godrozh_str);
            try {
                godrozh = stoi(godrozh_str);
                if (godrozh < 1930 || godrozh > 2007) {
                    cout << u8"Год рождения должен быть от 1930 до 2007!\n";
                    continue;
                }
                break;
            }
            catch (const exception& e) {
                cout << u8"Введите корректное число!\n";
            }
        } while (true);
        cout << endl;
        // Ввод адреса
        do {
            locale loc("ru_RU.UTF-8");
            cout << u8"Введите название улицы, номер дома и номер квартиры через пробел(пример: Ленина 64 5): ";
            cin.clear();
            cin.sync();
            getline(cin, prev_adres);
            if (!(firstTrue(prev_adres))) {
                cout << u8"Неверный формат адреса! Проверьте заглавную букву в названии улицы " << endl;
            }
            else {
                string ulitsa;
                int dom = 0, kv = 0;
                istringstream iss(prev_adres);
                iss >> ulitsa >> dom >> kv;
                if (!(kv == 0) or !(dom == 0)) {
                    keybd_event(VK_RETURN, 0, 0, 0);
                    keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    adres = u8"Ул. " + ulitsa + u8", д. " + to_string(dom) + u8", кв. " + to_string(kv);
                    break;
                }
                else {
                    cout << u8"Неверный формат адреса! Введите заново в формате улица, номер дома и номер квартиры" << endl;
                }
            }
        } while (true);

        // Ввод места рождения
        do {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << u8"Введите место рождения: ";
            getline(cin, mesto);
            if (!isRussianLettersOnly(mesto) or !firstTrue(mesto)) {
                cout << u8"Место рождения должно содержать только русские буквы или пробел и начинаться с заглавной буквы!";
                cin.sync();
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
            }
            else {
                keybd_event(VK_RETURN, 0, 0, 0);
                keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                break;
            }
        } while (true);

        SQLiteStmt stmt(db.get(), "INSERT INTO users (familiya, imya, otchestvo, godrozh, adres, mesto) VALUES (?, ?, ?, ?, ?, ?);");
        sqlite3_bind_text(stmt.get(), 1, familiya.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.get(), 2, imya.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.get(), 3, otchestvo.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt.get(), 4, godrozh);
        sqlite3_bind_text(stmt.get(), 5, adres.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt.get(), 6, mesto.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            cerr << u8"Ошибка добавления данных: " << sqlite3_errmsg(db.get()) << endl;
            continue;
        }
        cout << u8"Данные успешно добавлены в базу данных." << endl;

        int new_id = static_cast<int>(sqlite3_last_insert_rowid(db.get()));

        // Добавление в файл для года
        if (last_search.year != -1 && godrozh == last_search.year) {
            string target_file = last_search.last_year_file.empty() ? "year_sort.txt" : last_search.last_year_file;
            writeUserToFile(target_file, new_id, familiya, imya, otchestvo, godrozh, adres, mesto, true);
           
        }
        // Добавление в файл для улицы
        if (!last_search.street.empty() && adres.find(last_search.street) != string::npos) {
            string target_file = last_search.last_street_file.empty() ? "adres_sort.txt" : last_search.last_street_file;
            writeUserToFile(target_file, new_id, familiya, imya, otchestvo, godrozh, adres, mesto, true);
            
        }
        // Добавление в файл для города
        if (!last_search.city.empty() && mesto == last_search.city) {
            string target_file = last_search.last_city_file.empty() ? "city_sort.txt" : last_search.last_city_file;
            writeUserToFile(target_file, new_id, familiya, imya, otchestvo, godrozh, adres, mesto, true);
            
        }

        string sort_files[] = { "sorted_familiya.txt", "sorted_imya.txt", "sorted_otchestvo.txt",
                               "sorted_godrozh.txt", "sorted_adres.txt", "sorted_mesto.txt" };
        for (const auto& file : sort_files) {
            ifstream check(file);
            if (check.good()) {
                writeUserToFile(file, new_id, familiya, imya, otchestvo, godrozh, adres, mesto, true);
            }
            check.close();
        }

        if (append && last_search.year == -1 && last_search.city.empty() && last_search.street.empty()) {
            SQLiteStmt stmt_addr(db.get(), "SELECT * FROM users WHERE adres LIKE ?;");
            sqlite3_bind_text(stmt_addr.get(), 1, ("%" + adres + "%").c_str(), -1, SQLITE_STATIC);
            write("adres_sort.txt", stmt_addr.get(), true);

            SQLiteStmt stmt_year(db.get(), "SELECT * FROM users WHERE godrozh = ?;");
            sqlite3_bind_int(stmt_year.get(), 1, godrozh);
            write("year_sort.txt", stmt_year.get(), true);

            SQLiteStmt stmt_city(db.get(), "SELECT * FROM users WHERE mesto = ?;");
            sqlite3_bind_text(stmt_city.get(), 1, mesto.c_str(), -1, SQLITE_STATIC);
            write("city_sort.txt", stmt_city.get(), true);
        }
    }
}

// Работа с существующей базой данных
void later_db(const string& table_name) {
    if (table_name == "list_voiters1.db") {
        cout << u8"\n\n\t\t\t\tВыбрана функция работы с предустановленной базой данных" << endl;
    }
    else {
        cout << u8"\n\n\t\t\tВыбрана функция работы с созданной базой данных" << endl;
    }
    while (true) {
        int choice = getMenuChoice(u8"\n\n\nВыберите функцию для дальнейшей работы: \n-------------------------------------------------\n1) Вывести базу данных в консоль\n\n2) Найти по улице, на которой проживает избиратель\n\n3) Найти по году рождения избирателя\n\n4) Найти по городу рождения избирателя\n\n5) Отсортировать базу данных или файл\n\n6) Дополнить базу данных\n\n7) Удалить пользователя по ID\n\n8) Вывести содержимое файла из директории\n\n9) Назад\n-------------------------------------------------\nВведите цифру подпункта меню: ");
        if (choice == 9) {
            cout << "\n\n";
            return;
        }
        if (choice == 6) create_db(table_name, true);
        else work_db(choice, table_name);
    }
}

// Главная функция программы
int main() {
    setlocale(LC_ALL, "ru_RU.UTF-8");
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    cout << u8"\t\t\t\tОзнакомительная практика Рыжов Степан УИБ-111 :)\n" << endl;
    while (true) {
        int choice = getMenuChoice(u8"\t\t\t\t\t\tГлавное меню\n\nВыберите функцию для дальнейшей работы: \n-------------------------------------------------\n1) Использовать существующую базу данных\n\n2) Создать новую базу данных\n\n3) Выход из программы\n-------------------------------------------------\nВведите цифру подпункта меню: ");
        string db_name;
        switch (choice) {
        case 1:
            do {
                cout << u8"Введите имя базы данных (по умолчанию вводите list_voiters1): ";
                cin.ignore(10000, '\n');
                cin.sync();
                getline(cin, db_name);
                if (!isValidFilename(db_name)) {
                    cin.sync();
                    keybd_event(VK_RETURN, 0, 0, 0);
                    keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    cout << u8"Имя базы данных должно содержать только буквы, цифры, подчеркивание или точку!\n";
                }
            } while (!isValidFilename(db_name));
            later_db(db_name + ".db");
            break;
        case 2:
            do {
                cin.clear();
                cin.ignore(1000, '\n');
                cout << u8"Введите имя базы данных для создания: ";
                getline(cin, db_name);
                if (!isValidFilename(db_name)) {
                    cout << u8"Имя базы данных должно содержать только буквы, цифры, подчеркивание или точку!";
                    cin.sync();
                    keybd_event(VK_RETURN, 0, 0, 0);
                    keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                }
                else {
                    break;
                }
            } while (true);
            create_db(db_name + ".db", false);
            later_db(db_name + ".db");
            break;
        case 3:
            return 0;
        default:
            cout << u8"Неверно введенная функция!\n\n";
            break;
        }
    }
    return 0;
}

/* Программа разработана студентом группы УИБ-111 Рыжовым Степаном, с использованием базы данных SQL и консольного интерфейса
Использованные библиотеки: 
<iostream>
<iomanip>
<fstream>
<string>
<vector>
<algorithm>
<sstream>
<Windows.h>
*/