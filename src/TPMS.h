#ifndef TPMS_H
#define TPMS_H

#include "Manikin.h"
#include <string>
#include <map>
#include <memory>
#include <mutex>

class TPMS {
public:
	TPMS();
	~TPMS();

	void SetID(const std::string& id);
	void SetMode(bool podMode);
	void InitializeManikin(const std::string& manikinId);
	void InitializeManikins(int count);
	Manikin* GetManikin(const std::string& manikinId);

private:
	std::string myID;
	bool mode = false;
	std::map<std::string, Manikin*> manikins;
	std::mutex manikinsMutex;
};

#endif // TPMS_H