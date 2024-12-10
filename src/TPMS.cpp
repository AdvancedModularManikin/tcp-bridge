#include "TPMS.h"


TPMS::TPMS() {}

TPMS::~TPMS() {
	// Clean up dynamically allocated Manikins
	for (auto& pair : manikins) {
		delete pair.second;
	}
}

void TPMS::SetID(const std::string& id) {
	myID = id;
}

void TPMS::SetMode(bool podMode) {
	mode = podMode;
}

void TPMS::InitializeManikin(const std::string& manikinId) {
	// Check if the manikin already exists
	if (manikins.find(manikinId) == manikins.end()) {
		manikins[manikinId] = new Manikin(manikinId, mode, myID);
	}
}

void TPMS::InitializeManikins(int count) {
	for (int i = 1; i <= count; ++i) {
		InitializeManikin("manikin_" + std::to_string(i));
	}
}

Manikin* TPMS::GetManikin(const std::string& manikinId) {
	auto it = manikins.find(manikinId);
	if (it != manikins.end()) {
		return it->second;
	}
	return nullptr; // Return nullptr if the manikin ID is not found
}