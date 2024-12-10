#pragma once

#include "Manikin.h"
#include <map>
#include <string>

class TPMS {
public:
	TPMS();
	~TPMS();

	// Setters
	void SetID(const std::string& id);
	void SetMode(bool podMode);

	// Manikin initialization
	void InitializeManikins(int count);
	void InitializeManikin(const std::string& manikinId);

	// Accessor
	Manikin* GetManikin(const std::string& manikinId);

private:
	// Member variables
	std::string myID;                 // ID of the TPMS
	bool mode{};                        // Mode (podMode)

	// Manikin management
	std::map<std::string, Manikin*> manikins; // Map to store manikins dynamically by ID
};