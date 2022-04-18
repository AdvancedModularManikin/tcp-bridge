#pragma once

#include "Manikin.h"

#include "bridge.h"

class TPMS {

public:
    TPMS();

    ~TPMS();

    void InitializeManikins();

    Manikin* GetManikin(std::string mid);

    Manikin* mgr1;
    Manikin* mgr2;
    Manikin* mgr3;
    Manikin* mgr4;

};

