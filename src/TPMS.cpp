#include "TPMS.h"

TPMS::TPMS() {

}

TPMS::~TPMS() {

}

void TPMS::InitializeManikins() {
    mgr1 = new Manikin("manikin_1");
    mgr2 = new Manikin("manikin_2");
}