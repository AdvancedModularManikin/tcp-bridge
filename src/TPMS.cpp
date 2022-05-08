#include "TPMS.h"

TPMS::TPMS() {

}

TPMS::~TPMS() {

}

void TPMS::SetMode(bool podMode) {
    mode = podMode;
}

void TPMS::InitializeManikins(int count) {
    switch (count) {
        case 4:
            mgr4 = new Manikin("manikin_4", mode);
        case 3:
            mgr3 = new Manikin("manikin_3", mode);
        case 2:
            mgr2 = new Manikin("manikin_2", mode);
        case 1:
            mgr1 = new Manikin("manikin_1", mode);
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

