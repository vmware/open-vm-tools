/*
 *	 Author: bwilliams
 *  Created: Jul 2009
 *
 *	Copyright (C) 2009-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#ifndef CCmdLineOptions_h_
#define CCmdLineOptions_h_

namespace Caf {

class COMMONAGGREGATOR_LINKAGE CCmdLineOptions {
private:
	typedef std::map<std::string, gchar*> CStringOptions;
	typedef std::map<std::string, gint> CIntOptions;
	typedef std::map<std::string, gboolean> CBoolOptions;

public:
	CCmdLineOptions();
	virtual ~CCmdLineOptions();

public:
	void initialize(const std::string& cmdDescription, const uint32 maxOptions);
	void parse(int32 argc, char* argv[]);

	void addStringOption(const std::string& longName, const char shortName, const std::string& optionDescription);
	void addIntOption(const std::string& longName, const char shortName, const std::string& optionDescription);
	void addBoolOption(const std::string& longName, const char shortName, const std::string& optionDescription);

	std::string findStringOption(const std::string& longName);
	int32 findIntOption(const std::string& longName);
	bool findBoolOption(const std::string& longName);

private:
	bool _isInitialized;
	uint32 _optionCnt;
	std::string _cmdDescription;

	uint32 _maxOptions;
	GOptionEntry* _gOptions;

	CStringOptions _stringOptions;
	CIntOptions _intOptions;
	CBoolOptions _boolOptions;

	// Temporary for storing longNames and optionDescription for addFooOption() methods above.
	std::vector<std::string> _longNames, _optionDescriptions;

private:
	void checkOptionCnt(
		const std::string& longName,
		const uint32 optionCnt,
		const uint32 maxOptions) const;
	void populateOption(
		GOptionEntry& optionEntry,
		const std::string& longName,
		const char shortName,
		const std::string& optionDescription);

private:
	CAF_CM_CREATE;
	CAF_CM_CREATE_LOG;
	CAF_CM_DECLARE_NOCOPY(CCmdLineOptions);
};

CAF_DECLARE_SMART_POINTER( CCmdLineOptions);

}

#endif // #ifndef CCmdLineOptions_h_
