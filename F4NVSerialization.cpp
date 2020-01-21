#include <fstream>

#include "F4NVSerialization.h"
#include "f4se/PluginAPI.h"
#include "f4se\GameObjects.h"
#include "f4se\GameReferences.h"


namespace F4NVSerialization
{

	void RevertCallback(const F4SESerializationInterface * intfc)
	{
		_DMESSAGE("Clearing F4NV serialization data.");
	}

	void LoadCallback(const F4SESerializationInterface * intfc)
	{
		_DMESSAGE("Loading F4NV serialization data.");
		UInt32 type, version, length;
		while (intfc->GetNextRecordInfo(&type, &version, &length))
		{
			_DMESSAGE("type %i", type);
			switch (type)
			{
			case 'F4NV':
				_DMESSAGE("Loading F4NV type");
				Load(intfc, InternalEventVersion::kCurrentVersion);
				break;
			}
		}

	}

	bool Load(const F4SESerializationInterface * intfc, UInt32 version)
	{



		return true;
	}

	void SaveCallback(const F4SESerializationInterface * intfc)
	{
		_DMESSAGE("Save F4NV serialization data.");
		Save(intfc, 'F4NV', InternalEventVersion::kCurrentVersion);
	}

	bool Save(const F4SESerializationInterface * intfc, UInt32 type, UInt32 version)
	{
		intfc->OpenRecord(type, version);




		/*if (!intfc->WriteRecordData(&GoodAndBadFactionsSaveArray, sizeof(GoodAndBadFactionsSaveArray))) {
		_MESSAGE("Error write GoodAndBadFactionsSaveArray parameter");
		}*/
		return true;
	}



}