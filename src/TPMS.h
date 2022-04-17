#pragma once

#include "Manikin.h"

#include "bridge.h"

class TPMS {

public:
    TPMS();

    ~TPMS();

    void InitializeManikins();

    Manikin* mgr1;
    Manikin* mgr2;

};

