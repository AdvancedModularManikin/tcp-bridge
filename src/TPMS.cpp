#include "TPMS.h"

TPMS::TPMS() {

}

TPMS::~TPMS() {

}

void TPMS::SetMode(bool podMode) {
    mode = podMode;
}

void TPMS::InitializeManikin(std::string manikinId) {
    if (manikinId == "manikin_1") {
        mgr1 = new Manikin("manikin_1", mode);
    } else if (manikinId == "manikin_2") {
        mgr2 = new Manikin("manikin_2", mode);
    } else if (manikinId == "manikin_3") {
        mgr3 = new Manikin("manikin_3", mode);
    } else if (manikinId == "manikin_4") {
        mgr4 = new Manikin("manikin_4", mode);
    }

}

void TPMS::InitializeManikins(int count) {
    switch (count) {
        case 4:
            InitializeManikin("manikin_4");
        case 3:
            InitializeManikin("manikin_3");
        case 2:
            InitializeManikin("manikin_2");
        case 1:
            InitializeManikin("manikin_1");
        default:
            break;
    }
}

Manikin *TPMS::GetManikin(std::string mid) {
    if (mid == "manikin_1") {
        return mgr1;
    } else if (mid == "manikin_2") {
        return mgr2;
    } else if (mid == "manikin_3") {
        return mgr3;
    } else if (mid == "manikin_4") {
        return mgr4;
    }
}

