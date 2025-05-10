#include "TPMS.h"

TPMS::TPMS() {}

TPMS::~TPMS() {
	// Use a lock to ensure thread safety during cleanup
	std::lock_guard<std::mutex> lock(manikinsMutex);

	// Clean up dynamically allocated Manikins
	for (auto& pair : manikins) {
		delete pair.second;
	}
	manikins.clear();
}

void TPMS::SetID(const std::string& id) {
	myID = id;
}

void TPMS::SetMode(bool podMode) {
	mode = podMode;
}

void TPMS::InitializeManikin(const std::string& manikinId) {
	std::lock_guard<std::mutex> lock(manikinsMutex);

	// Check if the manikin already exists
	auto it = manikins.find(manikinId);
	if (it == manikins.end()) {
		try {
			Manikin* manikin = new Manikin(manikinId, mode, myID);
			manikins[manikinId] = manikin;
			// Log successful creation
			LOG_INFO << "Created new manikin with ID: " << manikinId;
		} catch (const std::exception& e) {
			LOG_ERROR << "Failed to create manikin with ID " << manikinId << ": " << e.what();
		}
	} else {
		LOG_DEBUG << "Manikin with ID " << manikinId << " already exists, skipping initialization";
	}
}

void TPMS::InitializeManikins(int count) {
	if (count <= 0) {
		LOG_WARNING << "Requested to initialize " << count << " manikins, but count must be positive";
		return;
	}

	LOG_INFO << "Initializing " << count << " manikins";
	for (int i = 1; i <= count; ++i) {
		std::string manikinId = "manikin_" + std::to_string(i);
		InitializeManikin(manikinId);
	}
}

Manikin* TPMS::GetManikin(const std::string& manikinId) {
	std::lock_guard<std::mutex> lock(manikinsMutex);

	auto it = manikins.find(manikinId);
	if (it != manikins.end()) {
		return it->second;
	}

	LOG_WARNING << "Attempted to get non-existent manikin with ID: " << manikinId;
	return nullptr; // Return nullptr if the manikin ID is not found
}