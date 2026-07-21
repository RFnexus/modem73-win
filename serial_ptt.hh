#pragma once

#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <setupapi.h>

enum class PTTLine {
    DTR = 0,
    RTS = 1,
    BOTH = 2
};

struct SerialPortInfo {
    std::string port;
    std::string description;
};

class SerialPTT {
public:
    SerialPTT() = default;

    ~SerialPTT() {
        close();
    }



    bool open(const std::string& port, PTTLine line = PTTLine::RTS,
              bool invert_dtr = false, bool invert_rts = false) {
        close();

        port_ = normalize_port(port);
        line_ = line;
        invert_dtr_ = invert_dtr;
        invert_rts_ = invert_rts;

        // \\.\ prefix required for COM10 and above
        std::string path = port_.rfind("\\\\.\\", 0) == 0 ? port_ : "\\\\.\\" + port_;
        handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            last_error_ = std::string("Failed to open ") + port_ + ": " +
                          describe_error(GetLastError());
            return false;
        }

        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        if (GetCommState(handle_, &dcb)) {
            dcb.fDtrControl = DTR_CONTROL_DISABLE;
            dcb.fRtsControl = RTS_CONTROL_DISABLE;
            SetCommState(handle_, &dcb);
        }




        ptt_off();




        last_error_.clear();
        open_ = true;
        return true;
    }


    void close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            ptt_off();
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
        open_ = false;
    }


    bool is_open() const { return open_; }

    const std::string& port() const { return port_; }

    const std::string& last_error() const { return last_error_; }

    bool ptt_on() {
        return apply(true);
    }

    bool ptt_off() {
        if (handle_ == INVALID_HANDLE_VALUE) return true;
        return apply(false);
    }

    bool reconnect() {
        std::string saved_port = port_;
        PTTLine saved_line = line_;
        bool saved_invert_dtr = invert_dtr_;
        bool saved_invert_rts = invert_rts_;

        close();
        return open(saved_port, saved_line, saved_invert_dtr, saved_invert_rts);
    }

    static std::vector<SerialPortInfo> enumerate() {
        std::vector<SerialPortInfo> ports;

        static const GUID ports_class_guid =
            {0x4d36e978, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

        HDEVINFO devs = SetupDiGetClassDevsA(&ports_class_guid, nullptr, nullptr, DIGCF_PRESENT);
        if (devs == INVALID_HANDLE_VALUE) return ports;

        SP_DEVINFO_DATA info;
        info.cbSize = sizeof(info);
        for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &info); i++) {
            char name[64] = {0};
            HKEY key = SetupDiOpenDevRegKey(devs, &info, DICS_FLAG_GLOBAL, 0,
                                            DIREG_DEV, KEY_READ);
            if (key == INVALID_HANDLE_VALUE) continue;
            DWORD len = sizeof(name) - 1;
            LONG rc = RegQueryValueExA(key, "PortName", nullptr, nullptr,
                                       reinterpret_cast<BYTE*>(name), &len);
            RegCloseKey(key);
            if (rc != ERROR_SUCCESS || strncmp(name, "COM", 3) != 0) continue;

            char friendly[256] = {0};
            SetupDiGetDeviceRegistryPropertyA(devs, &info, SPDRP_FRIENDLYNAME, nullptr,
                                              reinterpret_cast<BYTE*>(friendly),
                                              sizeof(friendly) - 1, nullptr);
            ports.push_back({name, friendly});
        }
        SetupDiDestroyDeviceInfoList(devs);

        std::sort(ports.begin(), ports.end(),
                  [](const SerialPortInfo& a, const SerialPortInfo& b) {
                      return atoi(a.port.c_str() + 3) < atoi(b.port.c_str() + 3);
                  });
        return ports;
    }


private:
    bool apply(bool on) {
        if (handle_ == INVALID_HANDLE_VALUE) return false;
        bool ok = true;

        if (line_ == PTTLine::DTR || line_ == PTTLine::BOTH) {
            bool assert_line = on != invert_dtr_;
            ok = EscapeCommFunction(handle_, assert_line ? SETDTR : CLRDTR) && ok;
        }

        if (line_ == PTTLine::RTS || line_ == PTTLine::BOTH) {
            bool assert_line = on != invert_rts_;
            ok = EscapeCommFunction(handle_, assert_line ? SETRTS : CLRRTS) && ok;
        }

        if (!ok)
            last_error_ = port_ + ": EscapeCommFunction failed: " +
                          describe_error(GetLastError());

        return ok;
    }

    static std::string normalize_port(const std::string& port) {
        if (!port.empty() &&
            port.find_first_not_of("0123456789") == std::string::npos)
            return "COM" + port;
        std::string p = port;
        if (p.rfind("com", 0) == 0) {
            p[0] = 'C';
            p[1] = 'O';
            p[2] = 'M';
        }
        return p;
    }

    static std::string describe_error(DWORD err) {
        switch (err) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return "error 2 (no such COM port)";
        case ERROR_ACCESS_DENIED:
            return "error 5 (access denied - port in use by another program?)";
        case ERROR_SEM_TIMEOUT:
            return "error 121 (device not responding)";
        default:
            return "error " + std::to_string(err);
        }
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool open_ = false;
    std::string port_;
    PTTLine line_ = PTTLine::RTS;
    bool invert_dtr_ = false;
    bool invert_rts_ = false;
    std::string last_error_;
};
