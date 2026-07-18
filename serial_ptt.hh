#pragma once

#include <string>
#include <cstring>
#include <windows.h>

enum class PTTLine {
    DTR = 0,
    RTS = 1,
    BOTH = 2
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

        port_ = port;
        line_ = line;
        invert_dtr_ = invert_dtr;
        invert_rts_ = invert_rts;

        // \\.\ prefix required for COM10 and above
        std::string path = port.rfind("\\\\.\\", 0) == 0 ? port : "\\\\.\\" + port;
        handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            last_error_ = std::string("Failed to open ") + port + ": error " +
                          std::to_string(GetLastError());
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

    const std::string& last_error() const { return last_error_; }

    bool ptt_on() {
        return apply(true);
    }

    bool ptt_off() {
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

        return ok;
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool open_ = false;
    std::string port_;
    PTTLine line_ = PTTLine::RTS;
    bool invert_dtr_ = false;
    bool invert_rts_ = false;
    std::string last_error_;
};
