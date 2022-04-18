#include "TPMS.h"

TPMS::TPMS() {

}

TPMS::~TPMS() {

}

void TPMS::InitializeManikins() {
    mgr1 = new Manikin("manikin_1");
    mgr2 = new Manikin("manikin_2");
    //mgr3 = new Manikin("manikin_3");
    //mgr4 = new Manikin("manikin_4");
}

Manikin* TPMS::GetManikin(std::string mid) {
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

