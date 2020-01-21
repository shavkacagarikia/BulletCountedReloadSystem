#include <map>
#include <fstream>
#include <iostream>
#include <string>
#include <fstream>  
#include <vector>  
#include <iterator>
#include "f4se\GameData.h"
#include "Utils.h"

typedef std::pair<std::string, std::string> entry;


namespace std {
	std::istream &operator >> (std::istream &is, entry &d) {
		std::getline(is, d.first, '=');
		std::getline(is, d.second);
		return is;
	}
}


void Getinis(std::vector<WIN32_FIND_DATA>* arr) {
	char* modSettingsDirectory = "Data\\BCR\\*.ini";

	HANDLE hFind;
	WIN32_FIND_DATA data;
	hFind = FindFirstFile(modSettingsDirectory, &data);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			arr->push_back(data);
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
}

void HandleIniFiles() {
	std::vector<WIN32_FIND_DATA> modSettingFiles;
	Getinis(&modSettingFiles);
	for (size_t i = 0; i < modSettingFiles.size(); i++)
	{
		std::string fileName = modSettingFiles[i].cFileName;
		std::string fileNameNoSuffix = fileName.substr(0, fileName.size() - 4);

		auto isModEnabled = (*g_dataHandler)->LookupLoadedModByName(fileNameNoSuffix.c_str());
		if (isModEnabled) {

			std::string ff = "Data\\BCR\\" + fileName;

			std::ifstream in(ff);

			std::map<std::string, std::string> dict((std::istream_iterator<entry>(in)),
				std::istream_iterator<entry>());


			for (entry const &e : dict) {
				if (e.first.find("weaponFormId") != std::string::npos) {

					std::string s1 = e.second;
					std::string combined = fileNameNoSuffix + "|" + e.second;
					TESForm* form = GetFormFromIdentifier(combined);
					if (form) {
						TESObjectWEAP* weapForm = (TESObjectWEAP*)form;
						if (weapForm) {
							weapForm->weapData.skill = checkAvif;
						}
					}


				}

			}
		}

	}
}