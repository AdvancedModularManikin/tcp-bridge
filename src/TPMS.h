#pragma once

#include "Manikin.h"

#include "bridge.h"

class TPMS {

public:
    TPMS();

    ~TPMS();

    void SetID(std::string id);
    void SetMode(bool podMode);
    void InitializeManikins(int count);
    void InitializeManikin(std::string manikinId);

    Manikin* GetManikin(std::string mid);

    std::string myID;

    Manikin* mgr1;
    Manikin* mgr2;
    Manikin* mgr3;
    Manikin* mgr4;

    bool mode;

};

